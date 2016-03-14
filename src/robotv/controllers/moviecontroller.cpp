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

bool MovieController::processGetList(MsgPacket* request, MsgPacket* response) {
    for(cRecording* recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
        recordingToPacket(recording, response);
    }

    return true;

}

bool MovieController::processRename(MsgPacket* request, MsgPacket* response) {
    uint32_t uid = 0;
    const char* recid = request->get_String();
    uid = recid2uid(recid);

    const char* newtitle = request->get_String();
    cRecording* recording = RecordingsCache::instance().lookup(uid);
    int r = ROBOTV_RET_DATAINVALID;

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

    response->put_U32(r);
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

void MovieController::recordingToPacket(cRecording* recording, MsgPacket* response) {
    RecordingsCache& cache = RecordingsCache::instance();
    const cEvent* event = recording->Info()->GetEvent();

    time_t recordingStart = event->StartTime();
    int recordingDuration = event->Duration();

    cRecordControl* rc = cRecordControls::GetRecordControl(recording->FileName());

    if(rc) {
        recordingStart    = rc->Timer()->StartTime();
        recordingDuration = rc->Timer()->StopTime() - recordingStart;
    }
    else {
        recordingStart = recording->Start();
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
    response->put_String(recording->Info()->ChannelName() ? m_toUtf8.Convert(recording->Info()->ChannelName()) : "");

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
    const char* title = recording->Info()->Title();
    response->put_String(title ? m_toUtf8.Convert(title) : "");

    // subtitle
    const char* subTitle = recording->Info()->ShortText();
    response->put_String(subTitle ? m_toUtf8.Convert(subTitle) : "");

    // description
    const char* description = recording->Info()->Description();
    response->put_String(description ? m_toUtf8.Convert(description) : "");

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

    response->put_String((isempty(directory)) ? "" : m_toUtf8.Convert(directory));

    // filename / uid of recording
    uint32_t uid = RecordingsCache::instance().add(recording);
    char recid[9];
    snprintf(recid, sizeof(recid), "%08x", uid);
    response->put_String(recid);

    // playcount
    response->put_U32(cache.getPlayCount(uid));

    // content
    response->put_U32(event->Contents());

    // thumbnail url - for future use
    response->put_String((const char*)cache.getPosterUrl(uid));

    // icon url - for future use
    response->put_String((const char*)cache.getBackgroundUrl(uid));

    free(fullname);
}
