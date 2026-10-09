#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>

class PairingServer : public QObject {
    Q_OBJECT

public:
    explicit PairingServer(QObject *parent = nullptr);

    void start(const QString &code);
    void stop();
    int  port() const { return 45678; }

signals:
    void devicePaired(const QString &deviceName, const QString &deviceId, const QString &secret, const QString &serverDeviceId, const QString &deviceIp);
    void pairingFailed(const QString &reason);
    
private slots:
    void onNewConnection();
    void onDataReceived();
    void onClientDisconnected();

private:
    QTcpServer *server;
    QTcpSocket *pendingClient;
    QString     activeCode;
};