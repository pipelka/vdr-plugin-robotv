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

#include <algorithm>
#include <set>
#include "moviecontroller.h"
#include "net/msgpacket.h"
#include "robotv/robotvcommand.h"
#include "tools/recid2uid.h"
#include "config/config.h"
#include "recordings/recordingscache.h"
#include "vdr/videodir.h"
#include "vdr/menu.h"

MovieController::MovieController() {
}

MovieController::MovieController(const MovieController& orig) {
}

MovieController::~MovieController() {
}

bool MovieController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_RECORDINGS_DISKSIZE:
            return processGetDiskSpace(request, response);

        case ROBOTV_RECORDINGS_GETFOLDERS:
            return processGetFolders(request, response);

        case ROBOTV_RECORDINGS_GETLIST:
            return processGetList(request, response);

        case ROBOTV_RECORDINGS_RENAME:
            return processRename(request, response);

        case ROBOTV_RECORDINGS_DELETE:
            return processDelete(request, response);

        case ROBOTV_RECORDINGS_SETPLAYCOUNT:
            return processSetPlayCount(request, response);

        case ROBOTV_RECORDINGS_SETPOSITION:
            return processSetPosition(request, response);

        case ROBOTV_RECORDINGS_SETURLS:
            return processSetUrls(request, response);

        case ROBOTV_RECORDINGS_GETPOSITION:
            return processGetPosition(request, response);

        case ROBOTV_RECORDINGS_GETMARKS:
            return processGetMarks(request, response);

        case ROBOTV_RECORDINGS_SEARCH:
            return processSearch(request, response);
    }

    return false;
}

bool MovieController::processGetDiskSpace(MsgPacket* request, MsgPacket* response) {
    int freeMb = 0;
    int percent = cVideoDirectory::VideoDiskSpace(&freeMb);
    int total = (freeMb / (100 - percent)) * 100;

    response->put_U32(total);
    response->put_U32(freeMb);
    response->put_U32(percent);

    return true;
}

bool MovieController::processGetFolders(MsgPacket* request, MsgPacket* response) {
    RoboTVServerConfig& config = RoboTVServerConfig::instance();
    std::set<std::string> folders;
    size_t size = config.seriesFolder.length();

    if(size  > 0) {
        folders.insert(config.seriesFolder);
    }

    for(cRecording *recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
        std::string folder = folderFromName(recording->Name());

        if(folder.empty() || (size > 0 && folder.substr(0, size + 1) == config.seriesFolder + "/")) {
            continue;
        }

        folders.insert(folder);
    }

    for(auto& folder: folders) {
        response->put_String(folder);
    }

    response->compress(9);
    return true;
}

bool MovieController::processGetList(MsgPacket* request, MsgPacket* response) {
    for(cRecording* recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
        recordingToPacket(recording, response);
    }

    response->compress(9);
    return true;

}

bool MovieController::processRename(MsgPacket* request, MsgPacket* response) {
    uint32_t uid = 0;
    const char* recid = request->get_String();
    uid = recid2uid(recid);

    const char* newName = request->get_String();

    char* s = (char*)newName;

    while(*s != 0) {
        if(*s == ' ' || *s == ':') {
            *s = '_';
        }
        s++;
    }

    cRecording* recording = RecordingsCache::instance().lookup(uid);

    if(recording == nullptr) {
        ERRORLOG("recording with id '%s' not found!", recid);
        response->put_U32(ROBOTV_RET_DATAINVALID);
        return true;
    }

    INFOLOG("renaming recording '%s' to '%s'", recording->Name(), newName);

    bool success = recording->ChangeName(newName);

    if(success) {
        RecordingsCache::instance().update(uid, recording);
    }

    Recordings.Update();
    Recordings.TouchUpdate();

    response->put_U32(success ? 0 : -1);
    return true;
}

bool MovieController::processDelete(MsgPacket* request, MsgPacket* response) {
    const char* recid = request->get_String();
    uint32_t uid = recid2uid(recid);
    cRecording* recording = RecordingsCache::instance().lookup(uid);

    if(recording == NULL) {
        ERRORLOG("Recording not found !");
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    DEBUGLOG("deleting recording: %s", recording->Name());

    cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());

    if(rc != NULL) {
        ERRORLOG("Recording \"%s\" is in use by timer %d", recording->Name(), rc->Timer()->Index() + 1);
        response->put_U32(ROBOTV_RET_DATALOCKED);
        return true;
    }

    if(!recording->Delete()) {
        ERRORLOG("Error while deleting recording!");
        response->put_U32(ROBOTV_RET_ERROR);
        return true;
    }

    Recordings.DelByName(recording->FileName());
    INFOLOG("Recording \"%s\" deleted", recording->FileName());
    response->put_U32(ROBOTV_RET_OK);

    return true;
}

bool MovieController::processSetPlayCount(MsgPacket* request, MsgPacket* response) {
    const char* recid = request->get_String();
    uint32_t count = request->get_U32();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setPlayCount(uid, count);

    return true;
}

