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

#ifndef ROBOTV_MOVIECONTROLLER_H
#define ROBOTV_MOVIECONTROLLER_H

#include "vdr/tools.h"
#include "controller.h"

class MsgPacket;

class MovieController : public Controller {
public:

    MovieController();

    virtual ~MovieController();

    bool process(MsgPacket* request, MsgPacket* response);

protected:

    bool processGetDiskSpace(MsgPacket* request, MsgPacket* response);

    bool processGetList(MsgPacket* request, MsgPacket* response);

    bool processRename(MsgPacket* request, MsgPacket* response);

    bool processDelete(MsgPacket* request, MsgPacket* response);

    bool processSetPlayCount(MsgPacket* request, MsgPacket* response);

    bool processSetPosition(MsgPacket* request, MsgPacket* response);

    bool processGetPosition(MsgPacket* request, MsgPacket* response);

    bool processGetMarks(MsgPacket* request, MsgPacket* response);

    bool processSetUrls(MsgPacket* request, MsgPacket* response);

private:

    MovieController(const MovieController& orig);

    cCharSetConv m_toUtf8;

};

#endif // ROBOTV_MOVIESCONTROLLER_H

