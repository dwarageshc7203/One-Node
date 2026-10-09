#include "mdnsadvertiser.h"

#include <QByteArray>
#include <QDebug>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

MdnsAdvertiser::MdnsAdvertiser(QObject *parent)
    : DiscoveryBackend(parent), stopRequested(false), simplePoll(nullptr), client(nullptr), group(nullptr), serviceName(QStringLiteral("OneNode"))
{
}

MdnsAdvertiser::~MdnsAdvertiser() {
    stop();
}

void MdnsAdvertiser::start() {
    if (workerThread.joinable()) {
        return;
    }

    stopRequested = false;
    workerThread = std::thread(&MdnsAdvertiser::run, this);
}

void MdnsAdvertiser::stop() {
    if (!workerThread.joinable()) {
        return;
    }

    stopRequested = true;
    if (AvahiSimplePoll *poll = simplePoll.load()) {
        avahi_simple_poll_quit(poll);
    }

    workerThread.join();
}

void MdnsAdvertiser::run() {
    int error = 0;

    simplePoll.store(avahi_simple_poll_new());
    if (!simplePoll.load()) {
        qWarning() << "Failed to create Avahi simple poll";
        return;
    }

    client = avahi_client_new(avahi_simple_poll_get(simplePoll.load()), AvahiClientFlags(0),
                              &MdnsAdvertiser::clientCallback, this, &error);
    if (!client) {
        qWarning() << "Failed to create Avahi client:" << avahi_strerror(error);
        avahi_simple_poll_free(simplePoll.load());
        simplePoll.store(nullptr);
        return;
    }

    if (stopRequested) {
        avahi_simple_poll_quit(simplePoll.load());
    }

    avahi_simple_poll_loop(simplePoll.load());

    if (group) {
        avahi_entry_group_free(group);
        group = nullptr;
    }
    if (client) {
        avahi_client_free(client);
        client = nullptr;
    }
    if (AvahiSimplePoll *poll = simplePoll.exchange(nullptr)) {
        avahi_simple_poll_free(poll);
    }
}

void MdnsAdvertiser::clientCallback(AvahiClient *client, AvahiClientState state, void *userdata) {
    auto *self = static_cast<MdnsAdvertiser *>(userdata);
    if (!self) {
        return;
    }

    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        self->createServices(client);
        break;
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        if (self->group) {
            avahi_entry_group_reset(self->group);
        }
        break;
    case AVAHI_CLIENT_FAILURE:
        qWarning() << "Avahi client failure:" << avahi_strerror(avahi_client_errno(client));
        if (AvahiSimplePoll *poll = self->simplePoll.load()) {
            avahi_simple_poll_quit(poll);
        }
        break;
    default:
        break;
    }
}

void MdnsAdvertiser::entryGroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata) {
    auto *self = static_cast<MdnsAdvertiser *>(userdata);
    if (!self) {
        return;
    }

    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        qInfo() << "mDNS service established";
        break;
    case AVAHI_ENTRY_GROUP_COLLISION: {
        QByteArray currentName = self->serviceName.toUtf8();
        char *alternativeName = avahi_alternative_service_name(currentName.constData());
        self->serviceName = QString::fromUtf8(alternativeName);
        avahi_free(alternativeName);
        self->createServices(avahi_entry_group_get_client(group));
        break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE:
        qWarning() << "Avahi entry group failure:" << avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(group)));
        if (AvahiSimplePoll *poll = self->simplePoll.load()) {
            avahi_simple_poll_quit(poll);
        }
        break;
    default:
        break;
    }
}

void MdnsAdvertiser::createServices(AvahiClient *client) {
    if (!client) {
        return;
    }

    if (!group) {
        group = avahi_entry_group_new(client, &MdnsAdvertiser::entryGroupCallback, this);
        if (!group) {
            qWarning() << "Failed to create Avahi entry group";
            return;
        }
    } else {
        avahi_entry_group_reset(group);
    }

    QByteArray serviceNameBytes = serviceName.toUtf8();
    int ret = avahi_entry_group_add_service(group,
                                            AVAHI_IF_UNSPEC,
                                            AVAHI_PROTO_UNSPEC,
                                            AvahiPublishFlags(0),
                                            serviceNameBytes.constData(),
                                            "_onenode._tcp",
                                            nullptr,
                                            nullptr,
                                            45678,
                                            nullptr);
    if (ret < 0) {
        qWarning() << "Failed to add Avahi service:" << avahi_strerror(ret);
        return;
    }

    ret = avahi_entry_group_commit(group);
    if (ret < 0) {
        qWarning() << "Failed to commit Avahi service:" << avahi_strerror(ret);
    }
}