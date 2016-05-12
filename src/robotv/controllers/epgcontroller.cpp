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
#include "robotv/robotvchannels.h"
#include "tools/hash.h"
#include "db/storage.h"
#include "timercontroller.h"

EpgController::EpgController() {
}

EpgController::EpgController(const EpgController& orig) {
}

EpgController::~EpgController() {
}

bool EpgController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_EPG_GETFORCHANNEL:
            return processGet(request, response);

        case ROBOTV_EPG_SEARCH:
            return processSearch(request, response);
    }

    return false;
}

bool EpgController::processGet(MsgPacket* request, MsgPacket* response) {
    uint32_t channelUid = request->get_U32();
    uint32_t startTime = request->get_U32();
    uint32_t duration = request->get_U32();

    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);

    const cChannel* channel = NULL;

    channel = findChannelByUid(channelUid);

    if(channel != NULL) {
        DEBUGLOG("get schedule called for channel '%s'", (const char*)channel->GetChannelID().ToString());
    }

    if(!channel) {
        response->put_U32(0);
        c.unlock();

        ERRORLOG("written 0 because channel = NULL");
        return true;
    }

    cSchedulesLock MutexLock;
    const cSchedules* Schedules = cSchedules::Schedules(MutexLock);

    if(!Schedules) {
        response->put_U32(0);
        c.unlock();

        DEBUGLOG("written 0 because Schedule!s! = NULL");
        return true;
    }

    const cSchedule* Schedule = Schedules->GetSchedule(channel->GetChannelID());

    if(!Schedule) {
        response->put_U32(0);
        c.unlock();

        DEBUGLOG("written 0 because Schedule = NULL");
        return true;
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

        if(m_artwork.get(eventContent, m_toUtf8.convert(eventTitle), posterUrl, backgroundUrl)) {
            response->put_String(posterUrl.c_str());
            response->put_String(backgroundUrl.c_str());
        }
        else {
            response->put_String("x");
            response->put_String("x");
        }
    }

    c.unlock();
    return true;
}

bool EpgController::processSearch(MsgPacket* request, MsgPacket* response) {
    cSchedulesLock MutexLock;

    std::string searchTerm = request->get_String();
    time_t now = time(NULL);

    const cSchedules* schedules = cSchedules::Schedules(MutexLock);

    if(schedules == nullptr) {
        return true;
    }

    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);
    cChannels* channels = c.get();

    searchEpg(searchTerm, [&](tEventID eventId, time_t timeStamp, tChannelID channelId) {
        if(timeStamp < now) {
            return;
        }

        const cSchedule* schedule = schedules->GetSchedule(channelId);

        if(schedule == nullptr) {
            return;
        }

        const cEvent* event = schedule->GetEvent(eventId, timeStamp);

        if(event == nullptr) {
            return;
        }

        TimerController::event2Packet(event, response);

        cChannel* channel = channels->GetByChannelID(channelId);
        response->put_String(channel ? channel->Name() : "");
        response->put_U32(createChannelUid(channel));
    });

    c.unlock();
    return true;
}

bool EpgController::searchEpg(const std::string& searchTerm, std::function<void(tEventID, time_t, tChannelID)> callback) {
    roboTV::Storage& storage = roboTV::Storage::getInstance();

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
