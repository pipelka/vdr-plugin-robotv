/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
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

LiveQueue::LiveQueue(int sock) : m_socket(sock), m_readFd(-1), m_writeFd(-1), m_queueSize(400) {
    m_pause = false;
}

LiveQueue::~LiveQueue() {
    DEBUGLOG("Deleting LiveQueue");
    m_cond.Signal();
    Cancel(3);
    cleanup();
    close();
}

void LiveQueue::cleanup() {
    cMutexLock lock(&m_lock);

    while(!empty()) {
        delete front();
        pop();
    }
}

void LiveQueue::request() {
    cMutexLock lock(&m_lock);

    // read packet from storage
    MsgPacket* p = MsgPacket::read(m_readFd, 1000);

    // check for buffer overrun
    if(p == NULL) {
        // ring-buffer overrun ?
        off_t pos = lseek(m_readFd, 0, SEEK_CUR);

        if(pos < (off_t)m_bufferSize) {
            return;
        }

        lseek(m_readFd, 0, SEEK_SET);
        p = MsgPacket::read(m_readFd, 1000);
    }

    // no packet
    if(p == NULL) {
        return;
    }

    // put packet into queue
    push(p);

    m_cond.Signal();
}

bool LiveQueue::isPaused() {
    cMutexLock lock(&m_lock);
    return m_pause;
}

bool LiveQueue::getTimeShiftMode() {
    cMutexLock lock(&m_lock);
    return (m_pause || (!m_pause && m_writeFd != -1));
}

bool LiveQueue::add(MsgPacket* p, StreamInfo::Content content) {
    cMutexLock lock(&m_lock);

    // in timeshift mode ?
    if(m_pause || (!m_pause && m_writeFd != -1)) {
        // write packet
        if(!p->write(m_writeFd, 1000)) {
            ERRORLOG("Unable to write packet into timeshift ringbuffer !");
            delete p;
            return false;
        }

        // ring-buffer overrun ?
        off_t length = lseek(m_writeFd, 0, SEEK_CUR);

        if(length >= (off_t)m_bufferSize) {
            // truncate to current position
            if(ftruncate(m_writeFd, length) == 0) {
                lseek(m_writeFd, 0, SEEK_SET);
            }
        }

        delete p;
        return true;
    }

    // discard teletext / signalinfo packets if the buffer fills up, ...
    if(size() > (m_queueSize / 2)) {
        if(content == StreamInfo::scTELETEXT || content == StreamInfo::scNONE) {
            delete p;
            m_cond.Signal();
            return true;
        }
    }

    // add packet to queue
    push(p);

    // queue too long ?
    while(size() > m_queueSize) {
        p = front();
        pop();
    }

    m_cond.Signal();

    return true;
}

void LiveQueue::Action() {
    INFOLOG("LiveQueue started");

    // wait for first packet
    m_cond.Wait(0);

    while(Running()) {
        MsgPacket* p = NULL;

        m_lock.Lock();

        // just wait if we are paused
        if(m_pause) {
            m_lock.Unlock();
            m_cond.Wait(0);
            m_lock.Lock();
        }

        // check packet queue
        if(size() > 0) {
            p = front();
        }

        m_lock.Unlock();

        // no packets to send
        if(p == NULL) {
            m_cond.Wait(3000);
            continue;
        }
        // send packet
        else if(p->write(m_socket, 1000)) {
            pop();
            delete p;
        }

    }

    INFOLOG("LiveQueue stopped");
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

bool LiveQueue::pause(bool on) {
    cMutexLock lock(&m_lock);

    // deactivate timeshift
    if(!on) {
        m_pause = false;
        m_cond.Signal();
        return true;
    }

    if(m_pause) {
        return false;
    }

    // create offline storage
    if(m_readFd == -1) {
        m_storage = cString::sprintf("%s/robotv-ringbuffer-%05i.data", (const char*)m_timeShiftDir, m_socket);
        DEBUGLOG("FILE: %s", (const char*)m_storage);

        m_readFd = open(m_storage, O_CREAT | O_RDONLY, 0644);
        m_writeFd = open(m_storage, O_CREAT | O_WRONLY, 0644);
        lseek(m_readFd, 0, SEEK_SET);
        lseek(m_writeFd, 0, SEEK_SET);

        if(m_readFd == -1) {
            ERRORLOG("Failed to create timeshift ringbuffer !");
        }
    }

    m_pause = true;

    // push all packets from the queue to the offline storage
    DEBUGLOG("Writing %i packets into timeshift buffer", size());

    while(!empty()) {
        MsgPacket* p = front();

        if(p->write(m_writeFd, 1000)) {
            delete p;
            pop();
        }
    }

    return true;
}

void LiveQueue::setTimeShiftDir(const cString& dir) {
    m_timeShiftDir = dir;
    DEBUGLOG("TIMESHIFTDIR: %s", (const char*)m_timeShiftDir);
}

void LiveQueue::setBufferSize(uint64_t s) {
    m_bufferSize = s;
    DEBUGLOG("BUFFSERIZE: %llu bytes", m_bufferSize);
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
