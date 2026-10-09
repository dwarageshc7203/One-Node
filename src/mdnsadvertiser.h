#pragma once

#include "discoverybackend.h"
#include <QString>
#include <thread>
#include <atomic>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

struct AvahiSimplePoll;

class MdnsAdvertiser : public DiscoveryBackend {
public:
    explicit MdnsAdvertiser(QObject *parent = nullptr);
    ~MdnsAdvertiser() override;

    void start() override;
    void stop() override;

private:
    static void clientCallback(AvahiClient *client, AvahiClientState state, void *userdata);
    static void entryGroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);

    void run();
    void createServices(AvahiClient *client);

    std::thread              workerThread;
    std::atomic_bool         stopRequested;
    std::atomic<AvahiSimplePoll *> simplePoll;
    AvahiClient             *client;
    AvahiEntryGroup         *group;
    QString                  serviceName;
};