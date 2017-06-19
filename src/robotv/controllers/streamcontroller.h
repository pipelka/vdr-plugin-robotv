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

#ifndef ROBOTV_STREAMCONTROLLER_H
#define	ROBOTV_STREAMCONTROLLER_H

#include <mutex>

#include "live/livestreamer.h"
#include "controller.h"

class RoboTvClient;
class MsgPacket;

class StreamController : public Controller {
public:

    StreamController(RoboTvClient* parent);

    virtual ~StreamController();

    MsgPacket* process(MsgPacket* request);

    void processChannelChange(const cChannel* Channel);

protected:

    MsgPacket* processOpen(MsgPacket* request);

    MsgPacket* processClose(MsgPacket* request);

    MsgPacket* processPause(MsgPacket* request);

    MsgPacket* processRequest(MsgPacket* request);

    MsgPacket* processSignal(MsgPacket* request);

    MsgPacket* processSeek(MsgPacket* request);

private:

    StreamController(const StreamController& orig);

    int startStreaming(int version, const cChannel* channel, int32_t priority, bool waitforiframe = false);

    void stopStreaming();

    std::string m_language;

    StreamInfo::Type m_langStreamType;

    LiveStreamer* m_streamer = NULL;

    std::mutex m_lock;

    RoboTvClient* m_parent;
};

#endif	// ROBOTV_STREAMCONTROLLER_H

