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

#include "channelcache.h"
#include "config/config.h"
#include "tools/hash.h"
#include "robotv/robotvchannels.h"

ChannelCache::ChannelCache() : m_config(RoboTVServerConfig::instance()) {
}

ChannelCache& ChannelCache::instance() {
    static ChannelCache cache;
    return cache;
}

void ChannelCache::add(uint32_t channeluid, const StreamBundle& channel) {
    lock();

    if(channeluid != 0) {
        m_cache[channeluid] = channel;
    }

    unlock();
}

void ChannelCache::add(const cChannel* channel) {
    cMutexLock lock(&m_access);

    uint32_t uid = CreateChannelUID(channel);

    // ignore invalid channels
    if(uid == 0) {
        return;
    }

    auto i = m_cache.find(uid);

    // valid channel already in cache
    if(i != m_cache.end()) {
        if(i->second.size() != 0) {
            return;
        }
    }

    // create new cache item
    StreamBundle item = StreamBundle::createFromChannel(channel);

    add(uid, item);
}

StreamBundle ChannelCache::lookup(uint32_t channeluid) {
    static StreamBundle empty;

    lock();

    auto i = m_cache.find(channeluid);

    if(i == m_cache.end()) {
        unlock();
        return empty;
    }

    StreamBundle result = m_cache[channeluid];
    unlock();

    return result;
}

void ChannelCache::save() {
    cString filename = AddDirectory(m_config.CacheDirectory.c_str(), CHANNEL_CACHE_FILE".bak");

    int fd = open(*filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if(fd == -1) {
        ERRORLOG("Unable to open channel cache data file (%s) !", (const char*)filename);
        return;
    }

    lock();

    MsgPacket* p = new MsgPacket;
    p->put_String("V2");
    p->put_U32(m_cache.size());

    for(std::map<uint32_t, StreamBundle>::iterator i = m_cache.begin(); i != m_cache.end(); i++) {
        p->put_U32(i->first);
        *p << i->second;
    }

    unlock();

    p->write(fd, 1000);
    delete p;
    close(fd);

    cString filenamenew = AddDirectory(m_config.CacheDirectory.c_str(), CHANNEL_CACHE_FILE);

    rename(filename, filenamenew);
}

void ChannelCache::gc() {
    cMutexLock lock(&m_access);
    std::map<uint32_t, StreamBundle> m_newcache;

    INFOLOG("channel cache garbage collection ...");
    INFOLOG("before: %zu channels in cache", m_cache.size());

    // remove orphaned cache entries
    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);
    cChannels* channels = c.get();

    for(cChannel* channel = channels->First(); channel; channel = channels->Next(channel)) {
        uint32_t uid = CreateChannelUID(channel);

        // ignore invalid channels
        if(uid == 0) {
            continue;
        }

        // lookup channel in current cache
        std::map<uint32_t, StreamBundle>::iterator i = m_cache.find(uid);

        if(i == m_cache.end()) {
            continue;
        }

        // add to new cache if it exists
        m_newcache[uid] = i->second;
    }

    c.unlock();

    // regenerate cache
    m_cache.clear();

    for(auto i = m_newcache.begin(); i != m_newcache.end(); i++) {
        m_cache[i->first] = i->second;
    }

    INFOLOG("after: %zu channels in cache", m_cache.size());
}

void ChannelCache::load() {
    m_cache.clear();

    // load cache
    cString filename = AddDirectory(m_config.CacheDirectory.c_str(), CHANNEL_CACHE_FILE);

    int fd = open(*filename, O_RDONLY);

    if(fd == -1) {
        ERRORLOG("Unable to open channel cache data file (%s) !", (const char*)filename);
        return;
    }

    MsgPacket* p = MsgPacket::read(fd, 1000);

    if(p == NULL) {
        ERRORLOG("Unable to load channel cache data file (%s) !", (const char*)filename);
        close(fd);
        return;
    }

    std::string version = p->get_String();

    if(version != "V2") {
        INFOLOG("old channel cache detected - skipped");
        return;
    }

    uint32_t c = p->get_U32();

    // sanity check
    if(c > 10000) {
        delete p;
        close(fd);
        return;
    }

    INFOLOG("Loading %u channels from cache", c);

    for(uint32_t i = 0; i < c; i++) {
        uint32_t uid = p->get_U32();

        StreamBundle bundle;
        *p >> bundle;

        if(uid != 0) {
            m_cache[uid] = bundle;
        }
    }

    delete p;
    close(fd);

    gc();
}
