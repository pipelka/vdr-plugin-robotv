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
#include <fcntl.h>
#include <cstring>

#include "config/config.h"
#include "net/msgpacket.h"
#include "livequeue.h"
#include "tools/time.h"

std::string LiveQueue::m_timeShiftDir;
uint64_t LiveQueue::m_bufferSize = 1024 * 1024 * 1024;

LiveQueue::LiveQueue(int socket) : m_readFd(-1), m_writeFd(-1), m_socket(socket) {
    m_wrapped = false;
    m_hasWrapped = false;
    m_writerRunning = true;
    m_wrapCount = 0;
    m_queueStartTime = roboTV::currentTimeMillis();
    m_lastSyncTime = roboTV::currentTimeMillis();
    m_writeThread = nullptr;
    m_pause = false;

    if(m_timeShiftDir.empty()) {
        m_timeShiftDir = "/video";
    }
}

LiveQueue::~LiveQueue() {
    m_writerRunning = false;
    close();

    if(m_writeThread != nullptr) {
        m_writeThread->join();
    }

    while(!m_writerQueue.empty()) {
        const PacketData& p = m_writerQueue.front();
        delete p.p;
        m_writerQueue.pop_front();
    }

    delete m_writeThread;
    isyslog("LiveQueue terminated");
}

void LiveQueue::start() {
    if(m_writeThread != nullptr) {
        return;
    }

    // set queue start time
    m_queueStartTime = roboTV::currentTimeMillis();

    m_writeThread = new std::thread([&]() {
        createRingBuffer();

        while(m_writerRunning) {

            while(m_writerRunning && !m_writerQueue.empty()) {
                PacketData p{};

                {
                    std::lock_guard<std::mutex> lock(m_mutexQueue);
                    p = m_writerQueue.front();
                    m_writerQueue.pop_front();
                }

                write(p);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

}

void LiveQueue::createRingBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_pause = false;
    off_t length = (off_t)m_bufferSize + 1024 * 1024;

    m_storage = cString::sprintf("%s/robotv-ringbuffer-%05i.data", m_timeShiftDir.c_str(), m_socket);
    dsyslog("timeshift file: %s", (const char*)m_storage);

    m_writeFd = open(m_storage, O_CREAT | O_WRONLY, 0644);
    int rc = posix_fallocate(m_writeFd, 0, length);

    if(rc != 0) {
        dsyslog("unable to pre-allocate %li bytes for timeshift ringbuffer", length);
        dsyslog("ERROR: %s (status = %i)", strerror(rc), rc);
    }

    m_readFd = open(m_storage, O_NOATIME | O_RDONLY, 0644);
    posix_fadvise(m_readFd, 0, length, POSIX_FADV_SEQUENTIAL);

    if(m_readFd == -1) {
        esyslog("Failed to create timeshift ringbuffer !");
    }

    lseek(m_readFd, 0, SEEK_SET);
    lseek(m_writeFd, 0, SEEK_SET);

}

MsgPacket* LiveQueue::read() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_pause) {
        return nullptr;
    }

    return internalRead();
}

MsgPacket* LiveQueue::internalRead() {
    // check if read position wrapped

    off_t readPosition = lseek(m_readFd, 0, SEEK_CUR);
    off_t writePosition = lseek(m_writeFd, 0, SEEK_CUR);

    if(readPosition == (off_t)-1 || writePosition == (off_t)-1) {
        return nullptr;
    }

    if(readPosition >= (off_t)m_bufferSize) {
        isyslog("timeshift: read buffer wrap");
        lseek(m_readFd, 0, SEEK_SET);
        readPosition = 0;
        m_wrapped = !m_wrapped;
        isyslog("wrapped: %s", m_wrapped ? "yes" : "no");
    }

    // check if read position is still behind write position (if not wrapped))
    // if not -> skip packet (as we would start reading from the beginning of
    // the buffer)

    if(readPosition >= writePosition && !m_wrapped) {
        return nullptr;
    }

    // read packet from storage
    return MsgPacket::read(m_readFd, 1000);
}

bool LiveQueue::isPaused() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pause;
}

void LiveQueue::queue(MsgPacket* p, StreamInfo::Content content, int64_t pts) {
    start();

    {
        std::lock_guard<std::mutex> lock(m_mutexQueue);

        if (m_writerQueue.size() >= 400) {
            delete p;
            return;
        }

        m_writerQueue.push_back({p, content, pts});
    }
}

