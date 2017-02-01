/*
 * bitstream.h: Bitstream parsing
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

#ifndef ROBOTV_BITSTREAM_H
#define ROBOTV_BITSTREAM_H

#include <stdint.h>

class BitStream {
public:

    BitStream(const uint8_t* data, int length) : m_data(data), m_length(length), m_index(0) {
    }

    ~BitStream() {}

    int getBit(void);

    uint32_t getBits(int n);

    void byteAlign(void);

    void wordAlign(void);

    bool setLength(int Length);

    void skipBits(int n) {
        m_index += n;
    }

    void skipBit(void) {
        skipBits(1);
    }

    bool eof(void) const {
        return m_index >= m_length;
    }

    void reset(void) {
        m_index = 0;
    }

    int length(void) const {
        return m_length;
    }

    int index(void) const {
        return (eof() ? m_length : m_index);
    }

    const uint8_t* getData(void) const {
        return (eof() ? nullptr : m_data + (m_index / 8));
    }

private:

    const uint8_t* m_data;
    int m_length; // in bits
    int m_index; // in bits
};

#endif // ROBOTV_BITSTREAM_H