bool MovieController::processSetPosition(MsgPacket* request, MsgPacket* response) {
    const char* recid = request->get_String();
    uint64_t position = request->get_U64();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setLastPlayedPosition(uid, position);

    return true;
}

bool MovieController::processGetPosition(MsgPacket* request, MsgPacket* response) {
    const char* recid = request->get_String();

    uint32_t uid = recid2uid(recid);
    uint64_t position = RecordingsCache::instance().getLastPlayedPosition(uid);

    response->put_U64(position);
    return true;
}

bool MovieController::processGetMarks(MsgPacket* request, MsgPacket* response) {
    const char* recid = request->get_String();
    uint32_t uid = recid2uid(recid);

    cRecording* recording = RecordingsCache::instance().lookup(uid);

    if(recording == NULL) {
        ERRORLOG("GetMarks: recording not found !");
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return true;
    }

    cMarks marks;

    if(!marks.Load(recording->FileName(), recording->FramesPerSecond(), recording->IsPesRecording())) {
        INFOLOG("no marks found for: '%s'", recording->FileName());
        response->put_U32(ROBOTV_RET_NOTSUPPORTED);
        return true;
    }

    response->put_U32(ROBOTV_RET_OK);
    response->put_U64(recording->FramesPerSecond() * 10000);

    cMark* end = NULL;
    cMark* begin = NULL;

    while((begin = marks.GetNextBegin(end)) != NULL) {
        end = marks.GetNextEnd(begin);

        if(end != NULL) {
            response->put_String("SCENE");
            response->put_U64(begin->Position());
            response->put_U64(end->Position());
            response->put_String(begin->ToText());
        }
    }

    return true;
}

bool MovieController::processSetUrls(MsgPacket* request, MsgPacket* response) {
    RecordingsCache& cache = RecordingsCache::instance();

    const char* recid = request->get_String();
    const char* poster = request->get_String();
    const char* background = request->get_String();
    uint32_t id = request->get_U32();

    uint32_t uid = recid2uid(recid);
    cache.setPosterUrl(uid, poster);
    cache.setBackgroundUrl(uid, background);
    cache.setMovieID(uid, id);

    return true;
}

bool MovieController::processSearch(MsgPacket* request, MsgPacket* response) {
    RecordingsCache& cache = RecordingsCache::instance();

    const char* searchTerm = request->get_String();

    cache.search(searchTerm, [&](uint32_t recid) {
        cRecording* recording = cache.lookup(recid);

        if(recording == NULL) {
            return;
        }

        recordingToPacket(recording, response);
    });

    return true;
}

std::string MovieController::folderFromName(const std::string& name) {
    size_t pos = name.rfind(FOLDERDELIMCHAR);

    if(pos == std::string::npos) {
        return "";
    }

    std::string folder = name.substr(0, pos);
    std::replace(folder.begin(), folder.end(), '~', '/');
    std::replace(folder.begin(), folder.end(), '_', ' ');

    return folder;
}

void MovieController::recordingToPacket(cRecording* recording, MsgPacket* response) {
    RecordingsCache& cache = RecordingsCache::instance();
    RoboTVServerConfig& config = RoboTVServerConfig::instance();

    time_t recordingStart;
    int recordingDuration;
    int content = 0;

    const cEvent* event = recording->Info()->GetEvent();

    if(event != nullptr) {
        recordingStart = event->StartTime();
        recordingDuration = event->Duration();
        content = event->Contents();
    }
    else {
        cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());
        cTimer* timer = rc->Timer();

        recordingStart = timer->StartTime();
        recordingDuration = (int)(timer->StopTime() - recordingStart);
    }

    // recording_time
    response->put_U32(recordingStart);

    // duration
    response->put_U32(recordingDuration);

    // priority
    response->put_U32(recording->Priority());

    // lifetime
    response->put_U32(recording->Lifetime());

    // channel_name
    response->put_String(recording->Info()->ChannelName() ? m_toUtf8.convert(recording->Info()->ChannelName()) : "");

    // title
    const char* title = recording->Info()->Title();
    response->put_String(title ? m_toUtf8.convert(title) : "");

    // subtitle
    const char* subTitle = recording->Info()->ShortText();
    response->put_String(subTitle ? m_toUtf8.convert(subTitle) : "");

    // description
    const char* description = recording->Info()->Description();
    response->put_String(description ? m_toUtf8.convert(description) : "");

    // directory
    std::string directory = folderFromName(recording->Name());

    if(!directory.empty() && !config.seriesFolder.empty()) {
        if(directory.substr(0, config.seriesFolder.length()) == config.seriesFolder) {
            content = 0x15;
        }
    }

    response->put_String(m_toUtf8.convert(directory.c_str()));

    // filename / uid of recording
    uint32_t uid = RecordingsCache::instance().add(recording);
    char recid[9];
    snprintf(recid, sizeof(recid), "%08x", uid);
    response->put_String(recid);

    // playcount
    response->put_U32(cache.getPlayCount(uid));

    // content
    response->put_U32(content);

    // thumbnail url - for future use
    response->put_String((const char*)cache.getPosterUrl(uid));

    // icon url - for future use
    response->put_String((const char*)cache.getBackgroundUrl(uid));
}
