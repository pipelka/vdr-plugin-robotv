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

#ifndef ROBOTV_ARTWORK_H
#define	ROBOTV_ARTWORK_H

#include "db/storage.h"
#include <string>

class Artwork {
public:

    Artwork();

    virtual ~Artwork();

    bool get(int contentType, const std::string& title, std::string& posterUrl, std::string& backdropUrl);

    bool set(int contentType, const std::string& title, const std::string& posterUrl, const std::string& backdropUrl, int externalId);

    void cleanup(int afterDays = 4);

    void triggerCleanup(int afterDays = 4);

private:

    void createDb();

    roboTV::Storage& m_storage;

};

#endif	// ROBOTV_ARTWORK_H
