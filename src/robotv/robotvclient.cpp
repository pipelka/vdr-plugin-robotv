/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
 *
 *      Copyright (C) 2015 Alexander Pipelka
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <string>

#include <vdr/recording.h>
#include <vdr/channels.h>
#include <vdr/i18n.h>
#include <vdr/videodir.h>
#include <vdr/plugin.h>
#include <vdr/timers.h>
#include <vdr/menu.h>
#include <vdr/device.h>
#include <vdr/sources.h>

#include "config/config.h"
#include "net/msgpacket.h"
#include "recordings/recordingscache.h"
#include "tools/hash.h"
#include "tools/urlencode.h"
#include "tools/recid2uid.h"

#include "robotv/robotvchannels.h"

#include "robotvcommand.h"
#include "robotvclient.h"
#include "robotvserver.h"
#include "timerconflicts.h"


void RoboTvClient::putTimer(cTimer* timer, MsgPacket* p) {
    int flags = checkTimerConflicts(timer);

    p->put_U32(createTimerUid(timer));
    p->put_U32(timer->Flags() | flags);
    p->put_U32(timer->Priority());
    p->put_U32(timer->Lifetime());
    p->put_U32(createChannelUid(timer->Channel()));
    p->put_U32(timer->StartTime());
    p->put_U32(timer->StopTime());
    p->put_U32(timer->Day());
    p->put_U32(timer->WeekDays());
    p->put_String(m_toUtf8.Convert(timer->File()));
}

RoboTvClient::RoboTvClient(int fd, unsigned int id) : m_streamController(this), m_recordingController(this) {
    m_id = id;
    m_loggedIn = false;
    m_statusInterfaceEnabled  = false;
    m_request = NULL;
    m_response = NULL;
    m_compressionLevel = 0;
    m_timeout = 3000;

    m_socket = fd;

    Start();
}

RoboTvClient::~RoboTvClient() {
    DEBUGLOG("%s", __FUNCTION__);

    // shutdown connection
    shutdown(m_socket, SHUT_RDWR);
    Cancel(10);

    // close connection
    close(m_socket);

    // delete messagequeue
    {
        cMutexLock lock(&m_queueLock);

        while(!m_queue.empty()) {
            MsgPacket* p = m_queue.front();
            m_queue.pop();
            delete p;
        }
    }

    DEBUGLOG("done");
}

void RoboTvClient::Action(void) {
    bool bClosed(false);

    // only root may change the priority
    if(geteuid() == 0) {
        SetPriority(10);
    }

    while(Running()) {

        // send pending messages
        {
            cMutexLock lock(&m_queueLock);

            while(!m_queue.empty()) {
                MsgPacket* p = m_queue.front();

                if(!p->write(m_socket, m_timeout)) {
                    break;
                }

                m_queue.pop();
                delete p;
            }
        }

        m_request = MsgPacket::read(m_socket, bClosed, 1000);

        if(bClosed) {
            delete m_request;
            m_request = NULL;
            break;
        }

        if(m_request != NULL) {
            processRequest();
            delete m_request;
        }
    }
}

void RoboTvClient::TimerChange(const cTimer* Timer, eTimerChange Change) {
    // ignore invalid timers
    if(Timer == NULL) {
        return;
    }

    sendTimerChange();
}

void RoboTvClient::ChannelChange(const cChannel* Channel) {
    m_streamController.processChannelChange(Channel);

    cMutexLock msgLock(&m_msgLock);

    if(m_statusInterfaceEnabled && m_protocolVersion >= 6) {
        MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_CHANNELCHANGED, ROBOTV_CHANNEL_STATUS);
        m_channelController.addChannelToPacket(Channel, resp);
        queueMessage(resp);
    }
}

void RoboTvClient::sendTimerChange() {
    cMutexLock lock(&m_msgLock);

    if(m_statusInterfaceEnabled) {
        INFOLOG("Sending timer change request to client #%i ...", m_id);
        MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_TIMERCHANGE, ROBOTV_CHANNEL_STATUS);
        queueMessage(resp);
    }
}

void RoboTvClient::sendMoviesChange() {
    cMutexLock lock(&m_msgLock);

    if(!m_statusInterfaceEnabled) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_RECORDINGSCHANGE, ROBOTV_CHANNEL_STATUS);
    queueMessage(resp);
}

