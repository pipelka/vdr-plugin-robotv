/*
 * bitstream.cpp: Bitstream parsing
 *
 * this class was shamelessly stolen and refactored from the VDR project
 *
 * Copyright (C) 2000, 2003, 2006, 2008, 2013 Klaus Schmidinger
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * The author can be reached at vdr@tvdr.de
 *
 * The project's page is at http://www.tvdr.de
 */

#include "bitstream.h"

int BitStream::getBit(void) {
    if(m_index >= m_length) {
        return 1;
    }

    int r = (m_data[m_index >> 3] >> (7 - (m_index & 7))) & 1;
    ++m_index;
    return r;
}

uint32_t BitStream::getBits(int n) {
    uint32_t r = 0;

    while(n--) {
        r |= getBit() << n;
    }

    return r;
}

void BitStream::byteAlign(void) {
    int n = m_index % 8;

    if(n > 0) {
        skipBits(8 - n);
    }
}

void BitStream::wordAlign(void) {
    int n = m_index % 16;

    if(n > 0) {
        skipBits(16 - n);
    }
}

bool BitStream::setLength(int Length) {
    if(Length > m_length) {
        return false;
    }

    m_length = Length;
    return true;
}