bool LiveQueue::write(const PacketData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto timeStamp = roboTV::currentTimeMillis();
    auto p = data.p;
    auto content = data.content;
    auto pts = data.pts;


    // first packet set start time
    if(m_indexList.empty()) {
        m_queueStartTime = roboTV::currentTimeMillis();
    }

    // ring-buffer overrun ?

    off_t writePosition = lseek(m_writeFd, 0, SEEK_CUR);
    off_t readPosition = lseek(m_readFd, 0, SEEK_CUR);

    if(writePosition >= (off_t) m_bufferSize) {
        isyslog("timeshift: write buffer wrap");
        lseek(m_writeFd, 0, SEEK_SET);
        writePosition = 0;

        m_wrapped = !m_wrapped;
        m_hasWrapped = true;
        m_wrapCount++;

        isyslog("wrapped: %s", m_wrapped ? "yes" : "no");
    }

    off_t packetEndPosition = writePosition + p->getPacketLength();

    // check if write position if still behind read position (if wrapped)
    // if not -> shift read position forward

    while(packetEndPosition >= readPosition && m_wrapped) {
        if(internalRead() == nullptr) {
            esyslog("write overlap - wrapped read position behind write position !");
            delete p;
            return false;
        }
    }

    trim(packetEndPosition);

    // add keyframe to map
    bool keyFrame = (p->getClientID() == (uint16_t)StreamInfo::FrameType::IFRAME);

    if(keyFrame && content == StreamInfo::Content::VIDEO) {
        m_indexList.push_back({writePosition, timeStamp, pts, m_wrapCount});
    }

    // write packet
    bool success = p->write(m_writeFd, 1000);

    if(!success) {
        esyslog("Unable to write packet into timeshift ringbuffer !");
    }

    // sync every 2 seconds
    // we just want to avoid delays of the write-back cache hitting
    // us on buffer-wrap (or any other occasion)

    std::chrono::milliseconds now = roboTV::currentTimeMillis();

    if(now - m_lastSyncTime >= std::chrono::milliseconds(2000)) {
        if(fdatasync(m_writeFd) != 0) {
            esyslog("Failed to sync timeshift ring-buffer !");
        }

        m_lastSyncTime = now;
    }

    delete p;
    return success;
}

void LiveQueue::close() {
    ::close(m_readFd);
    ::close(m_writeFd);

    if(*m_storage) {
        unlink(m_storage);
    }
}

void LiveQueue::trim(off_t position) {
    if(!m_hasWrapped || m_indexList.empty()) {
        return;
    }

    auto p = m_indexList.front();

    if(p.filePosition < position && p.wrapCount < m_wrapCount) {
        m_indexList.pop_front();
    }

    if(!m_indexList.empty()) {
        p = m_indexList.front();
        m_queueStartTime = p.wallclockTime;
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
    dsyslog("TIMESHIFTDIR: %s", m_timeShiftDir.c_str());
}

void LiveQueue::setBufferSize(uint64_t s) {
    m_bufferSize = s;
    isyslog("timeshift buffersize: %lu bytes", m_bufferSize);
}

void LiveQueue::removeTimeShiftFiles() {
    DIR* dir = opendir(m_timeShiftDir.c_str());

    if(dir == nullptr) {
        return;
    }

    struct dirent* entry = nullptr;

    while((entry = readdir(dir)) != nullptr) {
        if(strncmp(entry->d_name, "robotv-ringbuffer-", 16) == 0) {
            isyslog("Removing old time-shift storage: %s", entry->d_name);
            unlink(AddDirectory(m_timeShiftDir.c_str(), entry->d_name));
        }
    }

    closedir(dir);
}

int64_t LiveQueue::seek(int64_t wallclockPositionMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    isyslog("seek: %lu", wallclockPositionMs);

    auto s = m_indexList.rbegin();
    auto e = m_indexList.rend();
    auto h = m_indexList.begin();

    if(s == e) {
        esyslog("empty timeshift queue - unable to seek");
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
    esyslog("fileposition not found!");
    return 0;
}

int64_t LiveQueue::getTimeshiftStartPosition() {
    return m_queueStartTime.count();
}
