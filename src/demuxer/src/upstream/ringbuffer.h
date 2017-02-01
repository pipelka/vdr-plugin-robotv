/*
 * ringbuffer.h: A ring buffer
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

#ifndef ROBOTV_RINGBUFFER_H
#define ROBOTV_RINGBUFFER_H

#include <ctime>
#include <cstdint>

class RingBuffer {
private:
    int m_size;
    int m_margin;
    int m_head;
    int m_tail;
    int m_gotten;
    uint8_t* m_buffer;

protected:
    int size(void) const {
        return m_size;
    }

    /**
     * By default a ring buffer has data ready as soon as there are at least
     * 'margin' bytes available. A derived class can reimplement this function
     * if it has other conditions that define when data is ready.
     * The return value is either 0 if there is not yet enough data available,
     * or the number of bytes from the beginning of data that are "ready".
     *
     * @param data pointer to data
     * @param count number of bytes
     * @return mber of bytes from the beginning of data that are "ready"
     */
    virtual int onDataReady(const uint8_t* data, int count);

public:
    /**
     * Creates a linear ring buffer.
     * The buffer will be able to hold at most size-margin-1 bytes of data, and will
     * be guaranteed to return at least margin bytes in one consecutive block.
     *
     * @param size total size of the buffer
     * @param margin block size
     */
    RingBuffer(int size, int margin = 0);

    virtual ~RingBuffer();

    int available(void) const;

    int free(void) const {
        return size() - available() - 1 - m_margin;
    }

    /**
     * Immediately clears the ring buffer.
     */
    void clear();

    /**
     * Puts at most count bytes of data into the ring buffer.
     * Returns the number of bytes actually stored.
     *
     * @param data pointer to data
     * @param count number of bytes to put
     * @return number of bytes actually stored
     */
    int put(const uint8_t *data, int count);

    /**
     * Gets data from the ring buffer.
     * The data will remain in the buffer until a call to del() deletes it.
     * Returns a pointer to the data, and stores the number of bytes
     * actually available in count. If the returned pointer is nullptr, count has no meaning.
     *
     * @param count number of bytes to get
     * @return pointer to the data
     */
    uint8_t* get(int &count);

    /**
     * Deletes at most Count bytes from the ring buffer.
     * count must be less or equal to the number that was returned by a previous
     * call to get().
     *
     * @param count number of bytes to remove from the buffer
     */
    void del(int count);
};

#endif // ROBOTV_RINGBUFFER_H
