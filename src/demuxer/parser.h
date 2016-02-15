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

#ifndef ROBOTV_DEMUXER_BASE_H
#define ROBOTV_DEMUXER_BASE_H

#include "demuxer.h"
#include "vdr/ringbuffer.h"

class Parser : public cRingBufferLinear {
public:

    Parser(TsDemuxer* demuxer, int buffersize = 64 * 1024, int packetsize = 4096);

    virtual ~Parser();

    virtual void Parse(unsigned char* data, int size, bool pusi);

protected:

    int ParsePESHeader(uint8_t* buf, size_t len);

    virtual void SendPayload(unsigned char* payload, int length);

    virtual int ParsePayload(unsigned char* payload, int length);

    virtual bool CheckAlignmentHeader(unsigned char* buffer, int& framesize);

    int FindStartCode(unsigned char* buffer, int buffersize, int offset, uint32_t startcode, uint32_t mask = 0xFFFFFFFF);

    TsDemuxer* m_demuxer;

    int64_t m_curPTS;
    int64_t m_curDTS;

    int m_samplerate;
    int m_bitrate;
    int m_channels;
    int m_duration;
    int m_headersize;
    StreamInfo::FrameType m_frametype;

    bool m_startup;

private:

    int64_t m_lastPTS;
    int64_t m_lastDTS;

    void PutData(unsigned char* data, int size, bool pusi);

    int FindAlignmentOffset(unsigned char* buffer, int buffersize, int startoffset, int& framesize);

};

#endif // ROBOTV_DEMUXER_BASE_H
