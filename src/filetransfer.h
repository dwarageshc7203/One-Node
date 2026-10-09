#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QQueue>

class FileTransfer : public QObject {
    Q_OBJECT

public:
    explicit FileTransfer(QObject *parent = nullptr);
    void sendFile(const QString &filePath, const QString &peerIp, int port = 45679, const QString &token = "");

signals:
    void transferStarted(const QString &fileName);
    void transferProgress(int percent);
    void transferDone(const QString &fileName);
    void transferFailed(const QString &reason);

private slots:
    void onConnected();
    void onBytesWritten(qint64 bytes);
    void onError(QAbstractSocket::SocketError error);
    void processNext();
    void onReadyRead();
    void startSendingFile();

private:
    enum HandshakeState {
        WaitServerGreeting,
        WaitServerMac,
        Completed
    };
    HandshakeState hState;
    QByteArray hBuffer;
    QByteArray clientNonce;
    
    QTcpSocket *socket;
    QQueue<QString> fileQueue;
    QString     peerIp;
    int         peerPort;
    QString     token;
    
    QString     filePath;
    QString     fileName;
    qint64      fileSize;
    qint64      totalWritten;
    bool        transferActive;
    bool        transferCompleted;
};