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

#ifndef ROBOTV_PACKETPLAYER_H
#define	ROBOTV_PACKETPLAYER_H

#include "recordings/recplayer.h"
#include "demuxer/demuxer.h"
#include "demuxer/demuxerbundle.h"
#include "net/msgpacket.h"

#include "vdr/remux.h"
#include <deque>
#include <chrono>

class PacketPlayer : public RecPlayer, protected TsDemuxer::Listener {
public:

    PacketPlayer(cRecording* rec);

    virtual ~PacketPlayer();

    MsgPacket* requestPacket(bool keyFrameMode);

    int64_t seek(int64_t position);

    const std::chrono::milliseconds& startTime() const {
        return m_startTime;
    }

    const std::chrono::milliseconds& endTime() const {
        return m_endTime;
    }

protected:

    MsgPacket* getNextPacket();

    MsgPacket* getPacket();

    void sendStreamPacket(StreamPacket* p);

    void requestStreamChange();

    void clearQueue();

    void reset();

    int64_t filePositionFromClock(int64_t wallclockTimeMs);

private:

    cPatPmtParser m_parser;

    cIndexFile* m_index;

    cRecording* m_recording;

    DemuxerBundle m_demuxers;

    uint64_t m_position = 0;

    uint64_t m_positionMarker = 0;

    bool m_requestStreamChange;

    bool m_firstKeyFrameSeen;

    int m_patVersion = -1;

    int m_pmtVersion = -1;

    std::deque<MsgPacket*> m_queue;

    MsgPacket* m_streamPacket = NULL;

    std::chrono::milliseconds m_startTime;

    std::chrono::milliseconds m_endTime;

    int64_t m_startPts = 0;

};

#endif	// ROBOTV_PACKETPLAYER_H
