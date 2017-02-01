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

#include <upstream/bitstream.h>
#include "robotvdmx/aaccommon.h"

#include "parser_adts.h"

ParserAdts::ParserAdts(TsDemuxer* demuxer) : Parser(demuxer, 64 * 1024, 8192) {
    m_headerSize = 9; // header is 9 bytes long (with CRC)
}

bool ParserAdts::ParseAudioHeader(uint8_t* buffer, int& channels, int& samplerate, int& framesize) {
    BitStream bs(buffer, m_headerSize * 8);

    // sync
    if(bs.getBits(12) != 0xFFF) {
        return false;
    }

    bs.skipBits(1); // MPEG Version (0 = MPEG4 / 1 = MPEG2)

    // layer is always 0
    if(bs.getBits(2) != 0) {
        return false;
    }

    bs.skipBits(1); // Protection absent
    bs.skipBits(2); // AOT
    int samplerateindex = bs.getBits(4); // sample rate index

    if(samplerateindex == 15) {
        return false;
    }

    bs.skipBits(1);      // Private bit

    int channelindex = bs.getBits(3); // channel index

    if(channelindex > 7) {
        return false;
    }

    bs.skipBits(4); // original, copy, copyright, ...

    framesize = bs.getBits(13);

    m_sampleRate = aac_samplerates[samplerateindex];
    m_channels = aac_channels[channelindex];
    m_duration = 1024 * 90000 / m_sampleRate;

    return true;
}

bool ParserAdts::checkAlignmentHeader(unsigned char* buffer, int& framesize, bool parse) {
    if(!ParseAudioHeader(buffer, m_channels, m_sampleRate, framesize)) {
        return false;
    }

    if(parse) {
        m_demuxer->setAudioInformation(m_channels, m_sampleRate, 0);
    }

    return true;
}