void RoboTvClient::Recording(const cDevice* Device, const char* Name, const char* FileName, bool On) {
    cMutexLock lock(&m_msgLock);

    if(m_statusInterfaceEnabled) {
        MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_RECORDING, ROBOTV_CHANNEL_STATUS);

        resp->put_U32(Device->CardIndex());
        resp->put_U32(On);

        if(Name) {
            resp->put_String(Name);
        }
        else {
            resp->put_String("");
        }

        if(FileName) {
            resp->put_String(FileName);
        }
        else {
            resp->put_String("");
        }

        queueMessage(resp);
    }
}

void RoboTvClient::OsdStatusMessage(const char* Message) {
    cMutexLock lock(&m_msgLock);

    if(m_statusInterfaceEnabled && Message) {
        /* Ignore this messages */
        if(strcasecmp(Message, trVDR("Channel not available!")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Delete timer?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Delete recording?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Press any key to cancel shutdown")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Press any key to cancel restart")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Editing - shut down anyway?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Recording - shut down anyway?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("shut down anyway?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Recording - restart anyway?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Editing - restart anyway?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Delete channel?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Timer still recording - really delete?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Delete marks information?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Delete resume information?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("CAM is in use - really reset?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Really restart?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Stop recording?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Cancel editing?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("Cutter already running - Add to cutting queue?")) == 0) {
            return;
        }
        else if(strcasecmp(Message, trVDR("No index-file found. Creating may take minutes. Create one?")) == 0) {
            return;
        }

        sendStatusMessage(Message);
    }
}

void RoboTvClient::sendStatusMessage(const char* Message) {
    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_MESSAGE, ROBOTV_CHANNEL_STATUS);

    resp->put_U32(0);
    resp->put_String(Message);

    queueMessage(resp);
}

bool RoboTvClient::processRequest() {
    cMutexLock lock(&m_msgLock);

    // set protocol version for all messages
    // except login, because login defines the
    // protocol version

    if(m_request->getMsgID() != ROBOTV_LOGIN) {
        m_request->setProtocolVersion(m_protocolVersion);
    }

    m_response = new MsgPacket(m_request->getMsgID(), ROBOTV_CHANNEL_REQUEST_RESPONSE, m_request->getUID());
    m_response->setProtocolVersion(m_protocolVersion);

    bool result = false;

    /** OPCODE 20 - 39: RoboTV network functions for live streaming */
    result = m_streamController.process(m_request, m_response);

    /** OPCODE 40 - 59: RoboTV network functions for recording streaming */
    result |= m_recordingController.process(m_request, m_response);

    /** OPCODE 60 - 79: RoboTV network functions for channel access */
    result |= m_channelController.process(m_request, m_response);

    switch(m_request->getMsgID()) {
            /** OPCODE 1 - 19: RoboTV network functions for general purpose */
        case ROBOTV_LOGIN:
            result = processLogin();
            break;

        case ROBOTV_GETTIME:
            result = processGetTime();
            break;

        case ROBOTV_ENABLESTATUSINTERFACE:
            result = processEnableStatusInterface();
            break;

        case ROBOTV_UPDATECHANNELS:
            result = processUpdateChannels();
            break;

        case ROBOTV_TIMER_GET:
            result = processTimerGet();
            break;

        case ROBOTV_TIMER_GETLIST:
            result = processTimerGetList();
            break;

        case ROBOTV_TIMER_ADD:
            result = processTimerAdd();
            break;

        case ROBOTV_TIMER_DELETE:
            result = processTimerDelete();
            break;

        case ROBOTV_TIMER_UPDATE:
            result = processTimerUpdate();
            break;


            /** OPCODE 100 - 119: RoboTV network functions for recording access */
        case ROBOTV_RECORDINGS_DISKSIZE:
            result = processMoviesGetDiskSpace();
            break;

        case ROBOTV_RECORDINGS_GETLIST:
            result = processMoviesGetList();
            break;

        case ROBOTV_RECORDINGS_RENAME:
            result = processMoviesRename();
            break;

        case ROBOTV_RECORDINGS_DELETE:
            result = processMoviesDelete();
            break;

        case ROBOTV_RECORDINGS_SETPLAYCOUNT:
            result = processMoviesSetPlayCount();
            break;

        case ROBOTV_RECORDINGS_SETPOSITION:
            result = processMoviesSetPosition();
            break;

        case ROBOTV_RECORDINGS_SETURLS:
            result = processMoviesSetUrls();
            break;

        case ROBOTV_RECORDINGS_GETPOSITION:
            result = processMoviesGetPosition();
            break;

        case ROBOTV_RECORDINGS_GETMARKS:
            result = processMoviesGetMarks();
            break;

        case ROBOTV_ARTWORK_GET:
            result = processArtworkGet();
            break;

        case ROBOTV_ARTWORK_SET:
            result = processArtworkSet();
            break;

            /** OPCODE 120 - 139: RoboTV network functions for epg access and manipulating */
        case ROBOTV_EPG_GETFORCHANNEL:
            result = processEPG_GetForChannel();
            break;

        default:
            break;
    }

    if(result) {
        queueMessage(m_response);
    }

    m_response = NULL;

    return result;
}


