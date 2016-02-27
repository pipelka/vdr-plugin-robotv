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

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "config/config.h"
#include "net/msgpacket.h"
#include "livequeue.h"

cString LiveQueue::m_timeShiftDir = "/video";
uint64_t LiveQueue::m_bufferSize = 1024 * 1024 * 1024;

LiveQueue::LiveQueue(int sock) : m_socket(sock), m_readFd(-1), m_writeFd(-1) {
    cleanup();
}

LiveQueue::~LiveQueue() {
    DEBUGLOG("Deleting LiveQueue");
    close();
}

void LiveQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_pause = false;

    m_storage = cString::sprintf("%s/robotv-ringbuffer-%05i.data", (const char*)m_timeShiftDir, m_socket);
    DEBUGLOG("timeshift file: %s", (const char*)m_storage);

    m_writeFd = open(m_storage, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    m_readFd = open(m_storage, O_CREAT | O_RDONLY, 0644);

    if(m_readFd == -1) {
        ERRORLOG("Failed to create timeshift ringbuffer !");
    }

    lseek(m_readFd, 0, SEEK_SET);
    lseek(m_writeFd, 0, SEEK_SET);

}

MsgPacket* LiveQueue::request() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_pause) {
        return NULL;
    }

    // check if read position wrapped

    off_t readPosition = lseek(m_readFd, 0, SEEK_CUR);
    off_t writePosition = lseek(m_writeFd, 0, SEEK_CUR);

    if(readPosition >= (off_t)m_bufferSize) {
        INFOLOG("timeshift: read buffer wrap");
        lseek(m_readFd, 0, SEEK_SET);
        readPosition = 0;
        m_wrapped = !m_wrapped;
        INFOLOG("wrapped: %s", m_wrapped ? "yes" : "no");
    }

    // check if read position is still behind write position (if not wrapped))
    // if not -> skip packet (as we would start reading from the beginning of
    // the buffer)

    if(readPosition >= writePosition && !m_wrapped) {
        return NULL;
    }

    // read packet from storage
    return MsgPacket::read(m_readFd, 1000);
}

bool LiveQueue::isPaused() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pause;
}

bool LiveQueue::add(MsgPacket* p, StreamInfo::Content content, bool keyFrame, int64_t pts) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto timeStamp = currentTimeMillis();

    // set queue start time

    if(m_queueStartTime.count() == 0) {
        m_queueStartTime = timeStamp;
    }

    // ring-buffer overrun ?

    off_t writePosition = lseek(m_writeFd, 0, SEEK_CUR);
    off_t readPosition = lseek(m_readFd, 0, SEEK_CUR);

    if(writePosition >= (off_t)m_bufferSize) {
        INFOLOG("timeshift: write buffer wrap");
        lseek(m_writeFd, 0, SEEK_SET);
        writePosition = 0;

        m_wrapped = !m_wrapped;
        m_hasWrapped = true;

        INFOLOG("wrapped: %s", m_wrapped ? "yes" : "no");
    }

    // check if write position if still behind read position (if wrapped)
    // if not -> skip packet (as we would overwrite our read position)

    if(writePosition >= readPosition && m_wrapped) {
        return NULL;
    }

    // add keyframe to map

    if(keyFrame && content == StreamInfo::Content::scVIDEO) {
        m_indexList.push_back({writePosition, timeStamp, pts});
        removeUpToFileposition(writePosition + p->getPacketLength());
    }

    // write packet

    if(!p->write(m_writeFd, 1000)) {
        ERRORLOG("Unable to write packet into timeshift ringbuffer !");
        delete p;
        return false;
    }

    delete p;
    return true;
}

void LiveQueue::close() {
    ::close(m_readFd);
    m_readFd = -1;
    ::close(m_writeFd);
    m_writeFd = -1;

    if(*m_storage) {
        unlink(m_storage);
    }
}

void LiveQueue::removeUpToFileposition(off_t position) {
    if(!m_hasWrapped) {
        return;
    }

    auto i = m_indexList.begin();

    while(i != m_indexList.end() && i->filePosition < position) {
        i = m_indexList.erase(i);
    }

    if(m_indexList.size() > 0) {
        auto i = m_indexList.begin();
        m_queueStartTime = i->wallclockTime;
    }
}

bool LiveQueue::pause(bool on) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_pause == on) {
        return false;
    }

    m_pause = on;
    return true;
}

void LiveQueue::setTimeShiftDir(const cString& dir) {
    m_timeShiftDir = dir;
    DEBUGLOG("TIMESHIFTDIR: %s", (const char*)m_timeShiftDir);
}

void LiveQueue::setBufferSize(uint64_t s) {
    m_bufferSize = s;
    INFOLOG("timeshift buffersize: %lu bytes", m_bufferSize);
}

void LiveQueue::removeTimeShiftFiles() {
    DIR* dir = opendir((const char*)m_timeShiftDir);

    if(dir == NULL) {
        return;
    }

    struct dirent* entry = NULL;

    while((entry = readdir(dir)) != NULL) {
        if(strncmp(entry->d_name, "robotv-ringbuffer-", 16) == 0) {
            INFOLOG("Removing old time-shift storage: %s", entry->d_name);
            unlink(AddDirectory(m_timeShiftDir, entry->d_name));
        }
    }

    closedir(dir);
}

int64_t LiveQueue::seek(int64_t wallclockPositionMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    INFOLOG("seek: %lu", wallclockPositionMs);

    auto s = m_indexList.rbegin();
    auto e = m_indexList.rend();
    auto h = m_indexList.begin();

    if(s == e) {
        ERRORLOG("empty timeshift queue - unable to seek");
        return 0;
    }

    // ahead of buffer
    if(wallclockPositionMs >= s->wallclockTime.count()) {
        lseek(m_readFd, s->filePosition, SEEK_SET);
        return s->pts;
    }

    // behind buffer
    else if(wallclockPositionMs <= h->wallclockTime.count()) {
        lseek(m_readFd, h->filePosition, SEEK_SET);
        return h->pts;
    }

    // in between ?
    while(s != e) {
        if(s->wallclockTime.count() <= wallclockPositionMs) {
            lseek(m_readFd, s->filePosition, SEEK_SET);
            return s->pts;
        }

        s++;
    }

    // not found
    ERRORLOG("fileposition not found!");
    return 0;
}

int64_t LiveQueue::getTimeshiftStartPosition() {
    return m_queueStartTime.count();
}

std::chrono::milliseconds LiveQueue::currentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()
           );
}
