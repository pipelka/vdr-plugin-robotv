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

#include <tools/utf8conv.h>
#include "vdr/recording.h"
#include "vdr/tools.h"
#include "controller.h"

class MsgPacket;

class MovieController : public Controller {
public:

    MovieController();

    virtual ~MovieController();

    MsgPacket* process(MsgPacket* request);

    static std::string folderFromName(const std::string& name);

protected:

    MsgPacket* processGetDiskSpace(MsgPacket* request);

    MsgPacket* processGetFolders(MsgPacket* request);

    MsgPacket* processGetList(MsgPacket* request);

    MsgPacket* processRename(MsgPacket* request);

    MsgPacket* processDelete(MsgPacket* request);

    MsgPacket* processSetPlayCount(MsgPacket* request);

    MsgPacket* processSetPosition(MsgPacket* request);

    MsgPacket* processGetPosition(MsgPacket* request);

    MsgPacket* processGetMarks(MsgPacket* request);

    MsgPacket* processSetUrls(MsgPacket* request);

    MsgPacket* processSearch(MsgPacket* request);

private:

    MovieController(const MovieController& orig);

    void recordingToPacket(cRecording* recording, MsgPacket* response);

    Utf8Conv m_toUtf8;

};

#endif // ROBOTV_MOVIESCONTROLLER_H