/** OPCODE 1 - 19: RoboTV network functions for general purpose */

bool RoboTvClient::processLogin() { /* OPCODE 1 */
    m_protocolVersion = m_request->getProtocolVersion();
    m_compressionLevel = m_request->get_U8();
    m_clientName = m_request->get_String();

    if(m_protocolVersion > ROBOTV_PROTOCOLVERSION || m_protocolVersion < 4) {
        ERRORLOG("Client '%s' has unsupported protocol version '%u', terminating client", m_clientName.c_str(), m_protocolVersion);
        return false;
    }

    INFOLOG("Welcome client '%s' with protocol version '%u'", m_clientName.c_str(), m_protocolVersion);

    // Send the login reply
    time_t timeNow = time(NULL);
    struct tm* timeStruct = localtime(&timeNow);
    int timeOffset = timeStruct->tm_gmtoff;

    m_response->setProtocolVersion(m_protocolVersion);
    m_response->put_U32(timeNow);
    m_response->put_S32(timeOffset);
    m_response->put_String("RoboTV VDR Server");
    m_response->put_String(ROBOTV_VERSION);

    setLoggedIn(true);
    return true;
}

bool RoboTvClient::processGetTime() { /* OPCODE 2 */
    time_t timeNow = time(NULL);
    struct tm* timeStruct = localtime(&timeNow);
    int timeOffset = timeStruct->tm_gmtoff;

    m_response->put_U32(timeNow);
    m_response->put_S32(timeOffset);

    return true;
}

bool RoboTvClient::processEnableStatusInterface() {
    bool enabled = m_request->get_U8();

    enableStatusInterface(enabled);

    m_response->put_U32(ROBOTV_RET_OK);

    return true;
}

bool RoboTvClient::processUpdateChannels() {
    uint8_t updatechannels = m_request->get_U8();

    if(updatechannels <= 5) {
        Setup.UpdateChannels = updatechannels;
        INFOLOG("Setting channel update method: %i", updatechannels);
        m_response->put_U32(ROBOTV_RET_OK);
    }
    else {
        m_response->put_U32(ROBOTV_RET_DATAINVALID);
    }

    return true;
}

/** OPCODE 80 - 99: RoboTV network functions for timer access */

bool RoboTvClient::processTimerGet() { /* OPCODE 81 */
    uint32_t number = m_request->get_U32();

    if(Timers.Count() == 0) {
        m_response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    cTimer* timer = Timers.Get(number - 1);

    if(timer == NULL) {
        m_response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    m_response->put_U32(ROBOTV_RET_OK);
    putTimer(timer, m_response);

    return true;
}

bool RoboTvClient::processTimerGetList() { /* OPCODE 82 */
    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to delete timer - timers being edited at VDR");
        m_response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    cTimer* timer;
    int numTimers = Timers.Count();

    m_response->put_U32(numTimers);

    for(int i = 0; i < numTimers; i++) {
        timer = Timers.Get(i);

        if(!timer) {
            continue;
        }

        putTimer(timer, m_response);
    }

    return true;
}

bool RoboTvClient::processTimerAdd() { /* OPCODE 83 */
    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to add timer - timers being edited at VDR");
        m_response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    m_request->get_U32(); // index unused
    uint32_t flags      = m_request->get_U32() > 0 ? tfActive : tfNone;
    uint32_t priority   = m_request->get_U32();
    uint32_t lifetime   = m_request->get_U32();
    uint32_t channelid  = m_request->get_U32();
    time_t startTime    = m_request->get_U32();
    time_t stopTime     = m_request->get_U32();
    time_t day          = m_request->get_U32();
    uint32_t weekdays   = m_request->get_U32();
    const char* file    = m_request->get_String();
    const char* aux     = m_request->get_String();

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
            m_response->put_U32(ROBOTV_RET_OK);
            return true;
        }
        else {
            ERRORLOG("Timer already defined: %d %s", t->Index() + 1, *t->ToText());
            m_response->put_U32(ROBOTV_RET_DATALOCKED);
        }
    }
    else {
        ERRORLOG("Error in timer settings");
        m_response->put_U32(ROBOTV_RET_DATAINVALID);
    }

    delete timer;

    return true;
}

