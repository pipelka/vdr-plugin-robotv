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

#ifndef ROBOTV_ARTWORKCONTROLLER_H
#define ROBOTV_ARTWORKCONTROLLER_H

#include "controller.h"
#include "recordings/artwork.h"

class MsgPacket;

class ArtworkController : public Controller {
public:

    ArtworkController();

    virtual ~ArtworkController();

    bool process(MsgPacket* request, MsgPacket* response);

protected:

    bool processGet(MsgPacket* request, MsgPacket* response);

    bool processSet(MsgPacket* request, MsgPacket* response);

private:

    ArtworkController(const ArtworkController& orig);

    Artwork m_artwork;
};

#endif // ROBOTV_ARTWORKCONTROLLER_H
