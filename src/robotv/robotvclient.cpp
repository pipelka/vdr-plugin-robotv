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

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <string>

#include <vdr/recording.h>
#include <vdr/channels.h>
#include <vdr/i18n.h>
#include <vdr/plugin.h>
#include <vdr/menu.h>
#include <vdr/device.h>
#include <vdr/sources.h>

#include "config/config.h"
#include "net/msgpacket.h"
#include "tools/hash.h"
#include "tools/urlencode.h"
#include "tools/recid2uid.h"

#include "robotv/robotvchannels.h"

#include "robotvcommand.h"
#include "robotvclient.h"
#include "robotvserver.h"

RoboTvClient::RoboTvClient(int fd, unsigned int id) : m_id(id), m_socket(fd),
    m_streamController(this),
    m_recordingController(this),
    m_timerController(this) {

    m_controllers = {
        &m_streamController,
        &m_recordingController,
        &m_channelController,
        &m_timerController,
        &m_movieController,
        &m_loginController,
        &m_epgController,
        &m_artworkController
    };

    Start();
}

RoboTvClient::~RoboTvClient() {
    // shutdown connection
    shutdown(m_socket, SHUT_RDWR);
    Cancel(10);

    // close connection
    close(m_socket);

    // delete messagequeue
    {
        cMutexLock lock(&m_queueLock);

        while(!m_queue.empty()) {
            MsgPacket* p = m_queue.front();
            m_queue.pop();
            delete p;
        }
    }

    DEBUGLOG("done");
}

void RoboTvClient::Action(void) {
    bool bClosed(false);

    while(Running()) {

        // send pending messages
        {
            cMutexLock lock(&m_queueLock);

            while(!m_queue.empty()) {
                MsgPacket* p = m_queue.front();

                if(!p->write(m_socket, m_timeout)) {
                    break;
                }

                m_queue.pop();
                delete p;
            }
        }

        m_request = MsgPacket::read(m_socket, bClosed, 1000);

        if(bClosed) {
            delete m_request;
            m_request = NULL;
            break;
        }

        if(m_request != NULL) {
            processRequest();
            delete m_request;
        }
    }
}

void RoboTvClient::TimerChange(const cTimer* Timer, eTimerChange Change) {
    // ignore invalid timers
    if(Timer == NULL) {
        return;
    }

    sendTimerChange();
}

void RoboTvClient::ChannelChange(const cChannel* Channel) {
    m_streamController.processChannelChange(Channel);

    if(!m_loginController.statusEnabled()) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_CHANNELCHANGED, ROBOTV_CHANNEL_STATUS);
    m_channelController.addChannelToPacket(Channel, resp);

    queueMessage(resp);
}

void RoboTvClient::sendTimerChange() {
    if(!m_loginController.statusEnabled()) {
        return;
    }

    INFOLOG("Sending timer change request to client #%i ...", m_id);
    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_TIMERCHANGE, ROBOTV_CHANNEL_STATUS);

    queueMessage(resp);
}

void RoboTvClient::sendMoviesChange() {
    if(!m_loginController.statusEnabled()) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_RECORDINGSCHANGE, ROBOTV_CHANNEL_STATUS);
    queueMessage(resp);
}

void RoboTvClient::Recording(const cDevice* Device, const char* Name, const char* FileName, bool On) {
    if(!m_loginController.statusEnabled()) {
        return;
    }

    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_RECORDING, ROBOTV_CHANNEL_STATUS);

    resp->put_U32(Device->CardIndex());
    resp->put_U32(On);

    if(Name) {
        resp->put_String(Name);
    }
    else {
        resp->put_String("");
    }

    if(FileName) {
        resp->put_String(FileName);
    }
    else {
        resp->put_String("");
    }

    queueMessage(resp);
}

void RoboTvClient::OsdStatusMessage(const char* Message) {
    if(!m_loginController.statusEnabled() || Message == NULL) {
        return;
    }

    /* Ignore this messages */
    if(strcasecmp(Message, trVDR("Channel not available!")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Delete timer?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Delete recording?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Press any key to cancel shutdown")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Press any key to cancel restart")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Editing - shut down anyway?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Recording - shut down anyway?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("shut down anyway?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Recording - restart anyway?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Editing - restart anyway?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Delete channel?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Timer still recording - really delete?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Delete marks information?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Delete resume information?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("CAM is in use - really reset?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Really restart?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Stop recording?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Cancel editing?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("Cutter already running - Add to cutting queue?")) == 0) {
        return;
    }
    else if(strcasecmp(Message, trVDR("No index-file found. Creating may take minutes. Create one?")) == 0) {
        return;
    }

    sendStatusMessage(Message);
}

void RoboTvClient::sendStatusMessage(const char* Message) {
    MsgPacket* resp = new MsgPacket(ROBOTV_STATUS_MESSAGE, ROBOTV_CHANNEL_STATUS);

    resp->put_U32(0);
    resp->put_String(Message);

    queueMessage(resp);
}

bool RoboTvClient::processRequest() {

    // set protocol version for all messages
    // except login, because login defines the
    // protocol version

    if(m_request->getMsgID() != ROBOTV_LOGIN) {
        m_request->setProtocolVersion(m_loginController.protocolVersion());
    }

    m_response = new MsgPacket(m_request->getMsgID(), ROBOTV_CHANNEL_REQUEST_RESPONSE, m_request->getUID());
    m_response->setProtocolVersion(m_loginController.protocolVersion());

    for(auto i : m_controllers) {
        if(i->process(m_request, m_response)) {
            queueMessage(m_response);
            return true;
        }
    }

    return false;
}

void RoboTvClient::queueMessage(MsgPacket* p) {
    cMutexLock lock(&m_queueLock);
    m_queue.push(p);
}
