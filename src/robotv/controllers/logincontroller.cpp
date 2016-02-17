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

#include "logincontroller.h"
#include "net/msgpacket.h"
#include "config/config.h"
#include "robotv/robotvcommand.h"

LoginController::LoginController() {
}

LoginController::LoginController(const LoginController& orig) {
}

LoginController::~LoginController() {
}

bool LoginController::process(MsgPacket* request, MsgPacket* response) {
    switch(request->getMsgID()) {
        case ROBOTV_LOGIN:
            return processLogin(request, response);

        case ROBOTV_UPDATECHANNELS:
            return processUpdateChannels(request, response);
    }

    return false;
}

bool LoginController::processLogin(MsgPacket* request, MsgPacket* response) {
    m_protocolVersion = request->getProtocolVersion();
    m_compressionLevel = request->get_U8();
    const char* clientName = request->get_String();
    m_statusInterfaceEnabled = request->get_U8();

    if(m_protocolVersion > ROBOTV_PROTOCOLVERSION || m_protocolVersion < 7) {
        ERRORLOG("Client '%s' has unsupported protocol version '%u', terminating client", clientName, m_protocolVersion);
        return false;
    }

    INFOLOG("Welcome client '%s' with protocol version '%u'", clientName, m_protocolVersion);

    // Send the login reply
    time_t timeNow = time(NULL);
    struct tm* timeStruct = localtime(&timeNow);
    int timeOffset = timeStruct->tm_gmtoff;

    response->setProtocolVersion(m_protocolVersion);
    response->put_U32(timeNow);
    response->put_S32(timeOffset);
    response->put_String("roboTV VDR Server");
    response->put_String(ROBOTV_VERSION);

    m_loggedIn = true;
    return true;
}

bool LoginController::processUpdateChannels(MsgPacket* request, MsgPacket* response) {
    uint8_t updateChannels = request->get_U8();

    if(updateChannels <= 5) {
        Setup.UpdateChannels = updateChannels;
        INFOLOG("Setting channel update method: %i", updateChannels);
        response->put_U32(ROBOTV_RET_OK);
    }
    else {
        response->put_U32(ROBOTV_RET_DATAINVALID);
    }

    return true;
}
