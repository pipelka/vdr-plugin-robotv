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

#include <service/epgsearch/services.h>
#include <vdr/plugin.h>
#include "epgcontroller.h"
#include "net/msgpacket.h"
#include "tools/hash.h"
#include "timercontroller.h"

EpgController::EpgController() {
}

EpgController::EpgController(const EpgController& orig) {
}

EpgController::~EpgController() {
}

MsgPacket* EpgController::process(MsgPacket* request) {
    switch(request->getMsgID()) {
        case ROBOTV_EPG_GETFORCHANNEL:
            return processGet(request);

        case ROBOTV_EPG_SEARCH:
            return processSearch(request);
    }

    return nullptr;
}

MsgPacket* EpgController::processGet(MsgPacket* request) {
    uint32_t channelUid = request->get_U32();
    uint32_t startTime = request->get_U32();
    uint32_t duration = request->get_U32();

    LOCK_CHANNELS_READ;
    LOCK_SCHEDULES_READ;

    const cChannel* channel = roboTV::Hash::findChannelByUid(Channels, channelUid);

    if(channel != NULL) {
        dsyslog("get schedule called for channel %i '%s' - %s",
                channel->Number(),
                (const char*)channel->GetChannelID().ToString(),
                m_toUtf8.convert(channel->Name()));
    }

    MsgPacket* response = createResponse(request);

    if(!channel) {
        response->put_U32(0);
        return response;
    }

    if(!Schedules) {
        response->put_U32(0);
        return response;
    }

    const cSchedule* Schedule = Schedules->GetSchedule(channel->GetChannelID());

    if(!Schedule) {
        response->put_U32(0);
        return response;
    }

    for(const cEvent* event = Schedule->Events()->First(); event; event = Schedule->Events()->Next(event)) {

        // collect data
        const char* eventTitle = event->Title();
        const char* eventSubTitle = event->ShortText();
        const char* eventDescription  = event->Description();
        uint32_t eventId = event->EventID();
        uint32_t eventTime = event->StartTime();
        uint32_t eventDuration = event->Duration();
        uint32_t eventContent = event->Contents();
        uint32_t eventRating = event->ParentalRating();

        //in the past filter
        if((eventTime + eventDuration) < (uint32_t)time(NULL)) {
            continue;
        }

        //start time filter
        if((eventTime + eventDuration) <= startTime) {
            continue;
        }

        //duration filter
        if(duration != 0 && eventTime >= (startTime + duration)) {
            continue;
        }

        if(!eventTitle) {
            eventTitle = "";
        }

        if(!eventSubTitle) {
            eventSubTitle = "";
        }

        if(!eventDescription) {
            eventDescription = "";
        }

        response->put_U32(eventId);
        response->put_U32(eventTime);
        response->put_U32(eventDuration);
        response->put_U32(eventContent);
        response->put_U32(eventRating);

        response->put_String(m_toUtf8.convert(eventTitle));
        response->put_String(m_toUtf8.convert(eventSubTitle));
        response->put_String(m_toUtf8.convert(eventDescription));

        // add epg artwork
        std::string posterUrl;
        std::string backgroundUrl;

        if(eventContent != 0 && m_artwork.get(eventContent, m_toUtf8.convert(eventTitle), posterUrl, backgroundUrl)) {
            response->put_String(posterUrl.c_str());
            response->put_String(backgroundUrl.c_str());
        }
        else {
            response->put_String("x");
            response->put_String("x");
        }

        // add more epg information (PROTOCOL VERSION 8)
        if(request->getProtocolVersion() >= 8) {
            response->put_S64(event->Vps());
            response->put_U8(event->TableID());
            response->put_U8(event->Version());
            response->put_U8((uint8_t)event->HasTimer());
            response->put_U8((uint8_t)event->IsRunning());

            const cComponents* components = event->Components();
            response->put_U32((uint32_t)(components ? components->NumComponents() : 0));

            if(components != nullptr) {
                int index = 0;
                tComponent* component;
                while((component = components->Component(index++)) != nullptr) {
                    response->put_String(component->description ? m_toUtf8.convert(component->description) : "");
                    response->put_String(component->language ? component->language : "");
                    response->put_U8(component->type);
                    response->put_U8(component->stream);
                }
            }

        }
    }

    response->compress(9);

    return response;
}

MsgPacket* EpgController::processSearch(MsgPacket* request) {
    std::string searchTerm = request->get_String();
    MsgPacket* response = createResponse(request);

    LOCK_CHANNELS_READ;

    searchEpg(searchTerm, [&](const cEvent* event) {
        auto channelId = event->ChannelID();
        auto channel = Channels->GetByChannelID(channelId);

        if(channel == nullptr) {
            return;
        }

        TimerController::event2Packet(event, response);

        response->put_String(m_toUtf8.convert(channel->Name()));
        response->put_U32(roboTV::Hash::createChannelUid(channel));
        if(request->getProtocolVersion() >= 8) {
            response->put_U32((uint32_t) channel->Number());
        }
    });

    response->compress(9);
    return response;
}

bool EpgController::searchEpg(const std::string& searchTerm, std::function<void(const cEvent* event)> callback) {
    if(searchTerm.size() < 3) {
        return false;
    }

    cPlugin* plugin = cPluginManager::GetPlugin("epgsearch");

    if(plugin == nullptr) {
        esyslog("unable to connect to 'epgsearch' plugin !");
        return false;
    }

    auto serviceData = new Epgsearch_searchresults_v1_0;
    serviceData->query = (char*)searchTerm.c_str();
    serviceData->mode = 1;
    serviceData->channelNr = 0;
    serviceData->useTitle = true;
    serviceData->useSubTitle = true;
    serviceData->useDescription = false;

    if(!plugin->Service("Epgsearch-searchresults-v1.0", serviceData)) {
        esyslog("unable to get 'Epgsearch-searchresults-v1.0' from plugin.");
        return false;
    }

    auto list = serviceData->pResultList;

    if(list == nullptr) {
        return false;
    }

    int count = 0;
    time_t now = time(nullptr);

    for(auto row = list->First(); row; row = list->Next(row)) {
        auto event = row->event;

        if(event->StartTime() < now) {
            continue;
        }

        if(count == 100) {
            break;
        }

        callback(event);
        count++;
    }

    delete serviceData->pResultList;
    delete serviceData;

    return true;
}
