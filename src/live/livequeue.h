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

#ifndef ROBOTV_LIVEQUEUE_H
#define ROBOTV_LIVEQUEUE_H

#include <queue>
#include <vdr/thread.h>
#include "demuxer/streaminfo.h"

class MsgPacket;

class LiveQueue : public cThread, protected std::queue<MsgPacket*> {
public:

    LiveQueue(int s);

    virtual ~LiveQueue();

    bool add(MsgPacket* p, StreamInfo::Content content);

    void request();

    bool pause(bool on = true);

    bool isPaused();

    bool getTimeShiftMode();

    static void setTimeShiftDir(const cString& dir);

    static void setBufferSize(uint64_t s);

    static void removeTimeShiftFiles();

    void cleanup();

protected:

    void Action();

    void close();

    int m_socket;

    int m_readFd;

    int m_writeFd;

    bool m_pause;

    cMutex m_lock;

    cCondWait m_cond;

    cString m_storage;

    size_type m_queueSize;

    static cString m_timeShiftDir;

    static uint64_t m_bufferSize;
};

#endif // ROBOTV_LIVEQUEUE_H
