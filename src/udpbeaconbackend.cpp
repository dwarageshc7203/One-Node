#include "udpbeaconbackend.h"

UdpBeaconBackend::UdpBeaconBackend(QObject *parent)
    : DiscoveryBackend(parent), udpSocket(new QUdpSocket(this)), timer(new QTimer(this))
{
    connect(timer, &QTimer::timeout, this, &UdpBeaconBackend::sendBeacon);
}

UdpBeaconBackend::~UdpBeaconBackend() {
    stop();
}

void UdpBeaconBackend::start() {
    timer->start(1000); // Broadcast every 1 second
}

void UdpBeaconBackend::stop() {
    timer->stop();
}

void UdpBeaconBackend::sendBeacon() {
    QJsonObject json;
    json["service"] = "OneNode";
    json["port"] = 45678;

    QByteArray datagram = QJsonDocument(json).toJson(QJsonDocument::Compact);
    udpSocket->writeDatagram(datagram, QHostAddress::Broadcast, 45677);
}