bool RoboTvClient::processTimerDelete() { /* OPCODE 84 */
    uint32_t uid = m_request->get_U32();
    bool     force  = m_request->get_U32();

    cTimer* timer = findTimerByUid(uid);

    if(timer == NULL) {
        ERRORLOG("Unable to delete timer - invalid timer identifier");
        m_response->put_U32(ROBOTV_RET_DATAINVALID);
        return true;
    }

    if(Timers.BeingEdited()) {
        ERRORLOG("Unable to delete timer - timers being edited at VDR");
        m_response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    if(timer->Recording() && !force) {
        ERRORLOG("Timer is recording and can be deleted (use force to stop it)");
        m_response->put_U32(ROBOTV_RET_RECRUNNING);
        return true;
    }

    timer->Skip();
    cRecordControls::Process(time(NULL));

    INFOLOG("Deleting timer %s", *timer->ToDescr());
    Timers.Del(timer);
    Timers.SetModified();
    m_response->put_U32(ROBOTV_RET_OK);

    return true;
}

bool RoboTvClient::processTimerUpdate() { /* OPCODE 85 */
    uint32_t uid = m_request->get_U32();
    bool active = m_request->get_U32();

    cTimer* timer = findTimerByUid(uid);

    if(timer == NULL) {
        ERRORLOG("Timer not defined");
        m_response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    if(timer->Recording()) {
        INFOLOG("Will not update timer - currently recording");
        m_response->put_U32(ROBOTV_RET_OK);
        return true;
    }

    cTimer t = *timer;

    uint32_t flags      = active ? tfActive : tfNone;
    uint32_t priority   = m_request->get_U32();
    uint32_t lifetime   = m_request->get_U32();
    uint32_t channelid  = m_request->get_U32();
    time_t startTime    = m_request->get_U32();
    time_t stopTime     = m_request->get_U32();
    time_t day          = m_request->get_U32();
    uint32_t weekdays   = m_request->get_U32();
    const char* file    = m_request->get_String();
    const char* aux     = m_request->get_String();

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
        m_response->put_U32(ROBOTV_RET_DATAINVALID);
        return true;
    }

    *timer = t;
    Timers.SetModified();
    sendTimerChange();

    m_response->put_U32(ROBOTV_RET_OK);

    return true;
}


/** OPCODE 100 - 119: RoboTV network functions for recording access */

bool RoboTvClient::processMoviesGetDiskSpace() { /* OPCODE 100 */
    int FreeMB;
#if VDRVERSNUM >= 20102
    int Percent = cVideoDirectory::VideoDiskSpace(&FreeMB);
#else
    int Percent = VideoDiskSpace(&FreeMB);
#endif
    int Total   = (FreeMB / (100 - Percent)) * 100;

    m_response->put_U32(Total);
    m_response->put_U32(FreeMB);
    m_response->put_U32(Percent);

    return true;
}

/*bool RoboTVClient::processMoviesGetCount() {
    m_response->put_U32(Recordings.Count());

    return true;
}*/

bool RoboTvClient::processMoviesGetList() { /* OPCODE 102 */
    RecordingsCache& reccache = RecordingsCache::instance();

    for(cRecording* recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
#if APIVERSNUM >= 10705
        const cEvent* event = recording->Info()->GetEvent();
#else
        const cEvent* event = NULL;
#endif

        time_t recordingStart    = 0;
        int    recordingDuration = 0;

        if(event) {
            recordingStart    = event->StartTime();
            recordingDuration = event->Duration();
        }
        else {
            cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());

            if(rc) {
                recordingStart    = rc->Timer()->StartTime();
                recordingDuration = rc->Timer()->StopTime() - recordingStart;
            }
            else {
#if APIVERSNUM >= 10727
                recordingStart = recording->Start();
#else
                recordingStart = recording->start;
#endif
            }
        }

        DEBUGLOG("GRI: RC: recordingStart=%lu recordingDuration=%i", recordingStart, recordingDuration);

        // recording_time
        m_response->put_U32(recordingStart);

        // duration
        m_response->put_U32(recordingDuration);

        // priority
        m_response->put_U32(
#if APIVERSNUM >= 10727
            recording->Priority()
#else
            recording->priority
#endif
        );

        // lifetime
        m_response->put_U32(
#if APIVERSNUM >= 10727
            recording->Lifetime()
#else
            recording->lifetime
#endif
        );

        // channel_name
        m_response->put_String(recording->Info()->ChannelName() ? m_toUtf8.Convert(recording->Info()->ChannelName()) : "");

        char* fullname = strdup(recording->Name());
        char* recname = strrchr(fullname, FOLDERDELIMCHAR);
        char* directory = NULL;

        if(recname == NULL) {
            recname = fullname;
        }
        else {
            *recname = 0;
            recname++;
            directory = fullname;
        }

        // title
        m_response->put_String(m_toUtf8.Convert(recname));

        // subtitle
        if(!isempty(recording->Info()->ShortText())) {
            m_response->put_String(m_toUtf8.Convert(recording->Info()->ShortText()));
        }
        else {
            m_response->put_String("");
        }

        // description
        if(!isempty(recording->Info()->Description())) {
            m_response->put_String(m_toUtf8.Convert(recording->Info()->Description()));
        }
        else {
            m_response->put_String("");
        }

        // directory
        if(directory != NULL) {
            char* p = directory;

            while(*p != 0) {
                if(*p == FOLDERDELIMCHAR) {
                    *p = '/';
                }

                if(*p == '_') {
                    *p = ' ';
                }

                p++;
            }

            while(*directory == '/') {
                directory++;
            }
        }

        m_response->put_String((isempty(directory)) ? "" : m_toUtf8.Convert(directory));

        // filename / uid of recording
        uint32_t uid = RecordingsCache::instance().add(recording);
        char recid[9];
        snprintf(recid, sizeof(recid), "%08x", uid);
        m_response->put_String(recid);

        // playcount
        m_response->put_U32(reccache.getPlayCount(uid));

        // content
        if(event != NULL) {
            m_response->put_U32(event->Contents());
        }
        else {
            m_response->put_U32(0);
        }

        // thumbnail url - for future use
        m_response->put_String((const char*)reccache.getPosterUrl(uid));

        // icon url - for future use
        m_response->put_String((const char*)reccache.getBackgroundUrl(uid));

        free(fullname);
    }

    m_response->compress(m_compressionLevel);

    return true;
}

