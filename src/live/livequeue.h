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

#ifndef ROBOTV_LIVEQUEUE_H
#define ROBOTV_LIVEQUEUE_H

#include <queue>
#include <vdr/thread.h>
#include "demuxer/streaminfo.h"
#include <chrono>
#include <mutex>
#include <list>

class MsgPacket;

class LiveQueue {
public:

    LiveQueue(int s);

    virtual ~LiveQueue();

    bool add(MsgPacket* p, StreamInfo::Content content, bool keyFrame, int64_t pts = 0);

    MsgPacket* request();

    bool pause(bool on = true);

    bool isPaused();

    static void setTimeShiftDir(const cString& dir);

    static void setBufferSize(uint64_t s);

    static void removeTimeShiftFiles();

    void cleanup();

    int64_t seek(int64_t wallclockPositionMs);

    int64_t getTimeshiftStartPosition();

    static std::chrono::milliseconds currentTimeMillis();

protected:

    struct PacketIndex {
        off_t filePosition;
        std::chrono::milliseconds wallclockTime;
        int64_t pts;
    };

    void close();

    void removeUpToFileposition(off_t position);

    std::list<struct PacketIndex> m_indexList;

    int m_socket;

    int m_readFd;

    int m_writeFd;

    bool m_pause;

    std::mutex m_mutex;

    cString m_storage;

    std::chrono::milliseconds m_queueStartTime;

    bool m_wrapped = false;

    bool m_hasWrapped = false;

    static cString m_timeShiftDir;

    static uint64_t m_bufferSize;
};

#endif // ROBOTV_LIVEQUEUE_H
