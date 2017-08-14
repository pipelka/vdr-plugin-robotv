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

#ifndef ROBOTV_RECEIVER_H
#define ROBOTV_RECEIVER_H

#include <stdint.h>
#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/receiver.h>
#include <vdr/ringbuffer.h>

#include "robotvdmx/demuxer.h"
#include "robotvdmx/streambundle.h"
#include "robotvdmx/demuxerbundle.h"
#include "robotv/robotvcommand.h"
#include "livequeue.h"

#include <list>
#include <mutex>
#include <robotv/StreamPacketProcessor.h>

class cChannel;
class MsgPacket;
class LiveQueue;
class RoboTvClient;

class LiveStreamer : public cReceiver, protected StreamPacketProcessor {
private:

    void sendStatus(int status);

    DemuxerBundle m_demuxers = NULL;

    LiveQueue* m_queue = NULL;

    RoboTvClient* m_parent = NULL;

    std::string m_language;

    StreamInfo::Type m_langStreamType = StreamInfo::Type::AC3;

    uint32_t m_uid;

    bool m_waitForKeyFrame = false;

    std::mutex m_mutex;

    MsgPacket* m_streamPacket = NULL;

    bool m_cacheEnabled;

protected:

#if VDRVERSNUM < 20300
    void Receive(uchar* data, int length);
#else
    void Receive(const uchar* Data, int Length);
#endif

    int64_t getCurrentTime(TsDemuxer::StreamPacket *p);

    void onPacket(MsgPacket* p, StreamInfo::Content content, int64_t pts);

    MsgPacket* createStreamChangePacket(DemuxerBundle& bundle);

private:

    StreamBundle createFromChannel(const cChannel* channel);

    void createDemuxers(StreamBundle* bundle);

public:

    LiveStreamer(RoboTvClient* parent, const cChannel* channel, int priority, bool cache);

    virtual ~LiveStreamer();

    void processChannelChange(const cChannel* Channel);

    bool isPaused();

    void setLanguage(const char* lang, StreamInfo::Type streamtype = StreamInfo::Type::AC3);

    void setWaitForKeyFrame(bool waitForKeyFrame);

    void pause(bool on);

    MsgPacket* requestPacket();

    void requestSignalInfo();

    int switchChannel(const cChannel* channel);

    int64_t seek(int64_t wallclockPositionMs);

};

#endif  // ROBOTV_RECEIVER_H
