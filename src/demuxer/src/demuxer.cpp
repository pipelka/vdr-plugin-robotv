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

#include <stdint.h>
#include <cstring>

#include "robotvdmx/pes.h"

#include "parsers/parser.h"
#include "parsers/parser_latm.h"
#include "parsers/parser_adts.h"
#include "parsers/parser_ac3.h"
#include "parsers/parser_h264.h"
#include "parsers/parser_h265.h"
#include "parsers/parser_mpegaudio.h"
#include "parsers/parser_mpegvideo.h"
#include "parsers/parser_subtitle.h"

#define DVD_TIME_BASE 1000000

TsDemuxer::TsDemuxer(TsDemuxer::Listener* streamer, const StreamInfo& info) : StreamInfo(info), m_streamer(streamer) {
    m_pesParser = createParser(m_type);
    setContent();
}

TsDemuxer::TsDemuxer(TsDemuxer::Listener* streamer, StreamInfo::Type type, int pid) : StreamInfo(pid, type), m_streamer(streamer) {
    m_pesParser = createParser(m_type);
}

Parser* TsDemuxer::createParser(StreamInfo::Type type) {
    switch(type) {
        case Type::MPEG2VIDEO:
            return new ParserMpeg2Video(this);

        case Type::H264:
            return new ParserH264(this);

        case Type::H265:
            return new ParserH265(this);

        case Type::MPEG2AUDIO:
            return new ParserMpeg2Audio(this);

        case Type::AAC:
            return new ParserAdts(this);

        case Type::LATM:
            return new ParserLatm(this);

        case Type::AC3:
        case Type::EAC3:
            return new ParserAc3(this);

        case Type::TELETEXT:
            m_parsed = true;
            return new ParserPes(this);

        case Type::DVBSUB:
            return new ParserSubtitle(this);

        default:
            m_content = Content::NONE;
            m_type = Type::NONE;
            break;
    }

    return NULL;
}

TsDemuxer::~TsDemuxer() {
    delete m_pesParser;
}

int64_t TsDemuxer::rescale(int64_t a) {
    uint64_t b = DVD_TIME_BASE;
    uint64_t c = 90000;

    return (a * b) / c;
}

void TsDemuxer::sendPacket(StreamPacket* pkt) {
    pkt->type = m_type;
    pkt->content = m_content;
    pkt->pid  = getPid();
    pkt->duration = (int)rescale(pkt->duration);
    pkt->streamPosition = m_streamPosition;

    m_streamer->onStreamPacket(pkt);
}

bool TsDemuxer::processTsPacket(unsigned char* data) const {
    if(data == NULL) {
        return false;
    }

    bool pusi  = TsPayloadStart(data);

    int bytes = TS_SIZE - TsPayloadOffset(data);

    if(bytes < 0 || bytes >= TS_SIZE) {
        return false;
    }

    if(TsIsScrambled(data)) {
        return false;
    }

    if(TsError(data)) {
        return false;
    }

    if(!TsHasPayload(data)) {
        return true;
    }

    // strip ts header
    data += TS_SIZE - bytes;

    // valid packet ?
    if(pusi && !PesIsHeader(data)) {
        return false;
    }

    /* Parse the data */
    if(m_pesParser) {
        m_pesParser->parse(data, bytes, pusi);
    }

    return true;
}

void TsDemuxer::setVideoInformation(int fpsScale, int fpsRate, uint16_t height, uint16_t width, int aspect) {
    // check for sane picture information
    if(width < 320 || height < 240 || aspect < 0) {
        return;
    }

    // only register changed video information
    if(width == m_width && height == m_height && aspect == m_aspect && fpsScale == m_fpsScale && fpsRate == m_fpsRate) {
        return;
    }

    m_fpsScale = fpsScale;
    m_fpsRate  = fpsRate;
    m_height   = height;
    m_width    = width;
    m_aspect   = aspect;
    m_parsed   = true;

    m_streamer->onStreamChange();
}

void TsDemuxer::setAudioInformation(uint8_t channels, uint32_t sampleRate, uint32_t bitRate) {
    // only register changed audio information
    if(channels == m_channels && sampleRate == m_sampleRate && bitRate == m_bitRate) {
        return;
    }

    m_channels      = channels;
    m_sampleRate    = sampleRate;
    m_bitRate       = bitRate;
    m_parsed        = true;

    m_streamer->onStreamChange();
}

void TsDemuxer::setVideoDecoderData(uint8_t* sps, size_t spsLength, uint8_t* pps, size_t ppsLength, uint8_t* vps, size_t vpsLength) {
    if(sps != NULL && spsLength <= sizeof(m_sps)) {
        m_spsLength = spsLength;
        memcpy(m_sps, sps, spsLength);
    }

    if(pps != NULL && ppsLength <= sizeof(m_pps)) {
        m_ppsLength = ppsLength;
        memcpy(m_pps, pps, ppsLength);
    }

    if(vps != NULL && vpsLength <= sizeof(m_vps)) {
        m_vpsLength = vpsLength;
        memcpy(m_vps, vps, vpsLength);
    }
}

uint8_t* TsDemuxer::getVideoDecoderSps(int& length) {
    length = m_spsLength;
    return m_spsLength == 0 ? NULL : m_sps;
}

uint8_t* TsDemuxer::getVideoDecoderPps(int& length) {
    length = m_ppsLength;
    return m_ppsLength == 0 ? NULL : m_pps;
}

uint8_t* TsDemuxer::getVideoDecoderVps(int& length) {
    length = m_vpsLength;
    return m_vpsLength == 0 ? NULL : m_vps;
}
