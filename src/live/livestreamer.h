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

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/receiver.h>
#include <vdr/thread.h>
#include <vdr/ringbuffer.h>

#include "demuxer/demuxer.h"
#include "demuxer/streambundle.h"
#include "demuxer/demuxerbundle.h"
#include "robotv/robotvcommand.h"

#include <list>
#include <mutex>

class cChannel;
class TsDemuxer;
class MsgPacket;
class LiveQueue;
class RoboTvClient;

class LiveStreamer : public cThread, public cRingBufferLinear, public cReceiver, public TsDemuxer::Listener {
private:
    friend class TsDemuxer;
    friend class ChannelCache;

    void detach(void);

    bool attach(void);

    TsDemuxer* findDemuxer(int Pid);

    void reorderStreams(int lang, StreamInfo::Type type);

    void sendStreamChange();

    void sendStatus(int status);

    void sendDetach();

    cDevice* m_device = NULL;              /*!> The receiving device the channel depents to */

    DemuxerBundle m_demuxers;

    bool m_startup;

    bool m_requestStreamChange;

    uint32_t m_scanTimeout;                  /*!> Channel scanning timeout (in seconds) */

    cTimeMs m_lastTick;

    bool m_signalLost;

    int m_languageIndex;

    StreamInfo::Type m_langStreamType;

    LiveQueue* m_queue = NULL;

    uint32_t m_uid;

    bool m_ready;

    uint32_t m_protocolVersion;

    bool m_waitForKeyFrame;

    RoboTvClient* m_parent;

    bool m_rawPTS;

    std::mutex m_mutex;

protected:

    void Action(void);

#if VDRVERSNUM < 20300
    void Receive(uchar* Data, int Length);
#else
    void Receive(const uchar* Data, int Length);
#endif

private:

    int switchChannel(const cChannel* channel);

    void tryChannelSwitch();

    void createDemuxers(StreamBundle* bundle);

public:

    LiveStreamer(RoboTvClient* parent, const cChannel* channel, int priority, bool rawPTS = false);

    virtual ~LiveStreamer();

    bool isStarting() const {
        return m_startup;
    }

    void processChannelChange(const cChannel* Channel);

    bool isPaused();

    bool getTimeShiftMode();

    void setLanguage(int lang, StreamInfo::Type streamtype = StreamInfo::stAC3);

    void setTimeout(uint32_t timeout);

    void setProtocolVersion(uint32_t protocolVersion);

    void setWaitForKeyFrame(bool waitForKeyFrame);

    void pause(bool on);

    void requestPacket();

    void requestSignalInfo();

    // TsDemuxer::Listener implementation

    void sendStreamPacket(StreamPacket* pkt);

    void requestStreamChange();

};

#endif  // ROBOTV_RECEIVER_H
