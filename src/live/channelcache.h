/*
 *      vdr-plugin-xvdr - XVDR server plugin for VDR
 *
 *      Copyright (C) 2011 Alexander Pipelka
 *
 *      https://github.com/pipelka/vdr-plugin-xvdr
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

#ifndef XVDR_CHANNELCACHE_H
#define XVDR_CHANNELCACHE_H

#include "vdr/thread.h"
#include "demuxer/streambundle.h"

class cChannelCache {
public:

    static void LoadChannelCacheData();

    static void SaveChannelCacheData();

    static void AddToCache(uint32_t channeluid, const cStreamBundle& channel);

    static void AddToCache(const cChannel* channel);

    static cStreamBundle GetFromCache(uint32_t channeluid);

    static void gc();

private:

    static void Lock() {
        m_access.Lock();
    }

    static void Unlock() {
        m_access.Unlock();
    }

    static std::map<uint32_t, cStreamBundle> m_cache;

    static cMutex m_access;

};

#endif // XVDR_CHANNELCACHE_H
