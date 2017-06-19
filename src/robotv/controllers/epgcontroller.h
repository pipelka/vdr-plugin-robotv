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

#ifndef ROBOTV_EPGCONTROLLER_H
#define ROBOTV_EPGCONTROLLER_H

#include <tools/utf8conv.h>
#include "controller.h"
#include "recordings/artwork.h"
#include "vdr/tools.h"
#include "vdr/epg.h"

class EpgController : public Controller {
public:

    EpgController();

    virtual ~EpgController();

    MsgPacket* process(MsgPacket* request);

protected:

    MsgPacket* processGet(MsgPacket* request);

    MsgPacket* processSearch(MsgPacket* request);

private:

    bool searchEpg(const std::string& searchTerm, std::function<void(tEventID, time_t, tChannelID)> callback);

    EpgController(const EpgController& orig);

    Utf8Conv m_toUtf8;

    Artwork m_artwork;
};

#endif // ROBOTV_EPGCONTROLLER_H
