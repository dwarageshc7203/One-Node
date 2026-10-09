#include "filetransfer.h"
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QDataStream>
#include <QSettings>
#include <QRandomGenerator>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

FileTransfer::FileTransfer(QObject *parent)
    : QObject(parent), socket(new QTcpSocket(this)),
    fileSize(0), totalWritten(0), transferActive(false), transferCompleted(false), peerPort(45679)
{
    connect(socket, &QTcpSocket::connected,
            this, &FileTransfer::onConnected);
    connect(socket, &QTcpSocket::readyRead,
            this, &FileTransfer::onReadyRead);
    connect(socket, &QTcpSocket::bytesWritten,
            this, &FileTransfer::onBytesWritten);
    connect(socket, &QAbstractSocket::errorOccurred,
            this, &FileTransfer::onError);
    connect(socket, &QTcpSocket::disconnected,
            this, &FileTransfer::processNext);
}

void FileTransfer::sendFile(const QString &path, const QString &peerIp, int port, const QString &token) {
    fileQueue.enqueue(path);
    this->peerIp = peerIp;
    this->peerPort = port;
    this->token = token;

    if (!transferActive) {
        processNext();
    }
}

void FileTransfer::processNext() {
    if (fileQueue.isEmpty()) {
        transferActive = false;
        return;
    }

    filePath = fileQueue.dequeue();
    fileName = QFileInfo(filePath).fileName();
    totalWritten = 0;
    transferActive = true;
    transferCompleted = false;

    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    QFile f(filePath);
    if (!f.exists()) {
        emit transferFailed("File not found: " + fileName);
        processNext(); // continue with next
        return;
    }
    fileSize = f.size();
    socket->connectToHost(QHostAddress(peerIp), peerPort);
}

void FileTransfer::onConnected() {
    qDebug() << "Connected to peer, starting handshake";
    hState = WaitServerGreeting;
    hBuffer.clear();
}

void FileTransfer::onReadyRead() {
    if (hState == Completed) return;

    hBuffer.append(socket->readAll());

    QSettings settings("One Node", "One Node");
    QByteArray secret = QByteArray::fromBase64(settings.value("pairing_secret").toString().toUtf8());
    QByteArray myId = settings.value("server_device_id").toString().toUtf8();
    QByteArray peerId = settings.value("device_id").toString().toUtf8();

    if (hState == WaitServerGreeting) {
        if (hBuffer.size() < 33) return;
        if (hBuffer[0] != 0x01) { socket->disconnectFromHost(); return; }
        
        QByteArray serverNonce = hBuffer.mid(1, 32);
        hBuffer.remove(0, 33);

        clientNonce.resize(32);
        for (int i = 0; i < 32; ++i) {
            clientNonce[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
        }

        QByteArray msgC = "ONv1-C" + serverNonce + myId + QByteArray("\x00", 1) + peerId;
        QMessageAuthenticationCode macC(QCryptographicHash::Sha256);
        macC.setKey(secret);
        macC.addData(msgC);
        QByteArray hmacC = macC.result();

        QByteArray resp;
        resp.append(clientNonce);
        resp.append(hmacC);
        quint16 idLen = myId.size();
        resp.append((idLen >> 8) & 0xFF);
        resp.append(idLen & 0xFF);
        resp.append(myId);
        socket->write(resp);
        socket->flush();

        hState = WaitServerMac;
    }

    if (hState == WaitServerMac) {
        if (hBuffer.size() < 32) return;
        
        QByteArray hmacS = hBuffer.left(32);
        hBuffer.remove(0, 32);

        QByteArray msgS = "ONv1-S" + clientNonce + peerId + QByteArray("\x00", 1) + myId;
        QMessageAuthenticationCode macS(QCryptographicHash::Sha256);
        macS.setKey(secret);
        macS.addData(msgS);
        QByteArray expectedHmacS = macS.result();

        int diff = 0;
        for (int i = 0; i < 32; ++i) diff |= (hmacS[i] ^ expectedHmacS[i]);
        if (diff != 0) { socket->disconnectFromHost(); return; }

        hState = Completed;
        // Handshake OK, now start transfer
        startSendingFile();
    }
}

void FileTransfer::startSendingFile() {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open file!";
        transferActive = false;
        emit transferFailed("Cannot open file: " + fileName);
        return;
    }

    emit transferStarted(fileName);

    QByteArray nameBytes = fileName.toUtf8();
    qint32 nameLen = (qint32)nameBytes.size();
    qint64 fSize   = (qint64)fileSize;

    QByteArray header;
    
    QByteArray nameHeader(4, 0);
    nameHeader[0] = (nameLen >> 24) & 0xFF;
    nameHeader[1] = (nameLen >> 16) & 0xFF;
    nameHeader[2] = (nameLen >>  8) & 0xFF;
    nameHeader[3] = (nameLen      ) & 0xFF;
    header.append(nameHeader);
    header.append(nameBytes);

    QByteArray sizeBytes(8, 0);
    sizeBytes[0] = (fSize >> 56) & 0xFF;
    sizeBytes[1] = (fSize >> 48) & 0xFF;
    sizeBytes[2] = (fSize >> 40) & 0xFF;
    sizeBytes[3] = (fSize >> 32) & 0xFF;
    sizeBytes[4] = (fSize >> 24) & 0xFF;
    sizeBytes[5] = (fSize >> 16) & 0xFF;
    sizeBytes[6] = (fSize >>  8) & 0xFF;
    sizeBytes[7] = (fSize      ) & 0xFF;
    header.append(sizeBytes);

    socket->write(header);

    QByteArray buffer;
    qint64 totalSent = 0;
    while (!f.atEnd()) {
        buffer = f.read(65536);
        socket->write(buffer);
        totalSent += buffer.size();
    }
    f.close();
    socket->flush();

    transferCompleted = true;
    emit transferDone(fileName);
    socket->disconnectFromHost();
}

void FileTransfer::onBytesWritten(qint64 bytes) {
    totalWritten += bytes;
    if (fileSize > 0) {
        int percent = (int)((totalWritten * 100) / fileSize);
        emit transferProgress(percent);
    }
}

void FileTransfer::onError(QAbstractSocket::SocketError error) {
    if (!transferActive) {
        return;
    }

    if (transferCompleted &&
        (error == QAbstractSocket::RemoteHostClosedError ||
         error == QAbstractSocket::OperationError)) {
        return;
    }

    transferActive = false;
    emit transferFailed("Connection error: " + socket->errorString());
}