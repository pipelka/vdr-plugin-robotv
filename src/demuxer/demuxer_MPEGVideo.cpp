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

#include "demuxer_MPEGVideo.h"
#include "vdr/tools.h"
#include "pes.h"

#define MPEG2_SEQUENCE_START 0x000001B3
#define MPEG2_PICTURE_START  0x00000100

// frame durations
static const unsigned int framedurations[16] = {
    0, 3753, 3750, 3600, 3003, 3000, 1800, 1501, 1500, 0, 0, 0, 0, 0, 0, 0
};

// frame rates
static const unsigned int framerates[16][2] = {
    {0, 0}, {24000, 1001}, {24, 1}, {25, 1}, {30000, 1001}, {30, 1}, {50, 1}, {60000, 1001}, {60, 1},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
};

// aspect ratios
static const double aspectratios[16] = {
    0, 1.0, 1.333333333, 1.777777778, 2.21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int GetFrameType(unsigned char* data, int length) {
    cBitStream bs(data, length * 8);
    bs.SkipBits(32); // skip picture start code
    bs.SkipBits(10); // skip temporal reference

    return bs.GetBits(3);
}

static StreamInfo::FrameType ConvertFrameType(int frametype) {
    switch(frametype) {
        case 1:
            return StreamInfo::ftIFRAME;

        case 2:
            return StreamInfo::ftPFRAME;

        case 3:
            return StreamInfo::ftBFRAME;

        case 4:
            return StreamInfo::ftDFRAME;

        default:
            break;
    }

    return StreamInfo::ftUNKNOWN;
}

ParserMpeg2Video::ParserMpeg2Video(TsDemuxer* demuxer) : ParserPes(demuxer, 512 * 1024), m_frameDifference(0), m_lastDts(DVD_NOPTS_VALUE) {
}

StreamInfo::FrameType ParserMpeg2Video::parsePicture(unsigned char* data, int length) {
    int frametype = GetFrameType(data, length);

    // get I,P frames distance
    if(frametype < 3 && m_curDts != DVD_NOPTS_VALUE && m_curPts != DVD_NOPTS_VALUE) {
        m_frameDifference = m_curPts - m_curDts;
        m_lastDts = m_curDts;
        return ConvertFrameType(frametype);
    }

    // extrapolate DTS
    if(m_curDts == DVD_NOPTS_VALUE && m_duration != 0) {
        m_curDts = PtsAdd(m_lastDts, m_duration);
        m_lastDts = m_curDts;
    }

    // B frames have DTS = PTS
    if(frametype == 3 && m_curPts == DVD_NOPTS_VALUE) {
        m_curPts = m_curDts;
    }

    // extrapolate PTS of I/P frame
    if(frametype < 3 && m_curPts == DVD_NOPTS_VALUE) {
        m_curPts = PtsAdd(m_curDts, m_frameDifference);
    }

    return ConvertFrameType(frametype);
}

int ParserMpeg2Video::parsePayload(unsigned char* data, int length) {
    // lookup sequence start code
    int o = findStartCode(data, length, 0, MPEG2_SEQUENCE_START);

    if(o >= 0) {
        // skip sequence start code
        o += 4;

        // parse picture sequence (width, height, aspect, duration)
        parseSequenceStart(data + o, length - 4);
    }

    // just to be sure, exit if there's isn't any duration
    if(m_duration == 0) {
        return length;
    }

    // check for picture start codes
    int s = findStartCode(data, length, 0, MPEG2_PICTURE_START);

    // abort if there isn't any picture information
    if(s == -1) {
        return length;
    }

    int e = findStartCode(data, length, s + 4, MPEG2_PICTURE_START);
    o = s;
    s = 0;

    // divide this packet into frames
    while(e != -1) {

        // parse and send payload data
        m_frameType = parsePicture(data + o, e - o);
        Parser::sendPayload(data + s, e - s);

        // get next picture offsets
        s = e;
        o = s;
        e = findStartCode(data, length, s + 4, MPEG2_PICTURE_START);

        // increment timestamps
        m_curPts = DVD_NOPTS_VALUE;
        m_curDts = PtsAdd(m_curDts, m_duration);
    }

    // append last part
    m_frameType = parsePicture(data + o, length - o);
    Parser::sendPayload(data + s, length - s);

    return length;
}

void ParserMpeg2Video::sendPayload(unsigned char* payload, int length) {
}

void ParserMpeg2Video::parseSequenceStart(unsigned char* data, int length) {
    cBitStream bs(data, length * 8);

    if(bs.Length() < 32) {
        return;
    }

    int width  = bs.GetBits(12);
    int height = bs.GetBits(12);

    // display aspect ratio
    double DAR = aspectratios[bs.GetBits(4)];

    // frame rate / duration
    int index = bs.GetBits(4);
    m_duration = framedurations[index];

    m_demuxer->setVideoInformation(framerates[index][1], framerates[index][0], height, width, (int)(DAR * 10000), 1, 1);
}
