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

#ifndef ROBOTV_CONFIG_H
#define ROBOTV_CONFIG_H

#include <string>
#include <stdint.h>

#include <vdr/config.h>

// default settings

#define ALLOWED_HOSTS_FILE  "allowed_hosts.conf"
#define GENERAL_CONFIG_FILE "robotv.conf"
#define STORAGE_DB_FILE     "storage.db"

#define LISTEN_PORT       34892

// backward compatibility

#if APIVERSNUM < 10701
#define FOLDERDELIMCHAR '~'
#endif


class RoboTVServerConfig : public cConfig<cSetupLine> {
protected:

    RoboTVServerConfig();

    bool Parse(const char* Name, const char* Value);

public:

    void Load();

    static RoboTVServerConfig& instance();

    // Remote server settings
    std::string configDirectory; // config directory path
    std::string cacheDirectory; // cache directory path
    uint16_t listenPort; // Port of remote server
    std::string piconsUrl;
    std::string reorderCmd;
    std::string epgImageUrl;
    std::string seriesFolder;
    bool filterChannels = false;
};

#endif // ROBOTV_CONFIG_H
