/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
 *
 *      Copyright (C) 2015 Alexander Pipelka
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

class PacketPlayer : public RecPlayer, protected TsDemuxer::Listener {
public:

    PacketPlayer(cRecording* rec);

    virtual ~PacketPlayer();

    MsgPacket* getPacket();

    int64_t seek(uint64_t position);

protected:

    MsgPacket* getNextPacket();

    void sendStreamPacket(StreamPacket* p);

    void requestStreamChange();

    void clearQueue();

    void reset();

private:

    cPatPmtParser m_parser;

    DemuxerBundle m_demuxers;

    uint64_t m_position = 0;

    bool m_requestStreamChange;

    bool m_firstKeyFrameSeen;

    int m_patVersion = -1;

    int m_pmtVersion = -1;

    std::deque<MsgPacket*> m_queue;

};

#endif	// ROBOTV_PACKETPLAYER_H
