#include "pairingserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QRandomGenerator>

PairingServer::PairingServer(QObject *parent)
    : QObject(parent), server(new QTcpServer(this)), pendingClient(nullptr)
{
    connect(server, &QTcpServer::newConnection,
            this, &PairingServer::onNewConnection);
}

void PairingServer::start(const QString &code) {
    activeCode = code;
    if (!server->isListening()) {
        server->listen(QHostAddress::Any, port());
    }
}

void PairingServer::stop() {
    if (pendingClient) {
        pendingClient->disconnectFromHost();
        pendingClient = nullptr;
    }
    server->close();
}

void PairingServer::onNewConnection() {
    if (pendingClient) {
        QTcpSocket *extra = server->nextPendingConnection();
        extra->disconnectFromHost();
        return;
    }
    pendingClient = server->nextPendingConnection();
    connect(pendingClient, &QTcpSocket::readyRead,
            this, &PairingServer::onDataReceived);
    connect(pendingClient, &QTcpSocket::disconnected,
            this, &PairingServer::onClientDisconnected);
}

void PairingServer::onDataReceived() {
    QByteArray data = pendingClient->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull()) {
        QJsonObject resp;
        resp["status"] = "error";
        resp["message"] = "Invalid data";
        pendingClient->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
        pendingClient->flush();
        pendingClient->disconnectFromHost();
        return;
    }

    QJsonObject obj = doc.object();
    QString receivedCode = obj["code"].toString();
    QString deviceName   = obj["device"].toString("Unknown device");
    QString deviceId     = obj["device_id"].toString("");

    if (receivedCode != activeCode) {
        QJsonObject resp;
        resp["status"] = "error";
        resp["message"] = "Invalid code";
        pendingClient->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
        pendingClient->flush();
        pendingClient->disconnectFromHost();
        emit pairingFailed("Wrong code entered on device");
        return;
    }

    // Generate 32-byte secure random secret
    QByteArray secretBytes;
    for (int i = 0; i < 32; ++i) {
        secretBytes.append(static_cast<char>(QRandomGenerator::system()->bounded(256)));
    }
    QString secretBase64 = secretBytes.toBase64();

    // The desktop also needs its own device_id (will be passed or retrieved in MainWindow)
    // For now we can generate a new one if not passed, but we should probably emit this
    QString serverDeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString clientIp = pendingClient->peerAddress().toString();

    QJsonObject resp;
    resp["status"] = "ok";
    resp["device_id"] = serverDeviceId;
    resp["device_name"] = "Desktop";
    resp["secret"] = secretBase64;
    pendingClient->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n");
    pendingClient->flush();

    emit devicePaired(deviceName, deviceId, secretBase64, serverDeviceId, clientIp);
    stop();
}

void PairingServer::onClientDisconnected() {
    pendingClient = nullptr;
}
