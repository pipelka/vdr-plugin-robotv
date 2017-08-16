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

#include "epgcontroller.h"
#include "net/msgpacket.h"
#include "robotv/robotvcommand.h"
#include "tools/hash.h"
#include "db/storage.h"
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
    const cChannel* channel = findChannelByUid(Channels, channelUid);

    if(channel != NULL) {
        dsyslog("get schedule called for channel '%s'", (const char*)channel->GetChannelID().ToString());
    }

    MsgPacket* response = createResponse(request);

    if(!channel) {
        response->put_U32(0);

        esyslog("written 0 because channel = NULL");
        return response;
    }

    LOCK_SCHEDULES_READ;

    if(!Schedules) {
        response->put_U32(0);

        dsyslog("written 0 because Schedule!s! = NULL");
        return response;
    }

    const cSchedule* Schedule = Schedules->GetSchedule(channel->GetChannelID());

    if(!Schedule) {
        response->put_U32(0);

        dsyslog("written 0 because Schedule = NULL");
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
    cSchedulesLock MutexLock;

    std::string searchTerm = request->get_String();
    time_t now = time(NULL);
    MsgPacket* response = createResponse(request);

    LOCK_SCHEDULES_READ;
    LOCK_CHANNELS_READ;

    if(Schedules == nullptr) {
        return response;
    }

    searchEpg(searchTerm, [&](tEventID eventId, time_t timeStamp, tChannelID channelId) {
        if(timeStamp < now) {
            return;
        }

        const cSchedule* schedule = Schedules->GetSchedule(channelId);

        if(schedule == nullptr) {
            return;
        }

        const cEvent* event = schedule->GetEvent(eventId, timeStamp);

        if(event == nullptr) {
            return;
        }

        TimerController::event2Packet(event, response);

        auto channel = Channels->GetByChannelID(channelId);
        response->put_String(channel ? channel->Name() : "");
        response->put_U32(createChannelUid(channel));
    });

    response->compress(9);
    return response;
}

bool EpgController::searchEpg(const std::string& searchTerm, std::function<void(tEventID, time_t, tChannelID)> callback) {
    roboTV::Storage storage;

    sqlite3_stmt* s = storage.query(
                          "SELECT epgindex.eventid,epgindex.timestamp,epgindex.channelid "
                          "FROM epgindex JOIN epgsearch ON epgindex.docid=epgsearch.docid "
                          "WHERE epgsearch MATCH %Q",
                          searchTerm.c_str()
                      );

    if(s == nullptr) {
        return false;
    }

    while(sqlite3_step(s) == SQLITE_ROW) {
        tEventID eventId = sqlite3_column_int(s, 0);
        time_t timeStamp = (time_t)sqlite3_column_int64(s, 1);
        tChannelID channelId = tChannelID::FromString((const char*)sqlite3_column_text(s, 2));

        callback(eventId, timeStamp, channelId);
    }

    sqlite3_finalize(s);
    return true;
}
