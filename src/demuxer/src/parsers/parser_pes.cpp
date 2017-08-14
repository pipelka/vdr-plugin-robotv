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

#include "robotvdmx/pes.h"

#include "parser_pes.h"

ParserPes::ParserPes(TsDemuxer* demuxer, int buffersize) : Parser(demuxer, buffersize, 0) {
    m_startup = true;
}

void ParserPes::parse(unsigned char* data, int size, bool pusi) {

    // packet completely assembled ?
    if(!m_startup) {
        int length = 0;
        uint8_t* buffer = get(length);

        if(pusi && buffer != NULL) {
            // parse payload
            if(length > 0) {
                int len = parsePayload(buffer, length);

                // send payload data
                sendPayload(buffer, len);
            }

            m_curDts = DVD_NOPTS_VALUE;
            m_curPts = DVD_NOPTS_VALUE;
        }
    }

    // new packet
    if(pusi) {
        // strip PES header
        int offset = parsePesHeader(data, size);
        data += offset;
        size -= offset;
        m_startup = false;

        // reset buffer
        clear();
    }

    // we start with the beginning of a packet
    if(!m_startup) {
        put(data, size);
    }
}
