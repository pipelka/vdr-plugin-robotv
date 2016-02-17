/*
 *      vdr-plugin-robotv - RoboTV server plugin for VDR
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

#ifndef ROBOTV_RECORDINGCONTROLLER_H
#define	ROBOTV_RECORDINGCONTROLLER_H

#include "controller.h"

class RoboTvClient;
class MsgPacket;
class PacketPlayer;

class RecordingController : public Controller {
public:

    RecordingController(RoboTvClient* parent);

    virtual ~RecordingController();

    bool process(MsgPacket* request, MsgPacket* response);

protected:

    bool processOpen(MsgPacket* request, MsgPacket* response);

    bool processClose(MsgPacket* request, MsgPacket* response);

    bool processGetBlock(MsgPacket* request, MsgPacket* response);

    bool processGetPacket(MsgPacket* request, MsgPacket* response);

    bool processUpdate(MsgPacket* request, MsgPacket* response);

    bool processSeek(MsgPacket* request, MsgPacket* response);


private:

    RecordingController(const RecordingController& orig);

    RoboTvClient* m_parent;

    PacketPlayer* m_recPlayer;
};

#endif	// ROBOTV_RECORDINGCONTROLLER_H

