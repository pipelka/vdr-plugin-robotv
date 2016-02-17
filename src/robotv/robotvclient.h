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

#ifndef ROBOTV_CLIENT_H
#define ROBOTV_CLIENT_H

#include <list>
#include <string>
#include <queue>

#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/receiver.h>
#include <vdr/status.h>

#include "demuxer/streaminfo.h"
#include "recordings/artwork.h"

#include "controllers/streamcontroller.h"
#include "controllers/recordingcontroller.h"
#include "controllers/channelcontroller.h"
#include "controllers/timercontroller.h"
#include "controllers/moviecontroller.h"
#include "controllers/logincontroller.h"
#include "controllers/epgcontroller.h"
#include "controllers/artworkcontroller.h"

class cChannel;
class cDevice;
class MsgPacket;
class PacketPlayer;

class RoboTvClient : public cThread, public cStatus {
private:

    unsigned int m_id;

    int m_socket;

    MsgPacket* m_request = NULL;

    MsgPacket* m_response = NULL;

    cCharSetConv m_toUtf8;

    int m_timeout = 3000;

    std::queue<MsgPacket*> m_queue;

    cMutex m_queueLock;

    Artwork m_artwork;

    // Controllers

    StreamController m_streamController;

    RecordingController m_recordingController;

    ChannelController m_channelController;

    TimerController m_timerController;

    MovieController m_movieController;

    LoginController m_loginController;

    EpgController m_epgController;

    ArtworkController m_artworkController;

    std::list<Controller*> m_controllers;

protected:

    bool processRequest();

    virtual void Action(void);

    virtual void TimerChange(const cTimer* Timer, eTimerChange Change);
    virtual void ChannelChange(const cChannel* Channel);
    virtual void Recording(const cDevice* Device, const char* Name, const char* FileName, bool On);
    virtual void OsdStatusMessage(const char* Message);

public:

    RoboTvClient(int fd, unsigned int id);

    virtual ~RoboTvClient();

    void sendMoviesChange();

    void sendTimerChange();

    void queueMessage(MsgPacket* p);

    void sendStatusMessage(const char* Message);

    unsigned int getId() const {
        return m_id;
    }

    int getSocket() const {
        return m_socket;
    }

};

#endif // ROBOTV_CLIENT_H
