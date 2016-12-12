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

#include <vdr/remux.h>

#include "config/config.h"
#include "live/livestreamer.h"
#include "parser.h"
#include "pes.h"
#include "demuxer_LATM.h"
#include "demuxer_ADTS.h"
#include "demuxer_AC3.h"
#include "demuxer_H264.h"
#include "demuxer_H265.h"
#include "demuxer_MPEGAudio.h"
#include "demuxer_MPEGVideo.h"
#include "demuxer_Subtitle.h"

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
        case stMPEG2VIDEO:
            return new ParserMpeg2Video(this);

        case stH264:
            return new ParserH264(this);

        case stH265:
            return new ParserH265(this);

        case stMPEG2AUDIO:
            return new ParserMpeg2Audio(this);

        case stAAC:
            return new ParserAdts(this);

        case stLATM:
            return new ParserLatm(this);

        case stAC3:
        case stEAC3:
            return new ParserAc3(this);

        case stTELETEXT:
            m_parsed = true;
            return new ParserPes(this);

        case stDVBSUB:
            return new ParserSubtitle(this);

        default:
            esyslog("Unrecognized type %i", m_type);
            m_content = scNONE;
            m_type = stNONE;
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
    // raw pts/dts
    pkt->rawDts = pkt->dts;
    pkt->rawPts = pkt->pts;

    int64_t dts = (pkt->dts == DVD_NOPTS_VALUE) ? pkt->dts : rescale(pkt->dts);
    int64_t pts = (pkt->pts == DVD_NOPTS_VALUE) ? pkt->pts : rescale(pkt->pts);

    // Rescale
    pkt->type     = m_type;
    pkt->content  = m_content;
    pkt->pid      = getPid();
    pkt->dts      = dts;
    pkt->pts      = pts;
    pkt->duration = rescale(pkt->duration);

    m_streamer->sendStreamPacket(pkt);
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
        esyslog("transport error");
        return false;
    }

    if(!TsHasPayload(data)) {
        dsyslog("no payload, size %d", bytes);
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

void TsDemuxer::setVideoInformation(int FpsScale, int FpsRate, int Height, int Width, int Aspect, int num, int den) {
    // check for sane picture information
    if(Width < 320 || Height < 240 || num <= 0 || den <= 0 || Aspect < 0) {
        return;
    }

    // only register changed video information
    if(Width == m_width && Height == m_height && Aspect == m_aspect && FpsScale == m_fpsScale && FpsRate == m_fpsRate) {
        return;
    }

    isyslog("--------------------------------------");
    isyslog("NEW PICTURE INFORMATION:");
    isyslog("Picture Width: %i", Width);
    isyslog("Picture Height: %i", Height);

    if(num != 1 || den != 1) {
        isyslog("Pixel Aspect: %i:%i", num, den);
    }

    if(Aspect == 0) {
        isyslog("Unknown Display Aspect Ratio");
    }
    else {
        isyslog("Display Aspect Ratio: %.2f", (double)Aspect / 10000);
    }

    if(FpsScale != 0 && FpsRate != 0) {
        isyslog("Frames per second: %.2lf", (double)FpsRate / (double)FpsScale);
    }

    isyslog("--------------------------------------");

    m_fpsScale = FpsScale;
    m_fpsRate  = FpsRate;
    m_height   = Height;
    m_width    = Width;
    m_aspect   = Aspect;
    m_parsed   = true;

    m_streamer->requestStreamChange();
}

void TsDemuxer::setAudioInformation(int Channels, int SampleRate, int BitRate, int BitsPerSample, int BlockAlign) {
    // only register changed audio information
    if(Channels == m_channels && SampleRate == m_sampleRate && BitRate == m_bitRate) {
        return;
    }

    isyslog("--------------------------------------");
    isyslog("NEW AUDIO INFORMATION:");
    isyslog("Channels: %i", Channels);
    isyslog("Samplerate: %i Hz", SampleRate);

    if(BitRate > 0) {
        isyslog("Bitrate: %i bps", BitRate);
    }

    isyslog("--------------------------------------");

    m_channels      = Channels;
    m_sampleRate    = SampleRate;
    m_blockAlign    = BlockAlign;
    m_bitRate       = BitRate;
    m_bitsPerSample = BitsPerSample;
    m_parsed        = true;

    m_streamer->requestStreamChange();
}

void TsDemuxer::setVideoDecoderData(uint8_t* sps, int spsLength, uint8_t* pps, int ppsLength, uint8_t* vps, int vpsLength) {
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
