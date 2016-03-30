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

#include <string.h>
#include <stdlib.h>

#include <vdr/plugin.h>
#include <vdr/videodir.h>

#include "config.h"
#include "live/livequeue.h"

RoboTVServerConfig::RoboTVServerConfig() : listenPort(LISTEN_PORT) {
}

void RoboTVServerConfig::Load() {
    LiveQueue::setTimeShiftDir(cVideoDirectory::Name());

    if(!cConfig<cSetupLine>::Load(AddDirectory(configDirectory.c_str(), GENERAL_CONFIG_FILE), true, false)) {
        return;
    }

    for(cSetupLine* l = First(); l; l = Next(l)) {
        if(!Parse(l->Name(), l->Value())) {
            ERRORLOG("Unknown config parameter %s = %s in %s", l->Name(), l->Value(), GENERAL_CONFIG_FILE);
        }
    }

    LiveQueue::removeTimeShiftFiles();
}

bool RoboTVServerConfig::Parse(const char* Name, const char* Value) {
    if(!strcasecmp(Name, "TimeShiftDir")) {
        LiveQueue::setTimeShiftDir(Value);
    }
    else if(!strcasecmp(Name, "MaxTimeShiftSize")) {
        LiveQueue::setBufferSize(strtoull(Value, NULL, 10));
    }
    else if(!strcasecmp(Name, "PiconsURL")) {
        piconsUrl = Value;
    }
    else if(!strcasecmp(Name, "ReorderCmd")) {
        reorderCmd = Value;
    }
    else if(!strcasecmp(Name, "EpgImageUrl")) {
        INFOLOG("EPG images template URL: %s", Value);
        epgImageUrl = Value;
    }
    else if(!strcasecmp(Name, "SeriesFolder")) {
        INFOLOG("Folder for TV shows: %s", Value);
        seriesFolder = Value;
    }
    else if(!strcasecmp(Name, "FilterChannels")) {
        filterChannels = (strcmp(Value, "true") == 0);
    }
    else {
        return false;
    }

    return true;
}

RoboTVServerConfig& RoboTVServerConfig::instance() {
    static RoboTVServerConfig config;
    return config;
}
