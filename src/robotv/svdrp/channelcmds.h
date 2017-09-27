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

#ifndef ROBOTV_CHANNELCMDS_H
#define ROBOTV_CHANNELCMDS_H

#include "recordings/artwork.h"
#include "tools/utf8conv.h"
#include "tools/json.hpp"
#include "vdr/channels.h"
#include "vdr/epg.h"
#include "vdr/tools.h"

class ChannelCmds {
public:

    ChannelCmds();

    virtual ~ChannelCmds();

    cString SVDRPCommand(const char* Command, const char* Option, int& ReplyCode);

private:

    cString processListChannelsJson(const char* Option, int& ReplyCode);

    cString processListEpgJson(const char* Option, int& ReplyCode);

    nlohmann::json jsonFromEvent(uint32_t channelUid, const cEvent* event, Artwork& artwork);

    nlohmann::json jsonFromChannel(const cChannel* channel, const char* groupName = NULL, bool enabled = true);

    ChannelCmds(const ChannelCmds& orig);

    Utf8Conv m_toUtf8;

};

#endif	// ROBOTV_CHANNELCMDS_H
