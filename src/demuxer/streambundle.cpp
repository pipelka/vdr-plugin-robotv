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

#include "demuxer/streambundle.h"

StreamBundle::StreamBundle() : m_changed(false) {
}

void StreamBundle::addStream(const StreamInfo& s) {
    if(s.getPid() == 0 || s.getType() == StreamInfo::stNONE) {
        return;
    }

    // allow only one video stream
    if(s.getContent() == StreamInfo::scVIDEO) {
        for(iterator i = begin(); i != end(); i++) {
            if(i->second.getContent() == StreamInfo::scVIDEO && i->second.getPid() != s.getPid()) {
                return;
            }
        }
    }

    StreamInfo old = (*this)[s.getPid()];
    (*this)[s.getPid()] = s;

    m_changed = (old != s);
}

bool StreamBundle::isParsed() {
    if(empty()) {
        return false;
    }

    for(iterator i = begin(); i != end(); i++)
        if(!i->second.isParsed()) {
            return false;
        }

    return true;
}

bool StreamBundle::operator ==(const StreamBundle& c) const {
    if(size() != c.size()) {
        return false;
    }

    for(const_iterator i = begin(); i != end(); i++)
        if(!c.contains(i->second)) {
            return false;
        }

    return true;
}

bool StreamBundle::isMetaOf(const StreamBundle& c) const {
    if(size() != c.size()) {
        return false;
    }

    for(const_iterator i = begin(); i != end(); i++) {
        const_iterator it = c.find(i->second.getPid());

        if(it == c.end()) {
            return false;
        }

        if(!i->second.isMetaOf(it->second)) {
            return false;
        }
    }

    return true;
}

bool StreamBundle::contains(const StreamInfo& s) const {
    const_iterator i = find(s.getPid());

    if(i == end()) {
        return false;
    }

    return (i->second == s);
}
