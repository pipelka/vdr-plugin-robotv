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

#include <live/channelcache.h>
#include "channelcontroller.h"
#include "tools/hash.h"
#include "tools/urlencode.h"

ChannelController::ChannelController() {
}

ChannelController::ChannelController(const ChannelController& orig) {
}

ChannelController::~ChannelController() {
}

MsgPacket* ChannelController::process(MsgPacket* request) {

    switch(request->getMsgID()) {
        case ROBOTV_CHANNELS_GETCHANNELS:
            return processGetChannels(request);
    }

    return nullptr;
}

MsgPacket* ChannelController::processGetChannels(MsgPacket* request) {
    ChannelCache& channelCache = ChannelCache::instance();
    RoboTVServerConfig& config = RoboTVServerConfig::instance();

    isyslog("Fetching channels ...");

    int type = request->get_U32();

    isyslog("Type: %s",
            type == 0 ? "MPEG2/H.264 channels" :
            type == 1 ? "radio channels" :
            type == 2 ? "H.264 channels" :
            "UNDEFINED"
           );

    //
    // PROTOCOL 7 - CLIENT MUST SEND THIS
    //

    const char* language = request->get_String();
    // do we want fta channels ?
    m_wantFta = request->get_U32();
    isyslog("Free To Air channels: %s", m_wantFta ? "Yes" : "No");

    // display only channels with native language audio ?
    m_filterLanguage = request->get_U32();
    isyslog("Only native language: %s", m_filterLanguage ? "Yes" : "No");

    // read caids
    m_caids.clear();
    uint32_t count = request->get_U32();

    isyslog("Enabled CaIDs: ");

    // sanity check (maximum of 20 caids)
    if(count < 20) {
        for(uint32_t i = 0; i < count; i++) {
            int caid = request->get_U32();
            m_caids.push_back(caid);
            isyslog("%04X", caid);
        }
    }

    m_languageIndex = I18nLanguageIndex(language);
    m_channelCount = channelCount();

    MsgPacket* response = createResponse(request);

    std::string groupName;

    for(cChannel* channel = Channels.First(); channel; channel = Channels.Next(channel)) {

        if(channel->GroupSep()) {
            groupName = m_toUtf8.convert(channel->Name());
            continue;
        }

        // skip disabled channels if filtering is enabled
        if(config.filterChannels && !channelCache.isEnabled(channel)) {
            continue;
        }

        if(!isChannelWanted(channel, type)) {
            continue;
        }

        addChannelToPacket(channel, response, groupName.c_str());
    }

    isyslog("client got %i channels", m_channelCount);
    return response;
}

int ChannelController::channelCount() {
    int count = 0;

    for(cChannel* channel = Channels.First(); channel; channel = Channels.Next(channel)) {
        if(isChannelWanted(channel, false)) {
            count++;
        }

        if(isChannelWanted(channel, true)) {
            count++;
        }
    }

    return count;
}

bool ChannelController::isChannelWanted(cChannel* channel, int type) {
    // dismiss invalid channels
    if(channel == NULL) {
        return false;
    }

    // radio
    if((type == 1) && !isRadio(channel)) {
        return false;
    }

    // (U)HD channels
    if((type == 2) && (channel->Vtype() != 27 && channel->Vtype() != 36)) {
        return false;
    }

    // skip channels witout SID
    if(channel->Sid() == 0) {
        return false;
    }

    if(strcmp(channel->Name(), ".") == 0) {
        return false;
    }

    // check language
    if(m_filterLanguage && m_languageIndex != -1) {
        bool bLanguageFound = false;
        const char* lang = NULL;

        // check MP2 languages
        for(int i = 0; i < MAXAPIDS; i++) {
            lang = channel->Alang(i);

            if(lang == NULL) {
                break;
            }

            if(m_languageIndex == I18nLanguageIndex(lang)) {
                bLanguageFound = true;
                break;
            }
        }

        // check other digital languages
        for(int i = 0; i < MAXDPIDS; i++) {
            lang = channel->Dlang(i);

            if(lang == NULL) {
                break;
            }

            if(m_languageIndex == I18nLanguageIndex(lang)) {
                bLanguageFound = true;
                break;
            }
        }

        if(!bLanguageFound) {
            return false;
        }
    }

    // user selection for FTA channels
    if(channel->Ca(0) == 0) {
        return m_wantFta;
    }

    // we want all encrypted channels if there isn't any CaID filter
    if(m_caids.size() == 0) {
        return true;
    }

    // check if we have a matching CaID
    for(std::list<int>::iterator i = m_caids.begin(); i != m_caids.end(); i++) {
        for(int j = 0; j < MAXCAIDS; j++) {

            if(channel->Ca(j) == 0) {
                break;
            }

            if(channel->Ca(j) == *i) {
                return true;
            }
        }
    }

    return false;
}

std::string ChannelController::createServiceReference(const cChannel* channel) {
    int hash = 0;

    if(cSource::IsSat(channel->Source())) {
        hash = channel->Source() & cSource::st_Pos;

#if VDRVERSNUM >= 20101
        hash = -hash;
#endif

        if(hash > 0x00007FFF) {
            hash |= 0xFFFF0000;
        }

        if(hash < 0) {
            hash = -hash;
        }
        else {
            hash = 1800 + hash;
        }

        hash = hash << 16;
    }
    else if(cSource::IsCable(channel->Source())) {
        hash = 0xFFFF0000;
    }
    else if(cSource::IsTerr(channel->Source())) {
        hash = 0xEEEE0000;
    }
    else if(cSource::IsAtsc(channel->Source())) {
        hash = 0xDDDD0000;
    }

    cString serviceref = cString::sprintf("1_0_%i_%X_%X_%X_%X_0_0_0",
                                          isRadio(channel) ? 2 : (channel->Vtype() == 27) ? 19 : 1,
                                          channel->Sid(),
                                          channel->Tid(),
                                          channel->Nid(),
                                          hash);

    return (const char*)serviceref;
}

std::string ChannelController::createLogoUrl(const cChannel* channel) {
    return createLogoUrl(channel, RoboTVServerConfig::instance().piconsUrl);
}

std::string ChannelController::createLogoUrl(const cChannel* channel, const std::string& baseUrl) {
    if(baseUrl.empty()) {
        return "";
    }

    std::string filename = createServiceReference(channel);

    if(baseUrl.size() > 4 && baseUrl.substr(0, 4) == "http") {
        filename = urlEncode(filename);
    }

    cString piconurl = AddDirectory(baseUrl.c_str(), filename.c_str());
    return (const char*)cString::sprintf("%s.png", (const char*)piconurl);
}

void ChannelController::addChannelToPacket(const cChannel* channel, MsgPacket* p, const char* group) {
    p->put_U32(channel->Number());
    p->put_String(m_toUtf8.convert(channel->Name()));
    p->put_U32(createChannelUid(channel));
    p->put_U32(channel->Ca());

    // logo url
    p->put_String(createLogoUrl(channel).c_str());

    // service reference
    p->put_String(createServiceReference(channel).c_str());

    // group
    p->put_String(group == NULL ? "" : group);
}

bool ChannelController::isRadio(const cChannel* channel) {

    // assume channels without VPID & APID are video channels
    if(channel->Vpid() == 0 && channel->Apid(0) == 0) {
        return false;
    }
    // channels without VPID are radio channels (channels with VPID 1 are encrypted radio channels)
    else if(channel->Vpid() == 0 || channel->Vpid() == 1) {
        return true;
    }

    return false;
}
