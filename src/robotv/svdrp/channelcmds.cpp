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
#include "live/channelcache.h"

using json = nlohmann::json;

ChannelCmds::ChannelCmds() {
}

ChannelCmds::ChannelCmds(const ChannelCmds& orig) {
}

ChannelCmds::~ChannelCmds() {
}

cString ChannelCmds::SVDRPCommand(const char* Command, const char* Option, int& ReplyCode) {
    if(strcasecmp(Command, "LSCJ") == 0) {
        return processListChannelsJson(Option, ReplyCode);
    }

    if(strcasecmp(Command, "LSEJ") == 0) {
        return processListEpgJson(Option, ReplyCode);
    }

    ReplyCode = 500;
    return NULL;
}

cString ChannelCmds::processListChannelsJson(const char* Option, int& ReplyCode) {
    ChannelCache& channelCache = ChannelCache::instance();

    json list = json::array();
    std::string groupName;

    LOCK_CHANNELS_READ;

    for(auto channel = Channels->First(); channel; channel = Channels->Next(channel)) {
        if(channel->GroupSep()) {
            groupName = m_toUtf8.convert(channel->Name());
            continue;
        }

        list.push_back(jsonFromChannel(channel, groupName.c_str(), channelCache.isEnabled(channel)));
    }

    return cString(list.dump().c_str());
}

cString ChannelCmds::processListEpgJson(const char* Option, int& ReplyCode) {
    if(Option == nullptr) {
        ReplyCode = 501;
        return "Invalid channel UID";
    }

    uint32_t id = (uint32_t)strtol(Option, nullptr, 10);

    LOCK_CHANNELS_READ;
    LOCK_SCHEDULES_READ;

    const cChannel* channel = roboTV::Hash::findChannelByUid(Channels, id);

    if(channel == nullptr) {
        channel = roboTV::Hash::findChannelByNumber(Channels, id);
    }

    if(channel == nullptr) {
        ReplyCode = 501;
        return "Channel unknown";
    }

    uint32_t channelUid = roboTV::Hash::createChannelUid(channel);

    const cSchedule* Schedule = Schedules->GetSchedule(channel->GetChannelID());

    json list = json::array();

    if(!Schedule) {
        return cString(list.dump().c_str());
    }

    Artwork artwork;
    time_t now = time(nullptr);

    for(const cEvent* event = Schedule->Events()->First(); event; event = Schedule->Events()->Next(event)) {
        if(event->StartTime() + event->Duration() >= now) {
            list.push_back(jsonFromEvent(channelUid, event, artwork));
        }
    }

    return cString(list.dump().c_str());
}

json ChannelCmds::jsonFromChannel(const cChannel* channel, const char* groupName, bool enabled) {
    json j = {
        {"number", channel->Number()},
        {"name", m_toUtf8.convert(channel->Name())},
        {"shortName", m_toUtf8.convert(channel->ShortName())},
        {"uid", roboTV::Hash::createChannelUid(channel)},
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

nlohmann::json ChannelCmds::jsonFromEvent(uint32_t channelUid, const cEvent* event, Artwork& artwork) {
    Artwork::Holder holder;
    artwork.getEpgImage(channelUid, event->EventID(), holder);

    json j = {
        {"title", m_toUtf8.convert(event->Title() ? event->Title() : "")},
        {"shortText", m_toUtf8.convert(event->ShortText() ? event->ShortText() : "")},
        {"description", m_toUtf8.convert(event->Description() ? event->Description() : "")},
        {"eventId", event->EventID()},
        {"channelUid", channelUid },
        {"contentId", holder.contentId},
        {"startTime", event->StartTime()},
        {"duration", event->Duration()},
        {"vpsTime", event->Vps()},
        {"posterUrl", m_toUtf8.convert(holder.posterUrl != "x" ? holder.posterUrl : "")},
        {"backdropUrl", m_toUtf8.convert(holder.backdropUrl != "x" ? holder.backdropUrl : "")},
        {"parentalRating", event->ParentalRating()}
    };

    return j;
}
