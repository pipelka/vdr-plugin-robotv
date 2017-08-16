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

MsgPacket* MovieController::process(MsgPacket* request) {
    switch(request->getMsgID()) {
        case ROBOTV_RECORDINGS_DISKSIZE:
            return processGetDiskSpace(request);

        case ROBOTV_RECORDINGS_GETFOLDERS:
            return processGetFolders(request);

        case ROBOTV_RECORDINGS_GETLIST:
            return processGetList(request);

        case ROBOTV_RECORDINGS_RENAME:
            return processRename(request);

        case ROBOTV_RECORDINGS_DELETE:
            return processDelete(request);

        case ROBOTV_RECORDINGS_SETPLAYCOUNT:
            return processSetPlayCount(request);

        case ROBOTV_RECORDINGS_SETPOSITION:
            return processSetPosition(request);

        case ROBOTV_RECORDINGS_SETURLS:
            return processSetUrls(request);

        case ROBOTV_RECORDINGS_GETPOSITION:
            return processGetPosition(request);

        case ROBOTV_RECORDINGS_GETMARKS:
            return processGetMarks(request);

        case ROBOTV_RECORDINGS_SEARCH:
            return processSearch(request);
    }

    return nullptr;
}

MsgPacket* MovieController::processGetDiskSpace(MsgPacket* request) {
    int freeMb = 0;
    int percent = cVideoDirectory::VideoDiskSpace(&freeMb);
    int total = (freeMb / (100 - percent)) * 100;

    MsgPacket* response = createResponse(request);

    response->put_U32(total);
    response->put_U32(freeMb);
    response->put_U32(percent);

    return response;
}

MsgPacket* MovieController::processGetFolders(MsgPacket* request) {
    RoboTVServerConfig& config = RoboTVServerConfig::instance();
    std::set<std::string> folders;
    size_t size = config.seriesFolder.length();

    if(size  > 0) {
        folders.insert(config.seriesFolder);
    }

    LOCK_RECORDINGS_READ;

    for(auto recording = Recordings->First(); recording; recording = Recordings->Next(recording)) {
        std::string folder = folderFromName(recording->Name());

        if(folder.empty() || (size > 0 && folder.substr(0, size + 1) == config.seriesFolder + "/")) {
            continue;
        }

        folders.insert(folder);
    }

    MsgPacket* response = createResponse(request);

    for(auto& folder: folders) {
        response->put_String(folder);
    }

    response->compress(9);
    return response;
}

MsgPacket* MovieController::processGetList(MsgPacket* request) {
    MsgPacket* response = createResponse(request);

    LOCK_RECORDINGS_READ;

    for(auto recording = Recordings->First(); recording; recording = Recordings->Next(recording)) {
        recordingToPacket(recording, response);
    }

    response->compress(9);
    return response;

}

MsgPacket* MovieController::processRename(MsgPacket* request) {
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

    MsgPacket* response = createResponse(request);

    LOCK_RECORDINGS_WRITE;
    auto recording = RecordingsCache::instance().lookup(Recordings, uid);

    if(recording == nullptr) {
        esyslog("recording with id '%s' not found!", recid);
        response->put_U32(ROBOTV_RET_DATAINVALID);
        return response;
    }

    isyslog("renaming recording '%s' to '%s'", recording->Name(), newName);

    bool success = recording->ChangeName(newName);

    if(success) {
        RecordingsCache::instance().update(uid, recording);
    }

    Recordings->Update();
    Recordings->TouchUpdate();

    response->put_U32(success ? 0 : -1);
    return response;
}

MsgPacket* MovieController::processDelete(MsgPacket* request) {
    const char* recid = request->get_String();
    uint32_t uid = recid2uid(recid);

    LOCK_RECORDINGS_WRITE;

    auto recording = RecordingsCache::instance().lookup(Recordings, uid);
    MsgPacket* response = createResponse(request);

    if(recording == nullptr) {
        esyslog("Recording not found !");
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return response;
    }

    dsyslog("deleting recording: %s", recording->Name());

    cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());

    if(rc != NULL) {
        esyslog("Recording \"%s\" is in use by timer %d", recording->Name(), rc->Timer()->Index() + 1);
        response->put_U32(ROBOTV_RET_DATALOCKED);
        return response;
    }

    if(!recording->Delete()) {
        esyslog("Error while deleting recording!");
        response->put_U32(ROBOTV_RET_ERROR);
        return response;
    }

    Recordings->DelByName(recording->FileName());
    isyslog("Recording \"%s\" deleted", recording->FileName());
    response->put_U32(ROBOTV_RET_OK);

    return response;
}

MsgPacket* MovieController::processSetPlayCount(MsgPacket* request) {
    const char* recid = request->get_String();
    uint32_t count = request->get_U32();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setPlayCount(uid, count);

    return createResponse(request);
}

MsgPacket* MovieController::processSetPosition(MsgPacket* request) {
    const char* recid = request->get_String();
    uint64_t position = request->get_U64();

    uint32_t uid = recid2uid(recid);
    RecordingsCache::instance().setLastPlayedPosition(uid, position);

    return createResponse(request);
}

MsgPacket* MovieController::processGetPosition(MsgPacket* request) {
    const char* recid = request->get_String();

    uint32_t uid = recid2uid(recid);
    uint64_t position = RecordingsCache::instance().getLastPlayedPosition(uid);

    MsgPacket* response = createResponse(request);
    response->put_U64(position);
    return response;
}

MsgPacket* MovieController::processGetMarks(MsgPacket* request) {
    const char* recid = request->get_String();
    uint32_t uid = recid2uid(recid);

    LOCK_RECORDINGS_READ;
    auto recording = RecordingsCache::instance().lookup(Recordings, uid);

    MsgPacket* response = createResponse(request);

    if(recording == NULL) {
        esyslog("GetMarks: recording not found !");
        response->put_U32(ROBOTV_RET_DATAUNKNOWN);
        return response;
    }

    cMarks marks;

    if(!marks.Load(recording->FileName(), recording->FramesPerSecond(), recording->IsPesRecording())) {
        isyslog("no marks found for: '%s'", recording->FileName());
        response->put_U32(ROBOTV_RET_NOTSUPPORTED);
        return response;
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

    return response;
}

MsgPacket* MovieController::processSetUrls(MsgPacket* request) {
    RecordingsCache& cache = RecordingsCache::instance();

    const char* recid = request->get_String();
    const char* poster = request->get_String();
    const char* background = request->get_String();
    uint32_t id = request->get_U32();

    uint32_t uid = recid2uid(recid);
    cache.setPosterUrl(uid, poster);
    cache.setBackgroundUrl(uid, background);
    cache.setMovieID(uid, id);

    return createResponse(request);
}

MsgPacket* MovieController::processSearch(MsgPacket* request) {
    RecordingsCache& cache = RecordingsCache::instance();

    const char* searchTerm = request->get_String();
    MsgPacket* response = createResponse(request);

    LOCK_RECORDINGS_READ;

    cache.search(searchTerm, [&](uint32_t recid) {
        auto recording = cache.lookup(Recordings, recid);

        if(recording == NULL) {
            return;
        }

        recordingToPacket(recording, response);
    });

    return response;
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

void MovieController::recordingToPacket(const cRecording* recording, MsgPacket* response) {
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
