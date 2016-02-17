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

#ifndef ROBOTV_STREAMBUNDLE_H
#define ROBOTV_STREAMBUNDLE_H

#include "demuxer/streaminfo.h"
#include "vdr/channels.h"
#include "vdr/remux.h"
#include <map>

class StreamBundle : public std::map<int, StreamInfo> {
public:

    StreamBundle();

    void addStream(const StreamInfo& s);

    bool operator ==(const StreamBundle& c) const;

    bool isMetaOf(const StreamBundle& c) const;

    bool contains(const StreamInfo& s) const;

    bool changed() const {
        return m_changed;
    }

    bool isParsed();

    static StreamBundle createFromChannel(const cChannel* channel);

    static StreamBundle createFromPatPmt(const cPatPmtParser* patpmt);

private:

    bool m_changed;

};

#endif // ROBOTV_STREAMBUNDLE_H
