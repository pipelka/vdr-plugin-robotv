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

#include "robotvdmx/aaccommon.h"
#include "upstream/bitstream.h"

#include "parser_latm.h"

ParserLatm::ParserLatm(TsDemuxer* demuxer) : Parser(demuxer, 64 * 1024, 8192) { //, m_framelength(0)
}

bool ParserLatm::checkAlignmentHeader(unsigned char* buffer, int& framesize, bool parse) {
    BitStream bs(buffer, 24 * 8);

    // read sync
    if(bs.getBits(11) != 0x2B7) {
        return false;
    }

    // read frame size
    framesize = bs.getBits(13) + 3;

    if(!bs.getBit()) {
        readStreamMuxConfig(&bs);
    }

    if(parse) {
        m_demuxer->setAudioInformation(m_channels, m_sampleRate, 0);
    }

    return true;
}

void ParserLatm::readStreamMuxConfig(BitStream *bs)  {
    int audioMuxVersion = bs->getBit();

    if(audioMuxVersion != 0) {
        return;
    }

    bs->skipBits(1);    // allStreamSameTimeFraming = 1
    bs->skipBits(6);    // numSubFrames = 0
    bs->skipBits(4);    // numPrograms = 0
    bs->skipBits(3);    // numLayer = 0

    auto aot = bs->getBits(5);
    if(aot == 31) {
        bs->skipBits(6);
    }

    auto sampleRateIndex = bs->getBits(4);

    if(sampleRateIndex == 0xf) {
        m_sampleRate = bs->getBits(24);
    }
    else {
        m_sampleRate = aac_samplerates[sampleRateIndex];
    }

    auto channelIndex = bs->getBits(4);

    if(channelIndex < 8) {
        m_channels = aac_channels[channelIndex];
    }

    m_duration = 1024 * 90000 / m_sampleRate;
}
