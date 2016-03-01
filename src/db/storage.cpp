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

#include "storage.h"
#include "config/config.h"
#include "vdr/tools.h"

using namespace roboTV;

Storage::Storage() : m_config(RoboTVServerConfig::instance()) {
    cString filename = AddDirectory(m_config.cacheDirectory.c_str(), STORAGE_DB_FILE);

    if(!open((const char*)filename)) {
        ERRORLOG("Unable to open database: '%s' - strange thing will happen !", (const char*)filename);
    }
}

Storage::~Storage() {
    close();
}

Storage& Storage::getInstance() {
    static Storage instance;
    return instance;
}
