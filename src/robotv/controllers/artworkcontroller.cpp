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

#include "artworkcontroller.h"
#include "net/msgpacket.h"
#include "robotv/robotvcommand.h"

ArtworkController::ArtworkController() {
}

ArtworkController::ArtworkController(const ArtworkController& orig) {
}

ArtworkController::~ArtworkController() {
}

MsgPacket* ArtworkController::process(MsgPacket* request) {
    switch(request->getMsgID()) {
        case ROBOTV_ARTWORK_GET:
            return processGet(request);

        case ROBOTV_ARTWORK_SET:
            return processSet(request);
    }

    return nullptr;
}

MsgPacket* ArtworkController::processGet(MsgPacket* request) {
    const char* title = request->get_String();
    uint32_t content = request->get_U32();

    std::string poster;
    std::string background;

    if(!m_artwork.get(content, title, poster, background)) {
        poster = "x";
        background = "x";
    }
    else if(!request->eop()) {
        uint32_t channelUid = request->get_U32();
        uint32_t eventId = request->get_U32();

        if(channelUid != 0 && eventId != 0) {
            Artwork::Holder holder;
            holder.eventId = eventId;
            holder.channelUid = channelUid;
            holder.contentId = content;
            holder.posterUrl = poster;
            holder.backdropUrl = background;

            if(holder.hasArtwork()) {
                m_artwork.setEpgImage(holder);
            }
        }
    }

    MsgPacket* response = createResponse(request);

    response->put_String(poster.c_str());
    response->put_String(background.c_str());
    response->put_U32(0); // TODO - externalId

    return response;
}

MsgPacket* ArtworkController::processSet(MsgPacket* request) {
    const char* title = request->get_String();
    uint32_t content = request->get_U32();
    const char* poster = request->get_String();
    const char* background = request->get_String();
    uint32_t externalId = request->get_U32();

    m_artwork.set(content, title, poster, background, externalId);

    if(!request->eop()) {
        uint32_t channelUid = request->get_U32();
        uint32_t eventId = request->get_U32();

        if(channelUid != 0 && eventId != 0) {
            Artwork::Holder holder;
            holder.eventId = eventId;
            holder.channelUid = channelUid;
            holder.contentId = content;
            holder.posterUrl = poster;
            holder.backdropUrl = background;

            m_artwork.setEpgImage(holder);
        }
    }

    return createResponse(request);
}