bool RoboTvClient::processMoviesRename() { /* OPCODE 103 */
    uint32_t uid = 0;
    const char* recid = m_request->get_String();
    uid = recid2uid(recid);

    const char* newtitle     = m_request->get_String();
    cRecording* recording    = RecordingsCache::instance().lookup(uid);
    int         r            = ROBOTV_RET_DATAINVALID;

    if(recording != NULL) {
        // get filename and remove last part (recording time)
        char* filename_old = strdup((const char*)recording->FileName());
        char* sep = strrchr(filename_old, '/');

        if(sep != NULL) {
            *sep = 0;
        }

        // replace spaces in newtitle
        strreplace((char*)newtitle, ' ', '_');
        char filename_new[512];
        strncpy(filename_new, filename_old, 512);
        sep = strrchr(filename_new, '/');

        if(sep != NULL) {
            sep++;
            *sep = 0;
        }

        strncat(filename_new, newtitle, sizeof(filename_new) - 1);

        INFOLOG("renaming recording '%s' to '%s'", filename_old, filename_new);
        r = rename(filename_old, filename_new);
        Recordings.Update();

        free(filename_old);
    }

    m_response->put_U32(r);

    return true;
}

bool RoboTvClient::processMoviesDelete() { /* OPCODE 104 */
    const char* recid = m_request->get_String();
    uint32_t uid = recid2uid(recid);
    cRecording* recording = RecordingsCache::instance().lookup(uid);

    if(recording == NULL) {
        ERRORLOG("Recording not found !");
        m_response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    DEBUGLOG("deleting recording: %s", recording->Name());

    cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());

    if(rc != NULL) {
        ERRORLOG("Recording \"%s\" is in use by timer %d", recording->Name(), rc->Timer()->Index() + 1);
        m_response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    if(!recording->Delete()) {
        ERRORLOG("Error while deleting recording!");
        m_response->put_U32(ROBOTV_RET_ERROR);
        return true;
    }

    Recordings.DelByName(recording->FileName());
    INFOLOG("Recording \"%s\" deleted", recording->FileName());
    m_response->put_U32(ROBOTV_RET_OK);

    return true;
}

bool RoboTvClient::processMoviesSetPlayCount() {
    const char* recid = m_request->get_String();
    uint32_t count = m_request->get_U32();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setPlayCount(uid, count);

    return true;
}

bool RoboTvClient::processMoviesSetPosition() {
    const char* recid = m_request->get_String();
    uint64_t position = m_request->get_U64();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setLastPlayedPosition(uid, position);

    return true;
}

bool RoboTvClient::processMoviesSetUrls() {
    const char* recid = m_request->get_String();
    const char* poster = m_request->get_String();
    const char* background = m_request->get_String();
    uint32_t id = m_request->get_U32();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setPosterUrl(uid, poster);
    RecordingsCache::instance().setBackgroundUrl(uid, background);
    RecordingsCache::instance().setMovieID(uid, id);

    return true;
}

bool RoboTvClient::processArtworkGet() {
    const char* title = m_request->get_String();
    uint32_t content = m_request->get_U32();

    std::string poster;
    std::string background;

    if(!m_artwork.get(content, title, poster, background)) {
        poster = "x";
        background = "x";
    }

    m_response->put_String(poster.c_str());
    m_response->put_String(background.c_str());
    m_response->put_U32(0); // TODO - externalId

    return true;
}

bool RoboTvClient::processArtworkSet() {
    const char* title = m_request->get_String();
    uint32_t content = m_request->get_U32();
    const char* poster = m_request->get_String();
    const char* background = m_request->get_String();
    uint32_t externalId = m_request->get_U32();

    INFOLOG("set artwork: %s (%i): %s", title, content, background);
    m_artwork.set(content, title, poster, background, externalId);
    return true;
}

bool RoboTvClient::processMoviesGetPosition() {
    const char* recid = m_request->get_String();

    uint32_t uid = recid2uid(recid);
    uint64_t position = RecordingsCache::instance().getLastPlayedPosition(uid);

    m_response->put_U64(position);
    return true;
}

bool RoboTvClient::processMoviesGetMarks() {
#if VDRVERSNUM < 10732
    m_response->put_U32(ROBOTV_RET_NOTSUPPORTED);
    return true;
#endif

    const char* recid = m_request->get_String();
    uint32_t uid = recid2uid(recid);

    cRecording* recording = RecordingsCache::instance().lookup(uid);

    if(recording == NULL) {
        ERRORLOG("GetMarks: recording not found !");
        m_response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    cMarks marks;

    if(!marks.Load(recording->FileName(), recording->FramesPerSecond(), recording->IsPesRecording())) {
        INFOLOG("no marks found for: '%s'", recording->FileName());
        m_response->put_U32(ROBOTV_RET_NOTSUPPORTED);
        return true;
    }

    m_response->put_U32(ROBOTV_RET_OK);

    m_response->put_U64(recording->FramesPerSecond() * 10000);

#if VDRVERSNUM >= 10732

    cMark* end = NULL;
    cMark* begin = NULL;

    while((begin = marks.GetNextBegin(end)) != NULL) {
        end = marks.GetNextEnd(begin);

        if(end != NULL) {
            m_response->put_String("SCENE");
            m_response->put_U64(begin->Position());
            m_response->put_U64(end->Position());
            m_response->put_String(begin->ToText());
        }
    }

#endif

    return true;
}


/** OPCODE 120 - 139: RoboTV network functions for epg access and manipulating */

bool RoboTvClient::processEPG_GetForChannel() { /* OPCODE 120 */
    uint32_t channelUID = m_request->get_U32();
    uint32_t startTime  = m_request->get_U32();
    uint32_t duration   = m_request->get_U32();

    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);

    const cChannel* channel = NULL;

    channel = findChannelByUid(channelUID);

    if(channel != NULL) {
        DEBUGLOG("get schedule called for channel '%s'", (const char*)channel->GetChannelID().ToString());
    }

    if(!channel) {
        m_response->put_U32(0);
        c.unlock();

        ERRORLOG("written 0 because channel = NULL");
        return true;
    }

    cSchedulesLock MutexLock;
    const cSchedules* Schedules = cSchedules::Schedules(MutexLock);

    if(!Schedules) {
        m_response->put_U32(0);
        c.unlock();

        DEBUGLOG("written 0 because Schedule!s! = NULL");
        return true;
    }

    const cSchedule* Schedule = Schedules->GetSchedule(channel->GetChannelID());

    if(!Schedule) {
        m_response->put_U32(0);
        c.unlock();

        DEBUGLOG("written 0 because Schedule = NULL");
        return true;
    }

    bool atLeastOneEvent = false;

    uint32_t thisEventID;
    uint32_t thisEventTime;
    uint32_t thisEventDuration;
    uint32_t thisEventContent;
    uint32_t thisEventRating;
    const char* thisEventTitle;
    const char* thisEventSubTitle;
    const char* thisEventDescription;

    for(const cEvent* event = Schedule->Events()->First(); event; event = Schedule->Events()->Next(event)) {
        thisEventID           = event->EventID();
        thisEventTitle        = event->Title();
        thisEventSubTitle     = event->ShortText();
        thisEventDescription  = event->Description();
        thisEventTime         = event->StartTime();
        thisEventDuration     = event->Duration();
#if defined(USE_PARENTALRATING) || defined(PARENTALRATINGCONTENTVERSNUM)
        thisEventContent      = event->Contents();
        thisEventRating       = 0;
#elif APIVERSNUM >= 10711
        thisEventContent      = event->Contents();
        thisEventRating       = event->ParentalRating();
#else
        thisEventContent      = 0;
        thisEventRating       = 0;
#endif

        //in the past filter
        if((thisEventTime + thisEventDuration) < (uint32_t)time(NULL)) {
            continue;
        }

        //start time filter
        if((thisEventTime + thisEventDuration) <= startTime) {
            continue;
        }

        //duration filter
        if(duration != 0 && thisEventTime >= (startTime + duration)) {
            continue;
        }

        if(!thisEventTitle) {
            thisEventTitle        = "";
        }

        if(!thisEventSubTitle) {
            thisEventSubTitle     = "";
        }

        if(!thisEventDescription) {
            thisEventDescription  = "";
        }

        m_response->put_U32(thisEventID);
        m_response->put_U32(thisEventTime);
        m_response->put_U32(thisEventDuration);
        m_response->put_U32(thisEventContent);
        m_response->put_U32(thisEventRating);

        m_response->put_String(m_toUtf8.Convert(thisEventTitle));
        m_response->put_String(m_toUtf8.Convert(thisEventSubTitle));
        m_response->put_String(m_toUtf8.Convert(thisEventDescription));

        // add epg artwork
        if(m_protocolVersion >= 6) {
            std::string posterUrl;
            std::string backgroundUrl;

            if(m_artwork.get(thisEventContent, m_toUtf8.Convert(thisEventTitle), posterUrl, backgroundUrl)) {
                m_response->put_String(posterUrl.c_str());
                m_response->put_String(backgroundUrl.c_str());
            }
            else {
                m_response->put_String("x");
                m_response->put_String("x");
            }
        }

        atLeastOneEvent = true;
    }

    c.unlock();
    DEBUGLOG("Got all event data");

    if(!atLeastOneEvent) {
        m_response->put_U32(0);
        DEBUGLOG("Written 0 because no data");
    }

    m_response->compress(m_compressionLevel);

    return true;
}

void RoboTvClient::queueMessage(MsgPacket* p) {
    cMutexLock lock(&m_queueLock);
    m_queue.push(p);
}
