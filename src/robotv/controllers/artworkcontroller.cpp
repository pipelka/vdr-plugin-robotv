/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
 *
 *      Copyright (C) 2015 Alexander Pipelka
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

bool ArtworkController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_ARTWORK_GET:
            return processGet(request, response);

        case ROBOTV_ARTWORK_SET:
            return processSet(request, response);
    }

    return false;
}

bool ArtworkController::processGet(MsgPacket* request, MsgPacket* response) {
    const char* title = request->get_String();
    uint32_t content = request->get_U32();

    std::string poster;
    std::string background;

    if(!m_artwork.get(content, title, poster, background)) {
        poster = "x";
        background = "x";
    }

    response->put_String(poster.c_str());
    response->put_String(background.c_str());
    response->put_U32(0); // TODO - externalId

    return true;
}

bool ArtworkController::processSet(MsgPacket* request, MsgPacket* response) {
    const char* title = request->get_String();
    uint32_t content = request->get_U32();
    const char* poster = request->get_String();
    const char* background = request->get_String();
    uint32_t externalId = request->get_U32();

    INFOLOG("set artwork: %s (%i): %s", title, content, background);
    m_artwork.set(content, title, poster, background, externalId);
    return true;
}
