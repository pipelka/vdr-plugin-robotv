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

#ifndef ROBOTV_CONTROLLER_H
#define ROBOTV_CONTROLLER_H

#include <net/msgpacket.h>
#include <robotv/robotvcommand.h>

class Controller {
public:

    virtual ~Controller() = default;

    virtual MsgPacket* process(MsgPacket* request) = 0;

protected:

    inline MsgPacket* createResponse(MsgPacket* request) {
        MsgPacket* response = new MsgPacket(request->getMsgID(), ROBOTV_CHANNEL_REQUEST_RESPONSE, request->getUID());
        response->setProtocolVersion(request->getProtocolVersion());

        return response;
    }

    inline MsgPacket* createResponse(MsgPacket* request, MsgPacket* payload) {
        payload->setType(ROBOTV_CHANNEL_REQUEST_RESPONSE);
        payload->setMsgID(request->getMsgID());
        payload->setUID(request->getUID());
        payload->setProtocolVersion(request->getProtocolVersion());

        return payload;
    }
};

#endif // ROBOTV_CONTROLLER_H

