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

#include "channelcmds.h"
#include "robotv/controllers/channelcontroller.h"
#include "config/config.h"
#include "tools/hash.h"

using json = nlohmann::json;

ChannelCmds::ChannelCmds() {
}

ChannelCmds::ChannelCmds(const ChannelCmds& orig) {
}

ChannelCmds::~ChannelCmds() {
}

cString ChannelCmds::SVDRPCommand(const char* Command, const char* Option, int& ReplyCode) {
    if(strcmp(Command, "LSCJ") == 0) {
        return processListChannelsJson(Option, ReplyCode);
    }

    ReplyCode = 500;
    return NULL;
}

cString ChannelCmds::processListChannelsJson(const char* Option, int& ReplyCode) {
    RoboTVChannels& c = RoboTVChannels::instance();

    if(!c.lock(false)) {
        ReplyCode = 500;
        return NULL;
    }

    cChannels* channels = c.get();
    json list = json::array();
    std::string groupName;

    for(cChannel* channel = channels->First(); channel; channel = channels->Next(channel)) {
        if(channel->GroupSep()) {
            groupName = m_toUtf8.convert(channel->Name());
            continue;
        }

        list.push_back(jsonFromChannel(channel, groupName.c_str(), true));
    }

    c.Unlock();

    return cString(list.dump().c_str());
}

json ChannelCmds::jsonFromChannel(const cChannel* channel, const char* groupName, bool enabled) {
    json j = {
        {"number", channel->Number()},
        {"name", m_toUtf8.convert(channel->Name())},
        {"shortName", m_toUtf8.convert(channel->ShortName())},
        {"uid", createChannelUid(channel)},
        {"logoUrl", ChannelController::createLogoUrl(channel, RoboTVServerConfig::instance().piconsUrl).c_str()},
        {"serviceRef", ChannelController::createServiceReference(channel).c_str()},
        {"provider", channel->Provider()},
        {"group", groupName != NULL ? groupName : ""},
        {"enabled", enabled}
    };

    // caids
    json caids = json::array();
    int i = 0;

    while(channel->Ca(i) != 0) {
        caids.push_back(channel->Ca(i++));
    }

    j["caids"] = caids;

    // pids
    json pids;

    json video;

    video["pid"] = channel->Vpid();
    video["type"] = channel->Vtype();
    pids["video"] = video;

    json audio;
    i = 0;

    while(channel->Apid(i) != 0) {
        audio.push_back(json({
            {"pid", channel->Apid(i)},
            {"type", channel->Atype(i)},
            {"lang", channel->Alang(i)}
        }));
        i++;
    }

    i = 0;

    while(channel->Dpid(i) != 0) {
        audio.push_back(json({
            {"pid", channel->Dpid(i)},
            {"type", channel->Dtype(i)},
            {"lang", channel->Dlang(i)}
        }));
        i++;
    }

    pids["audio"] = audio;
    j["pids"] = pids;

    return j;
}
