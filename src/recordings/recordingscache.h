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

#ifndef ROBOTV_RECORDINGSCACHE_H
#define ROBOTV_RECORDINGSCACHE_H

#include <stdint.h>
#include <map>
#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/recording.h>
#include "db/storage.h"

class RecordingsCache {
protected:

    RecordingsCache();

    virtual ~RecordingsCache();

public:

    static RecordingsCache& instance();

    uint32_t add(cRecording* recording);

    cRecording* lookup(uint32_t uid);

    int getPlayCount(uint32_t uid);

    void setPlayCount(uint32_t uid, int count);

    uint64_t getLastPlayedPosition(uint32_t uid);

    void setLastPlayedPosition(uint32_t uid, uint64_t position);

    cString getPosterUrl(uint32_t uid);

    void setPosterUrl(uint32_t uid, const char* url);

    cString getBackgroundUrl(uint32_t uid);

    void setBackgroundUrl(uint32_t uid, const char* url);

    void setMovieID(uint32_t uid, uint32_t id);

    void gc();

protected:

    void update();

    void createDb();

private:

    RoboTV::Storage& m_storage;
};


#endif // ROBOTV_RECORDINGSCACHE_H
