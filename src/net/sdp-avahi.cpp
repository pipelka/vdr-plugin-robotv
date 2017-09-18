/*
 *      vdr-plugin-robotv - roboTV server plugin for VDR
 *
 *      Copyright (C) 2016 Alexander Pipelka
 *
 *      https://github.com/pipelka/vdr-plugin-robotv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "sdp-avahi.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>

#include <vdr/tools.h>
#include <config/config.h>

using namespace roboTV;

SdpAvahi::SdpAvahi() {
    start();
}

SdpAvahi::~SdpAvahi() {
    stop();
}

bool SdpAvahi::start() {
    if(m_client != nullptr) {
        return false;
    }

    m_poll = avahi_simple_poll_new();

    if(m_poll == nullptr) {
        esyslog("Failed to create simple poll object.");
        return false;
    }

    int error = 0;
    m_client = avahi_client_new(avahi_simple_poll_get(m_poll), static_cast<AvahiClientFlags>(0), clientCallback, this, &error);

    if(m_client == nullptr) {
        esyslog("Failed to create SDP client: %s", avahi_strerror(error));
        return false;
    }

    return true;
}

bool SdpAvahi::stop() {
    if(m_client != nullptr) {
        avahi_client_free(m_client);
        m_client = nullptr;
    }

    if(m_poll != nullptr) {
        avahi_simple_poll_free(m_poll);
        m_poll = nullptr;
    }

    return true;
}

bool SdpAvahi::poll(int timeoutMs) {
    if(m_poll == nullptr) {
        return false;
    }

    return (avahi_simple_poll_iterate(m_poll, timeoutMs) == 0);
}

bool SdpAvahi::createServices(AvahiClient *c) {
    RoboTVServerConfig& config = RoboTVServerConfig::instance();

    if(m_group == nullptr) {
        m_group = avahi_entry_group_new(c, entryGroupCallback, this);

        if(m_group == nullptr) {
            esyslog("failed to create new sdp group: %s\n", avahi_strerror(avahi_client_errno(c)));
            return false;
        }
    }

    if(avahi_entry_group_is_empty(m_group)) {
        isyslog("Adding service '%s'", m_name);

        int rc = avahi_entry_group_add_service(
                m_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                static_cast<AvahiPublishFlags>(0), m_name,
                "_robotv._tcp", nullptr, nullptr, config.listenPort,
                "roboTV VDR streaming",
                nullptr);

        if(rc < 0) {
            esyslog("Failed to add service: %s", avahi_strerror(rc));
            return false;
        }

        rc = avahi_entry_group_commit(m_group);

        if(rc < 0) {
            esyslog("Failed to commit group: %s", avahi_strerror(rc));
            return false;
        }
    }

    return true;
}

void SdpAvahi::entryGroupCallback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata) {
    auto sdp = static_cast<SdpAvahi *>(userdata);
    sdp->m_group = g;

    switch(state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            isyslog("mDNS/DNS-SD Service '%s' successfully established.", m_name);
            break;
        case AVAHI_ENTRY_GROUP_COLLISION : {
            esyslog("Name collision for service '%s'", m_name);
            break;
        }
        case AVAHI_ENTRY_GROUP_FAILURE :
            esyslog("Entry group failure: %s", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));
            break;
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

void SdpAvahi::clientCallback(AvahiClient *c, AvahiClientState state, void *userdata) {
    auto sdp = static_cast<SdpAvahi *>(userdata);

    switch(state) {
        case AVAHI_CLIENT_S_RUNNING:
            sdp->createServices(c);
            isyslog("SDP client up and running");
            break;
        case AVAHI_CLIENT_FAILURE:
            esyslog("Client failure: %s", avahi_strerror(avahi_client_errno(c)));
            break;
        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            if(sdp->m_group) {
                avahi_entry_group_reset(sdp->m_group);
            }
            break;
        case AVAHI_CLIENT_CONNECTING:
            ;
    }
}
