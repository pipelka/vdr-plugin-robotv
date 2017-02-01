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
#include <upstream/bitstream.h>
#include "robotvdmx/ac3common.h"

#include "parser_ac3.h"

ParserAc3::ParserAc3(TsDemuxer* demuxer) : Parser(demuxer, 64 * 1024, 4096) {
    m_headerSize = AC3_HEADER_SIZE;
    m_enhanced = false;
}

bool ParserAc3::checkAlignmentHeader(unsigned char* buffer, int& framesize, bool parse) {
    BitStream bs(buffer, AC3_HEADER_SIZE * 8);

    if(bs.getBits(16) != 0x0B77) {
        return false;
    }

    bs.skipBits(24); // FFWD to bsid
    int bsid = bs.getBits(5); // bsid

    m_enhanced = (bsid > 10);

    bs.reset(); // rewind to start
    bs.skipBits(16); // skip syncword

    // EAC-3
    if(m_enhanced) {
        int frametype = bs.getBits(2);

        if(frametype == EAC3_FRAME_TYPE_RESERVED) {
            return false;
        }

        bs.skipBits(3);

        framesize = (bs.getBits(11) + 1) << 1;

        if(framesize < AC3_HEADER_SIZE) {
            return false;
        }

        int numBlocks = 6;
        int sr_code = bs.getBits(2);

        if(sr_code == 3) {
            int sr_code2 = bs.getBits(2);

            if(sr_code2 == 3) {
                return false;
            }

            m_sampleRate = AC3SampleRateTable[sr_code2] / 2;
        }
        else {
            numBlocks = EAC3Blocks[bs.getBits(2)];
            m_sampleRate = AC3SampleRateTable[sr_code];
        }

        int channelMode = bs.getBits(3);
        int lfeon = bs.getBits(1);

        m_bitRate  = (uint32_t)(8.0 * framesize * m_sampleRate / (numBlocks * 256.0));
        m_channels = AC3ChannelsTable[channelMode] + lfeon;
        m_duration = (framesize * 8 * 1000 * 90) / m_bitRate;
    }

        // AC-3
    else {
        bs.skipBits(16); // CRC
        int fscod = bs.getBits(2);
        int frmsizecod = bs.getBits(6);
        bs.getBits(5); // bsid

        bs.skipBits(3); // bitstream mode
        int acmod = bs.getBits(3);

        if(fscod == 3 || frmsizecod > 37) {
            return false;
        }

        if(acmod == AC3_CHMODE_STEREO) {
            bs.skipBits(2); // skip dsurmod
        }
        else {
            if((acmod & 1) && acmod != AC3_CHMODE_MONO) {
                bs.skipBits(2);
            }

            if(acmod & 4) {
                bs.skipBits(2);
            }
        }

        int lfeon = bs.getBits(1);

        m_sampleRate = AC3SampleRateTable[fscod];
        m_bitRate = (AC3BitrateTable[frmsizecod >> 1] * 1000);
        m_channels = AC3ChannelsTable[acmod] + lfeon;

        framesize = AC3FrameSizeTable[frmsizecod][fscod] * 2;

        m_duration = (framesize * 8 * 1000 * 90) / m_bitRate;
    }

    if(parse) {
        m_demuxer->setAudioInformation(m_channels, m_sampleRate, m_bitRate);
    }

    return true;
}
