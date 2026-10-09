#ifndef UDPBEACONBACKEND_H
#define UDPBEACONBACKEND_H

#include "discoverybackend.h"
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>

class UdpBeaconBackend : public DiscoveryBackend {
    Q_OBJECT
public:
    explicit UdpBeaconBackend(QObject *parent = nullptr);
    ~UdpBeaconBackend() override;

    void start() override;
    void stop() override;

private slots:
    void sendBeacon();

private:
    QUdpSocket *udpSocket;
    QTimer *timer;
};

#endif // UDPBEACONBACKEND_H
