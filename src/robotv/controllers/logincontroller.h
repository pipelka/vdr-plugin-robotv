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

#ifndef ROBOTV_LOGINCONTROLLER_H
#define ROBOTV_LOGINCONTROLLER_H

#include <stdint.h>
#include "controller.h"

class MsgPacket;

class LoginController : public Controller {
public:

    LoginController();

    virtual ~LoginController();

    MsgPacket* process(MsgPacket* request);

    bool statusEnabled() const {
        return m_statusInterfaceEnabled;
    }

    uint16_t protocolVersion() const {
        return m_protocolVersion;
    }

    bool loggedIn() const {
        return m_loggedIn;
    }

    void setSocket(int fd) {
        m_socket = fd;
    }

protected:

    MsgPacket* processLogin(MsgPacket* request);

    MsgPacket* processGetConfig(MsgPacket* request);

private:

    LoginController(const LoginController& orig);

    uint16_t m_protocolVersion = 0;

    int m_compressionLevel = 0;

    bool m_loggedIn = false;

    bool m_statusInterfaceEnabled = false;

    int m_socketPriority = 7;

    int m_socket = -1;

};

#endif // ROBOTV_LOGINCONTROLLER_H

