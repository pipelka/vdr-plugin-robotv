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

#include "demuxer/streaminfo.h"

#include <deque>
#include <chrono>
#include <mutex>
#include <list>
#include <thread>
#include <atomic>

class MsgPacket;

class LiveQueue {
public:

    LiveQueue(int socket);

    virtual ~LiveQueue();

    void queue(MsgPacket* p, StreamInfo::Content content, int64_t pts = 0);

    MsgPacket* read(bool keyFrameMode = false);

    int64_t seek(int64_t wallclockPositionMs);

    bool pause(bool on = true);

    bool isPaused();

    static void setTimeShiftDir(const cString& dir);

    static void setBufferSize(uint64_t s);

    static void removeTimeShiftFiles();

    int64_t getTimeshiftStartPosition();

protected:

    struct PacketData {
        MsgPacket* p;
        StreamInfo::Content content;
        int64_t pts;
    };

    struct PacketIndex {
        off_t filePosition;
        std::chrono::milliseconds wallclockTime;
        int64_t pts;
        int wrapCount;
    };

    bool write(const PacketData& data);

    void start();

    void createRingBuffer();

    void close();

    void trim(off_t position);

    MsgPacket* internalRead();

    void seekNextKeyFrame();

    std::deque<struct PacketIndex> m_indexList;

    int m_readFd;

    int m_writeFd;

    int m_socket;

    bool m_pause;

    std::mutex m_mutex;

    cString m_storage;

    std::chrono::milliseconds m_queueStartTime;

    bool m_wrapped;

    bool m_hasWrapped;

    int m_wrapCount;

    static cString m_timeShiftDir;

    static uint64_t m_bufferSize;

private:

    std::thread* m_writeThread;

    std::atomic<bool> m_writerRunning;

    std::chrono::milliseconds m_lastSyncTime;

    std::deque<PacketData> m_writerQueue;

    std::mutex m_mutexQueue;

};

#endif // ROBOTV_LIVEQUEUE_H
