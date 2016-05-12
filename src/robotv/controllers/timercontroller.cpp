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

#include "timercontroller.h"
#include "net/msgpacket.h"
#include "robotv/robotvcommand.h"
#include "robotv/robotvchannels.h"
#include "robotv/robotvclient.h"
#include "tools/hash.h"
#include "tools/utf8conv.h"
#include "vdr/menu.h"

#include <set>

TimerController::TimerController(RoboTvClient* parent) : m_parent(parent) {
}

TimerController::TimerController(const TimerController& orig) {
}

TimerController::~TimerController() {
}

bool TimerController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_TIMER_GET:
            return processGet(request, response);

        case ROBOTV_TIMER_GETLIST:
            return processGetTimers(request, response);

        case ROBOTV_TIMER_ADD:
            return processAdd(request, response);

        case ROBOTV_TIMER_DELETE:
            return processDelete(request, response);

        case ROBOTV_TIMER_UPDATE:
            return processUpdate(request, response);
            break;
    }

    return false;
}

void TimerController::timer2Packet(cTimer* timer, MsgPacket* p) {
    Utf8Conv toUtf8;
    int flags = checkTimerConflicts(timer);

    p->put_U32(timer->Index());
    p->put_U32(timer->Flags() | flags);
    p->put_U32(timer->Priority());
    p->put_U32(timer->Lifetime());
    p->put_U32(createChannelUid(timer->Channel()));
    p->put_U32(timer->StartTime());
    p->put_U32(timer->StopTime());
    p->put_U32(timer->Day());
    p->put_U32(timer->WeekDays());
    p->put_String(toUtf8.convert(timer->File()));

    // get timer event
    const cEvent* event = timer->Event();

    // no event available
    if(event == NULL) {
        p->put_U8(0);
        return;
    }

    // add event
    p->put_U8(1);

    event2Packet(event, p);
}

void TimerController::event2Packet(const cEvent* event, MsgPacket* p) {
    Utf8Conv toUtf8;

    p->put_U32(event->EventID());
    p->put_U32(event->StartTime());
    p->put_U32(event->Duration());

    int i = 0;

    for(;;) {
        uchar c = event->Contents(i++);
        p->put_U8(c);

        if(c == 0) {
            break;
        }
    }

    p->put_U32(event->ParentalRating());
    p->put_String(toUtf8.convert(!isempty(event->Title()) ? event->Title() : ""));
    p->put_String(toUtf8.convert(!isempty(event->ShortText()) ? event->ShortText() : ""));
    p->put_String(toUtf8.convert(!isempty(event->Description()) ? event->Description() : ""));
}

