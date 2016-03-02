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

#include "config/config.h"
#include "packetplayer.h"
#include "tools/time.h"

#define MIN_PACKET_SIZE (128 * 1024)

PacketPlayer::PacketPlayer(cRecording* rec) : RecPlayer(rec), m_demuxers(this) {
    m_requestStreamChange = true;
    m_firstKeyFrameSeen = false;
    m_index = new cIndexFile(rec->FileName(), false);
    m_recording = rec;
}

PacketPlayer::~PacketPlayer() {
    clearQueue();
    delete m_index;
}

void PacketPlayer::sendStreamPacket(StreamPacket* p) {
    // check if we've got a key frame
    if(p->content == StreamInfo::scVIDEO && p->frameType == StreamInfo::ftIFRAME && !m_firstKeyFrameSeen) {
        INFOLOG("got first key frame");
        m_firstKeyFrameSeen = true;
    }

    // skip non video / audio packets
    if(p->content != StreamInfo::scVIDEO && p->content != StreamInfo::scAUDIO) {
        m_positionMarker = m_position;
        return;
    }

    // streaming starts with a key frame
    if(!m_firstKeyFrameSeen) {
        return;
    }

    // initialise stream packet
    MsgPacket* packet = new MsgPacket(ROBOTV_STREAM_MUXPKT, ROBOTV_CHANNEL_STREAM);
    packet->disablePayloadCheckSum();

    // write stream data
    packet->put_U16(p->pid);

    packet->put_S64(p->rawPts);
    packet->put_S64(p->rawDts);
    packet->put_U32(p->duration);

    // write frame type into unused header field clientid
    packet->setClientID((uint16_t)p->frameType);

    // write payload into stream packet
    packet->put_U32(p->size);
    packet->put_Blob(p->data, p->size);
    packet->put_U64(m_position);
    packet->put_U64(m_totalLength);

    int64_t currentTime = 0;
    int64_t currentPts = p->rawPts;

    // set initial pts
    if(m_startPts == 0) {
        m_startPts = currentPts;
    }

    // pts wrap ?
    if(currentPts < m_startPts) {
        currentPts += 0x200000000ULL;
    }

    currentTime = m_startTime.count() + (currentPts - m_startPts) / 90;

    // add timestamp (wallclock time in ms starting at m_startTime)
    packet->put_S64(currentTime);

    m_queue.push_back(packet);
}

void PacketPlayer::requestStreamChange() {
    INFOLOG("stream change requested");
    m_requestStreamChange = true;
}

MsgPacket* PacketPlayer::getNextPacket() {
    int pmtVersion = 0;
    int patVersion = 0;

    int packet_count = 1;
    int packet_size = TS_SIZE * packet_count;

    unsigned char buffer[packet_size];

    // get next block (TS packets)
    if(getBlock(buffer, m_position, packet_size) != packet_size) {
        return NULL;
    }

    // advance to next block
    m_position += packet_size;

    // new PAT / PMT found ?
    if(m_parser.ParsePatPmt(buffer, TS_SIZE)) {
        m_parser.GetVersions(m_patVersion, pmtVersion);

        if(pmtVersion > m_pmtVersion) {
            INFOLOG("found new PMT version (%i)", pmtVersion);
            m_pmtVersion = pmtVersion;

            // update demuxers from new PMT
            INFOLOG("updating demuxers");
            StreamBundle streamBundle = StreamBundle::createFromPatPmt(&m_parser);
            m_demuxers.updateFrom(&streamBundle);

            m_requestStreamChange = true;
        }
    }

    // put packets into demuxer
    uint8_t* p = buffer;

    for(int i = 0; i < packet_count; i++) {
        m_demuxers.processTsPacket(p);
        p += TS_SIZE;
    }

    // stream change needed / requested
    if(m_requestStreamChange) {
        // first we need valid PAT/PMT
        if(!m_parser.GetVersions(patVersion, pmtVersion)) {
            return NULL;
        }

        // demuxers need to be ready
        if(!m_demuxers.isReady()) {
            return NULL;
        }

        INFOLOG("demuxers ready");

        for(auto i : m_demuxers) {
            i->info();
        }

        INFOLOG("create streamchange packet");
        m_requestStreamChange = false;
        return m_demuxers.createStreamChangePacket();
    }

    // get next packet from queue (if any)
    if(m_queue.size() == 0) {
        return NULL;
    }

    MsgPacket* packet = m_queue.front();
    m_queue.pop_front();

    return packet;
}

