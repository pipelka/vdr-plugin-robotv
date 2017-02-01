/*
 * ringbuffer.cpp: A ring buffer
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

#include "ringbuffer.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

RingBuffer::RingBuffer(int size, int margin) {
    m_size = size;
    m_tail = m_head = m_margin = margin;
    m_gotten = 0;
    m_buffer = NULL;

    if(size > 1) {  // 'Size - 1' must not be 0!
        if(margin <= size / 2) {
            m_buffer = (uint8_t*)malloc((size_t)size);
            clear();
        }
    }
}

RingBuffer::~RingBuffer() {
    ::free(m_buffer);
}

int RingBuffer::onDataReady(const uint8_t* data, int count) {
    return count >= m_margin ? count : 0;
}

int RingBuffer::available(void) const {
    int diff = m_head - m_tail;
    return (diff >= 0) ? diff : size() + diff - m_margin;
}

void RingBuffer::clear(void) {
    m_tail = m_head = m_margin;
}

int RingBuffer::put(const uint8_t *data, int count) {
    if(count <= 0) {
        return count;
    }

    int Tail = m_tail;
    int rest = size() - m_head;
    int diff = Tail - m_head;
    int free = ((Tail < m_margin) ? rest : (diff > 0) ? diff : size() + diff - m_margin) - 1;

    if(free > 0) {
        if(free < count) {
            count = free;
        }

        if(count >= rest) {
            memcpy(m_buffer + m_head, data, (size_t)rest);

            if(count - rest) {
                memcpy(m_buffer + m_margin, data + rest, (size_t)count - rest);
            }

            m_head = m_margin + count - rest;
        }
        else {
            memcpy(m_buffer + m_head, data, (size_t)count);
            m_head += count;
        }
    }
    else {
        count = 0;
    }

    return count;
}

uint8_t* RingBuffer::get(int &count) {
    int Head = m_head;
    int rest = size() - m_tail;

    if(rest < m_margin && Head < m_tail) {
        int t = m_margin - rest;
        memcpy(m_buffer + t, m_buffer + m_tail, (size_t)rest);
        m_tail = t;
        rest = Head - m_tail;
    }

    int diff = Head - m_tail;
    int cont = (diff >= 0) ? diff : size() + diff - m_margin;

    if(cont > rest) {
        cont = rest;
    }

    uint8_t* p = m_buffer + m_tail;

    if((cont = onDataReady(p, cont)) > 0) {
        count = m_gotten = cont;
        return p;
    }

    return nullptr;
}

void RingBuffer::del(int count) {
    if(count > m_gotten) {
        count = m_gotten;
    }

    if(count <= 0) {
        return;
    }

    int tail = m_tail;
    tail += count;
    m_gotten -= count;

    if(tail >= size()) {
        tail = m_margin;
    }

    m_tail = tail;
}
