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

#ifndef ROBOTV_TIMERCONTROLLER_H
#define ROBOTV_TIMERCONTROLLER_H

#include "vdr/timers.h"
#include "controller.h"

class RoboTvClient;
class MsgPacket;

class TimerController : public Controller {
public:

    TimerController(RoboTvClient* parent);

    virtual ~TimerController();

    bool process(MsgPacket* request, MsgPacket* response);

private:

    void putTimer(cTimer* timer, MsgPacket* p);

    int checkTimerConflicts(cTimer* timer);

    //

    bool processGet(MsgPacket* request, MsgPacket* response);

    bool processGetTimers(MsgPacket* request, MsgPacket* response);

    bool processAdd(MsgPacket* request, MsgPacket* response);

    bool processDelete(MsgPacket* request, MsgPacket* response);

    bool processUpdate(MsgPacket* request, MsgPacket* response);

    TimerController(const TimerController& orig);

    RoboTvClient* m_parent;

    cCharSetConv m_toUtf8;

};

#endif // ROBOTV_TIMERCONTROLLER_H

