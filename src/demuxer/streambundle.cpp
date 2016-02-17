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

StreamBundle StreamBundle::createFromPatPmt(const cPatPmtParser* patpmt) {
    StreamBundle item;
    int patVersion = 0;
    int pmtVersion = 0;

    if(!patpmt->GetVersions(patVersion, pmtVersion)) {
        return item;
    }

    // add video stream
    int vpid = patpmt->Vpid();
    int vtype = patpmt->Vtype();

    item.addStream(StreamInfo(vpid,
                              vtype == 0x02 ? StreamInfo::stMPEG2VIDEO :
                              vtype == 0x1b ? StreamInfo::stH264 :
                              vtype == 0x24 ? StreamInfo::stH265 :
                              StreamInfo::stNONE));

    // add (E)AC3 streams
    for(int i = 0; patpmt->Dpid(i) != 0; i++) {
        int dtype = patpmt->Dtype(i);
        item.addStream(StreamInfo(patpmt->Dpid(i),
                                  dtype == 0x6A ? StreamInfo::stAC3 :
                                  dtype == 0x7A ? StreamInfo::stEAC3 :
                                  StreamInfo::stNONE,
                                  patpmt->Dlang(i)));
    }

    // add audio streams
    for(int i = 0; patpmt->Apid(i) != 0; i++) {
        int atype = patpmt->Atype(i);
        item.addStream(StreamInfo(patpmt->Apid(i),
                                  atype == 0x04 ? StreamInfo::stMPEG2AUDIO :
                                  atype == 0x03 ? StreamInfo::stMPEG2AUDIO :
                                  atype == 0x0f ? StreamInfo::stAAC :
                                  atype == 0x11 ? StreamInfo::stLATM :
                                  StreamInfo::stNONE,
                                  patpmt->Alang(i)));
    }

    // add subtitle streams
    for(int i = 0; patpmt->Spid(i) != 0; i++) {
        StreamInfo stream(patpmt->Spid(i), StreamInfo::stDVBSUB, patpmt->Slang(i));

        stream.setSubtitlingDescriptor(
            patpmt->SubtitlingType(i),
            patpmt->CompositionPageId(i),
            patpmt->AncillaryPageId(i));

        item.addStream(stream);
    }

    return item;
}

StreamBundle StreamBundle::createFromChannel(const cChannel* channel) {
    StreamBundle item;

    // add video stream
    int vpid = channel->Vpid();
    int vtype = channel->Vtype();

    item.addStream(StreamInfo(vpid,
                              vtype == 0x02 ? StreamInfo::stMPEG2VIDEO :
                              vtype == 0x1b ? StreamInfo::stH264 :
                              vtype == 0x24 ? StreamInfo::stH265 :
                              StreamInfo::stNONE));

    // add (E)AC3 streams
    for(int i = 0; channel->Dpid(i) != 0; i++) {
        int dtype = channel->Dtype(i);
        item.addStream(StreamInfo(channel->Dpid(i),
                                  dtype == 0x6A ? StreamInfo::stAC3 :
                                  dtype == 0x7A ? StreamInfo::stEAC3 :
                                  StreamInfo::stNONE,
                                  channel->Dlang(i)));
    }

    // add audio streams
    for(int i = 0; channel->Apid(i) != 0; i++) {
        int atype = channel->Atype(i);
        item.addStream(StreamInfo(channel->Apid(i),
                                  atype == 0x04 ? StreamInfo::stMPEG2AUDIO :
                                  atype == 0x03 ? StreamInfo::stMPEG2AUDIO :
                                  atype == 0x0f ? StreamInfo::stAAC :
                                  atype == 0x11 ? StreamInfo::stLATM :
                                  StreamInfo::stNONE,
                                  channel->Alang(i)));
    }

    // add teletext stream
    if(channel->Tpid() != 0) {
        item.addStream(StreamInfo(channel->Tpid(), StreamInfo::stTELETEXT));
    }

    // add subtitle streams
    for(int i = 0; channel->Spid(i) != 0; i++) {
        StreamInfo stream(channel->Spid(i), StreamInfo::stDVBSUB, channel->Slang(i));

        stream.setSubtitlingDescriptor(
            channel->SubtitlingType(i),
            channel->CompositionPageId(i),
            channel->AncillaryPageId(i));

        item.addStream(stream);
    }

    return item;
}
