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

#ifndef ROBOTV_DEMUXERBUNDLE_H
#define ROBOTV_DEMUXERBUNDLE_H

#include "demuxer.h"
#include "streambundle.h"

#include <list>

class DemuxerBundle {
public:

    explicit DemuxerBundle(TsDemuxer::Listener* listener);

    virtual ~DemuxerBundle();

    void clear();

    TsDemuxer* findDemuxer(int pid) const;

    void reorderStreams(const char* lang, StreamInfo::Type type);

    bool isReady() const;

    void updateFrom(StreamBundle* bundle);

    bool processTsPacket(uint8_t* packet, int64_t streamPosition);

    std::list<TsDemuxer*>::iterator begin() {
        return m_list.begin();
    }

    std::list<TsDemuxer*>::iterator end() {
        return m_list.end();
    }

    std::size_t size() {
        return m_list.size();
    }

protected:

    void reset();

    TsDemuxer::Listener* m_listener = NULL;

private:

    bool m_pendingError;

    std::list<TsDemuxer*> m_list;

};

#endif // ROBOTV_DEMUXERBUNDLE_H