MsgPacket* PacketPlayer::getPacket() {
    MsgPacket* p = NULL;

    // process data until the next packet drops out
    while(m_position < m_totalLength && p == NULL) {
        p = getNextPacket();
    }

    return p;
}

MsgPacket* PacketPlayer::requestPacket(bool keyFrameMode) {
    MsgPacket* p = NULL;

    // initial start / end time
    if(m_startTime.count() == 0) {
        m_startTime = roboTV::currentTimeMillis();
        m_endTime = m_startTime + std::chrono::milliseconds(m_recording->LengthInSeconds() * 1000);
    }

    // create payload packet
    if(m_streamPacket == NULL) {
        m_streamPacket = new MsgPacket();
    }

    while(p = getPacket()) {

        if(keyFrameMode && p->getClientID() != StreamInfo::FrameType::ftIFRAME) {
            delete p;
            continue;
        }

        // send recording position on every keyframe
        if(p->getClientID() == StreamInfo::FrameType::ftIFRAME) {
            int64_t durationMs = (int)(((double)m_index->Last() * 1000.0) / m_recording->FramesPerSecond());
            m_endTime = m_startTime + std::chrono::milliseconds(durationMs);
        }

        // add data
        m_streamPacket->put_U16(p->getMsgID());
        m_streamPacket->put_U16(p->getClientID());

        // add payload
        uint8_t* data = p->getPayload();
        int length = p->getPayloadLength();
        m_streamPacket->put_Blob(data, length);

        delete p;

        // send payload packet if it's big enough
        if(m_streamPacket->getPayloadLength() >= MIN_PACKET_SIZE) {
            MsgPacket* result = m_streamPacket;
            m_streamPacket = NULL;

            return result;
        }
    }

    return NULL;
}

void PacketPlayer::clearQueue() {
    MsgPacket* p = NULL;

    while(m_queue.size() > 0) {
        p = m_queue.front();
        m_queue.pop_front();
        delete p;
    }
}

void PacketPlayer::reset() {
    // reset parser
    m_parser.Reset();
    m_demuxers.clear();
    m_requestStreamChange = true;
    m_firstKeyFrameSeen = false;
    m_patVersion = -1;
    m_pmtVersion = -1;

    // remove pending packets
    clearQueue();
}

int64_t PacketPlayer::filePositionFromClock(int64_t wallclockTimeMs) {
    double offsetMs = wallclockTimeMs - m_startTime.count();
    double fps = m_recording->FramesPerSecond();
    int frames = (int)((offsetMs * fps) / 1000.0);

    int index = m_index->GetClosestIFrame(frames);

    uint16_t fileNumber = 0;
    off_t fileOffset = 0;

    m_index->Get(index, &fileNumber, &fileOffset);

    if(fileNumber == 0) {
        return 0;
    }

    return m_segments[--fileNumber]->start + fileOffset;
}

int64_t PacketPlayer::seek(int64_t wallclockTimeMs) {
    // adujst position to TS packet borders
    m_position = filePositionFromClock(wallclockTimeMs);

    // invalid position ?
    if(m_position >= m_totalLength) {
        return -1;
    }

    INFOLOG("seek: %lu / %lu", m_position, m_totalLength);

    // reset parser
    reset();

    // first check for next video packet (PTS) after seek
    MsgPacket* p = NULL;
    int64_t pts = 0;

    for(;;) {
        p = getPacket();

        // exit if we have no more packets
        if(p == NULL) {
            return -1;
        }

        // get pid / pts
        int pid = p->get_U16();
        pts = p->get_U64();

        // delete packet
        delete p;

        // check for video pid
        if(pid == m_parser.Vpid()) {
            break;
        }
    }

    // reset again
    reset();
    m_position = filePositionFromClock(wallclockTimeMs);

    return pts;
}
