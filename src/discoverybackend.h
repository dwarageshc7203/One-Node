#ifndef DISCOVERYBACKEND_H
#define DISCOVERYBACKEND_H

#include <QObject>

class DiscoveryBackend : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~DiscoveryBackend() {}

    virtual void start() = 0;
    virtual void stop() = 0;
};

#endif // DISCOVERYBACKEND_H
