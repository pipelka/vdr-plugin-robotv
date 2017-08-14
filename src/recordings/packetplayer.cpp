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

#include <live/livestreamer.h>
#include <tools/time.h>
#include "packetplayer.h"

#define MIN_PACKET_SIZE (128 * 1024)

PacketPlayer::PacketPlayer(cRecording* rec) : RecPlayer(rec) {
    m_index = new cIndexFile(rec->FileName(), false);
    m_recording = rec;
    m_position = 0;

    // initial start / end time
    m_startTime = std::chrono::milliseconds(0);
    m_endTime = std::chrono::milliseconds(0);

    // allocate buffer
    m_buffer = (uint8_t*)malloc(TS_SIZE * maxPacketCount);
}

PacketPlayer::~PacketPlayer() {
    clearQueue();
    free(m_buffer);
    delete m_index;
}

void PacketPlayer::onPacket(MsgPacket* p, StreamInfo::Content content, int64_t pts) {
    m_queue.push_back(p);
}

int64_t PacketPlayer::getCurrentTime(TsDemuxer::StreamPacket *p) {
    // recheck recording duration
    if((p->frameType == StreamInfo::FrameType::IFRAME) || endTime().count() == 0) {
        if(startTime().count() == 0) {
            m_startTime = roboTV::currentTimeMillis();
        }

        update();
        m_endTime = m_startTime + std::chrono::milliseconds(m_recording->LengthInSeconds() * 1000);
    }

    int64_t durationMs = (m_recording->LengthInSeconds() * 1000 * p->streamPosition) / m_totalLength;
    return m_startTime.count() + durationMs;
}

MsgPacket* PacketPlayer::getNextPacket() {

    // check packet queue first
    if(!m_queue.empty()) {
        MsgPacket* packet = m_queue.front();
        m_queue.pop_front();

        return packet;
    }

    unsigned char* p = m_buffer;

    // get next block (TS packets)
    int bytesRead = getBlock(p, m_position, maxPacketCount * TS_SIZE);

    // TS sync
    int offset = 0;
    while(offset < bytesRead && *p != TS_SYNC_BYTE) {
        p++;
        offset++;
    }

    // skip bytes (maybe) until next sync
    bytesRead -= offset;
    m_position += offset;

    // continue from current position
    if(offset > 0) {
        isyslog("PacketPlayer: skipping %i bytes", offset);
        return nullptr;
    }

    // we need at least one TS packet
    if(bytesRead < TS_SIZE) {
        isyslog("PacketPlayer: packet (%i bytes) smaller than TS packet size - retrying", bytesRead);
        return nullptr;
    }

    // round to TS_SIZE border
    int count = (bytesRead / TS_SIZE);
    int bufferSize = TS_SIZE * count;

    // advance to next block
    m_position += bufferSize;

    for(int i = 0; i < count; i++) {
        putTsPacket(p, m_position);
        p += TS_SIZE;
    }

    // currently there isn't any packet available
    return nullptr;
}

MsgPacket* PacketPlayer::getPacket() {
    if(m_position >= m_totalLength) {
        dsyslog("PacketPlayer: end of file reached (position=%ld / total=%ld)", m_position, m_totalLength);
        // TODO - send end of stream packet
        return nullptr;
    }

    MsgPacket* p = nullptr;

    // process data until the next packet drops out
    while(m_position < m_totalLength && p == nullptr) {
        p = getNextPacket();
    }

    return p;
}

MsgPacket* PacketPlayer::requestPacket() {
    MsgPacket* p = nullptr;

    // create payload packet
    if(m_streamPacket == nullptr) {
        m_streamPacket = new MsgPacket();
        m_streamPacket->disablePayloadCheckSum();
    }

    while((p = getPacket()) != nullptr) {

        // add start / endtime
        if(m_streamPacket->eop()) {
            m_streamPacket->put_S64(startTime().count());
            m_streamPacket->put_S64(endTime().count());
        }

        // add data
        m_streamPacket->put_U16(p->getMsgID());
        m_streamPacket->put_U16(p->getClientID());

        // add payload
        uint8_t* data = p->getPayload();
        uint32_t length = p->getPayloadLength();
        m_streamPacket->put_Blob(data, length);

        delete p;

        // send payload packet if it's big enough
        if(m_streamPacket->getPayloadLength() >= MIN_PACKET_SIZE) {
            MsgPacket* result = m_streamPacket;
            m_streamPacket = nullptr;
            return result;
        }
    }

    dsyslog("PacketPlayer: requestPacket didn't get any packet !");
    return nullptr;
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
    StreamPacketProcessor::reset();

    // reset current stream packet
    delete m_streamPacket;
    m_streamPacket = nullptr;

    // remove pending packets
    clearQueue();
}

int64_t PacketPlayer::filePositionFromClock(int64_t wallclockTimeMs) {
    int64_t durationSinceStartMs = wallclockTimeMs - startTime().count();
    int64_t durationMs = endTime().count() - startTime().count();

    return (m_totalLength * durationSinceStartMs) / durationMs;
}

int64_t PacketPlayer::seek(int64_t wallclockTimeMs) {
    // adujst position to TS packet borders
    m_position = filePositionFromClock(wallclockTimeMs);

    // invalid position ?
    if(m_position >= m_totalLength) {
        return 0;
    }

    if(m_position < 0) {
        m_position = 0;
    }

    isyslog("seek: %lu / %lu (%lu)", m_position, m_totalLength, wallclockTimeMs / 1000);

    // reset parser
    reset();
    return 0;
}