bool TimerController::processGet(MsgPacket* request, MsgPacket* response) { /* OPCODE 81 */
    uint32_t number = request->get_U32();

    if(Timers.Count() == 0) {
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    cTimer* timer = Timers.Get(number - 1);

    if(timer == NULL) {
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    response->put_U32(ROBOTV_RET_OK);
    timer2Packet(timer, response);

    return true;
}

bool TimerController::processGetTimers(MsgPacket* request, MsgPacket* response) {
    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to delete timer - timers being edited at VDR");
        response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    cTimer* timer;
    int numTimers = Timers.Count();

    response->put_U32(numTimers);

    for(int i = 0; i < numTimers; i++) {
        timer = Timers.Get(i);

        if(!timer) {
            continue;
        }

        timer2Packet(timer, response);
    }

    return true;
}

bool TimerController::processAdd(MsgPacket* request, MsgPacket* response) {
    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to add timer - timers being edited at VDR");
        response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    request->get_U32(); // index unused
    uint32_t flags      = request->get_U32() > 0 ? tfActive : tfNone;
    uint32_t priority   = request->get_U32();
    uint32_t lifetime   = request->get_U32();
    uint32_t channelid  = request->get_U32();
    time_t startTime    = request->get_U32();
    time_t stopTime     = request->get_U32();
    time_t day          = request->get_U32();
    uint32_t weekdays   = request->get_U32();
    const char* file    = request->get_String();
    const char* aux     = request->get_String();

    // handle instant timers
    if(startTime == -1 || startTime == 0) {
        startTime = time(NULL);
    }

    struct tm tm_r;

    struct tm* time = localtime_r(&startTime, &tm_r);

    if(day <= 0) {
        day = cTimer::SetTime(startTime, 0);
    }

    int start = time->tm_hour * 100 + time->tm_min;
    time = localtime_r(&stopTime, &tm_r);
    int stop = time->tm_hour * 100 + time->tm_min;

    cString buffer;
    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);

    const cChannel* channel = findChannelByUid(channelid);

    if(channel != NULL) {
        buffer = cString::sprintf("%u:%s:%s:%04d:%04d:%d:%d:%s:%s\n", flags, (const char*)channel->GetChannelID().ToString(), *cTimer::PrintDay(day, weekdays, true), start, stop, priority, lifetime, file, aux);
    }

    c.unlock();

    cTimer* timer = new cTimer;

    if(timer->Parse(buffer)) {
        cTimer* t = Timers.GetTimer(timer);

        if(!t) {
            Timers.Add(timer);
            Timers.SetModified();
            INFOLOG("Timer %s added", *timer->ToDescr());
            response->put_U32(ROBOTV_RET_OK);
            return true;
        }
        else {
            ERRORLOG("Timer already defined: %d %s", t->Index() + 1, *t->ToText());
            response->put_U32(ROBOTV_RET_DATALOCKED);
        }
    }
    else {
        ERRORLOG("Error in timer settings");
        response->put_U32(ROBOTV_RET_DATAINVALID);
    }

    delete timer;

    return true;
}

bool TimerController::processDelete(MsgPacket* request, MsgPacket* response) {
    uint32_t uid = request->get_U32();
    bool force = request->get_U32();

    cTimer* timer = findTimerByUid(uid);

    if(timer == NULL) {
        ERRORLOG("Unable to delete timer - invalid timer identifier");
        response->put_U32(ROBOTV_RET_DATAINVALID);
        return true;
    }

    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to delete timer - timers being edited at VDR");
        response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    if(timer->Recording() && !force) {
        ERRORLOG("Timer is recording and can be deleted (use force to stop it)");
        response->put_U32(ROBOTV_RET_RECRUNNING);
        return true;
    }

    timer->Skip();
    cRecordControls::Process(time(NULL));

    INFOLOG("Deleting timer %s", *timer->ToDescr());
    Timers.Del(timer);
    Timers.SetModified();
    response->put_U32(ROBOTV_RET_OK);

    return true;
}

bool TimerController::processUpdate(MsgPacket* request, MsgPacket* response) {
    uint32_t uid = request->get_U32();
    bool active = request->get_U32();

    cTimer* timer = findTimerByUid(uid);

    if(timer == NULL) {
        ERRORLOG("Timer not defined");
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    if(timer->Recording()) {
        INFOLOG("Will not update timer - currently recording");
        response->put_U32(ROBOTV_RET_OK);
        return true;
    }

    cTimer t = *timer;

    uint32_t flags      = active ? tfActive : tfNone;
    uint32_t priority   = request->get_U32();
    uint32_t lifetime   = request->get_U32();
    uint32_t channelid  = request->get_U32();
    time_t startTime    = request->get_U32();
    time_t stopTime     = request->get_U32();
    time_t day          = request->get_U32();
    uint32_t weekdays   = request->get_U32();
    const char* file    = request->get_String();
    const char* aux     = request->get_String();

    struct tm tm_r;
    struct tm* time = localtime_r(&startTime, &tm_r);

    if(day <= 0) {
        day = cTimer::SetTime(startTime, 0);
    }

    int start = time->tm_hour * 100 + time->tm_min;
    time = localtime_r(&stopTime, &tm_r);
    int stop = time->tm_hour * 100 + time->tm_min;

    cString buffer;
    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);

    const cChannel* channel = findChannelByUid(channelid);

    if(channel != NULL) {
        buffer = cString::sprintf("%u:%s:%s:%04d:%04d:%d:%d:%s:%s\n", flags, (const char*)channel->GetChannelID().ToString(), *cTimer::PrintDay(day, weekdays, true), start, stop, priority, lifetime, file, aux);
    }

    c.unlock();

    if(!t.Parse(buffer)) {
        ERRORLOG("Error in timer settings");
        response->put_U32(ROBOTV_RET_DATAINVALID);
        return true;
    }

    *timer = t;
    Timers.SetModified();
    m_parent->sendTimerChange();

    response->put_U32(ROBOTV_RET_OK);

    return true;
}

int TimerController::checkTimerConflicts(cTimer* timer) {
    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);

    // check for timer conflicts
    DEBUGLOG("Checking conflicts for: %s", (const char*)timer->ToText(true));

    // order active timers by starttime
    std::map<time_t, cTimer*> timeline;
    int numTimers = Timers.Count();

    for(int i = 0; i < numTimers; i++) {
        cTimer* t = Timers.Get(i);

        // same timer -> skip
        if(!t || timer->Index() == i) {
            continue;
        }

        // timer not active -> skip
        if(!(t->Flags() & tfActive)) {
            continue;
        }

        // this one is earlier -> no match
        if(t->StopTime() <= timer->StartTime()) {
            continue;
        }

        // this one is later -> no match
        if(t->StartTime() >= timer->StopTime()) {
            continue;
        }

        timeline[t->StartTime()] = t;
    }

    std::set<int> transponders;
    transponders.insert(timer->Channel()->Transponder()); // we also count ourself
    cTimer* to_check = timer;

    std::map<time_t, cTimer*>::iterator i;

    for(i = timeline.begin(); i != timeline.end(); i++) {
        cTimer* t = i->second;

        // this one is earlier -> no match
        if(t->StopTime() <= to_check->StartTime()) {
            continue;
        }

        // this one is later -> no match
        if(t->StartTime() >= to_check->StopTime()) {
            continue;
        }

        // same transponder -> no conflict
        if(t->Channel()->Transponder() == to_check->Channel()->Transponder()) {
            continue;
        }

        // different source -> no conflict
        if(t->Channel()->Source() != to_check->Channel()->Source()) {
            continue;
        }

        DEBUGLOG("Possible conflict: %s", (const char*)t->ToText(true));
        transponders.insert(t->Channel()->Transponder());

        // now check conflicting timer
        to_check = t;
    }

    uint32_t number_of_devices_for_this_channel = 0;

    for(int i = 0; i < cDevice::NumDevices(); i++) {
        cDevice* device = cDevice::GetDevice(i);

        if(device != NULL && device->ProvidesTransponder(timer->Channel())) {
            number_of_devices_for_this_channel++;
        }
    }

    int cflags = 0;

    if(transponders.size() > number_of_devices_for_this_channel) {
        DEBUGLOG("ERROR - Not enough devices");
        cflags += 2048;
    }
    else if(transponders.size() > 1) {
        DEBUGLOG("Overlapping timers - Will record");
        cflags += 1024;
    }
    else {
        DEBUGLOG("No conflicts");
    }

    c.unlock();
    return cflags;
}
