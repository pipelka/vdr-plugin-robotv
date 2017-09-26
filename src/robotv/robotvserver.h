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

#ifndef ROBOTV_SERVER_H
#define ROBOTV_SERVER_H

#include <list>
#include <vdr/thread.h>

#include "config/config.h"

class RoboTvClient;

class RoboTVServer : public cThread {
protected:

    typedef std::list<RoboTvClient*> ClientList;

    virtual void Action(void);

    void clientConnected(int fd);

    int m_serverPort;

    int m_serverFd;

    bool m_ipv4Fallback;

    cString m_allowedHostsFile;

    ClientList m_clients;

    RoboTVServerConfig& m_config;

    static unsigned int m_idCnt;

public:

    RoboTVServer(int listenPort);

    virtual ~RoboTVServer();
};

#endif // ROBOTV_SERVER_H
