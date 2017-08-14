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

#ifndef ROBOTV_STREAMPACKETPROCESSOR_H
#define ROBOTV_STREAMPACKETPROCESSOR_H

#include <stdint.h>
#include <demuxer/include/robotvdmx/demuxer.h>
#include <demuxer/include/robotvdmx/demuxerbundle.h>
#include <net/msgpacket.h>
#include <vdr/remux.h>
#include <deque>

class StreamPacketProcessor : protected TsDemuxer::Listener {
public:

    StreamPacketProcessor();

    virtual ~StreamPacketProcessor() = default;

    /**
     * Put a TS packet.
     * Processes a single transport stream packet.
     * @param data pointer to the TS packet
     * @param position an optional position that will be passed through to the resulting StreamPacket
     * @return true on success, false if the processor was unable to process the packet.
     */
    bool putTsPacket(uint8_t* data, int64_t position = 0);

    /**
     * Reset the packet processor.
     * This function resets the internal state of the processor. Should be called
     * on any seek operation.
     */
    virtual void reset();

protected:

    /**
     * Get presentation time of a StreamPacket.
     * Returns the wallclock time of the packet in milliseconds
     * @param p pointer to StreamPacket
     * @return wallclock time in milliseconds of the packet
     */
    virtual int64_t getCurrentTime(TsDemuxer::StreamPacket *p) = 0;

    /**
     * Notification when MsgPacket is ready.
     * Is function is called whenever a packet is ready.
     * @param p pointer to MsgPacket
     */
    virtual void onPacket(MsgPacket* p, StreamInfo::Content content, int64_t pts) = 0;

    void onStreamPacket(TsDemuxer::StreamPacket *p);

    void onStreamChange();

    StreamBundle createFromPatPmt(const cPatPmtParser* patpmt);

    virtual MsgPacket* createStreamChangePacket(DemuxerBundle& bundle);

    inline DemuxerBundle& getDemuxers() {
        return m_demuxers;
    }
private:

    void cleanupQueue();

    cPatPmtParser m_parser;

    DemuxerBundle m_demuxers;

    int m_patVersion;

    int m_pmtVersion;

    bool m_requestStreamChange;

    std::deque<MsgPacket*> m_preQueue;
};


#endif // ROBOTV_STREAMPACKETPROCESSOR_H
