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

#include <thread>
#include "channelcache.h"
#include "tools/hash.h"

ChannelCache::ChannelCache() {
    createDb();
}

ChannelCache& ChannelCache::instance() {
    static ChannelCache cache;
    return cache;
}

void ChannelCache::gc() {
    // TODO - implement garbage collection
}

void ChannelCache::createDb() {
    std::string schema =
        "CREATE TABLE IF NOT EXISTS channelcache (\n"
        "  channeluid INT NOT NULL,\n"
        "  pid INT NOT NULL,\n"
        "  content INT NOT NULL,\n"
        "  type INT NOT NULL,\n"
        "  language TEXT,\n"
        "  audiotype INT DEFAULT 0,\n"
        "  fpsscale INT DEFAULT 0,\n"
        "  fpsrate INT DEFAULT 0,\n"
        "  height INT DEFAULT 0,\n"
        "  width INT DEFAULT 0,\n"
        "  aspect INT DEFAULT 1,\n"
        "  channels INT DEFAULT 0,\n"
        "  samplerate INT DEFAULT 0,\n"
        "  bitrate INT DEFAULT 0,\n"
        "  bitspersample INT DEFAULT 0,\n"
        "  blockalign INT DEFAULT 0,\n"
        "  parsed BOOLEAN DEFAULT 0,\n"
        "  subtitlingtype INT DEFAULT 0,\n"
        "  compositionpageid INT DEFAULT 0,\n"
        "  ancillarypageid INT DEFAULT 0,\n"
        "  sps BLOB,\n"
        "  pps BLOB,\n"
        "  vps BLOB,\n"
        "  PRIMARY KEY (channeluid, pid)"
        ");\n"
        "CREATE INDEX IF NOT EXISTS channelcache_channeluid ON channelcache(channeluid);\n"
        "CREATE TABLE IF NOT EXISTS enabledchannels (\n"
        "  channeluid INT NOT NULL,\n"
        "  enabled INT DEFAULT 0 NOT NULL,\n"
        "  PRIMARY KEY (channeluid)\n"
        ");\n";

    if(exec(schema) != SQLITE_OK) {
        esyslog("Unable to create database schema for channelcache");
    }
}

std::string ChannelCache::createStringLiteral(uint8_t* data, int length) {
    char buffer[3];
    std::string literal;

    for(int i = 0; i < length; i++) {
        snprintf(buffer, sizeof(buffer), "%02X", data[i]);
        literal += buffer;
    }

    return literal;
}

void ChannelCache::add(uint32_t channeluid, const StreamBundle& channel) {
    std::thread t([ = ]() {
        addDb(channeluid, channel);
    });

    t.detach();
}

void ChannelCache::addDb(uint32_t channeluid, const StreamBundle& channel) {
    begin();

    exec("DELETE FROM channelcache WHERE channeluid=%i", channeluid);

    for(auto i : channel) {
        StreamInfo& info = i.second;

        exec(
            "INSERT INTO channelcache("
            "channeluid,"
            "pid,"
            "content,"
            "type,"
            "language,"
            "audiotype,"
            "fpsscale,"
            "fpsrate,"
            "height,"
            "width,"
            "aspect,"
            "channels,"
            "samplerate,"
            "bitrate,"
            "bitspersample,"
            "blockalign,"
            "parsed,"
            "subtitlingtype,"
            "compositionpageid,"
            "ancillarypageid,"
            "sps,"
            "pps,"
            "vps) "
            "VALUES ("
            "%i,%i,%i,%i,%Q,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,x'%s',x'%s',x'%s'"
            ")",
            channeluid,
            info.m_pid,
            (int)info.m_content,
            (int)info.m_type,
            info.m_language,
            info.m_audioType,
            info.m_fpsScale,
            info.m_fpsRate,
            info.m_height,
            info.m_width,
            info.m_aspect,
            info.m_channels,
            info.m_sampleRate,
            info.m_bitRate,
            info.m_bitsPerSample,
            info.m_blockAlign,
            (int)info.m_parsed,
            info.m_subTitlingType,
            info.m_compositionPageId,
            info.m_ancillaryPageId,
            createStringLiteral(info.m_sps, info.m_spsLength).c_str(),
            createStringLiteral(info.m_pps, info.m_ppsLength).c_str(),
            createStringLiteral(info.m_vps, info.m_vpsLength).c_str()
        );
    }

    commit();
}

StreamBundle ChannelCache::lookup(uint32_t channeluid) {
    sqlite3_stmt* s = query(
                          "SELECT "
                          "  pid,"
                          "  content,"
                          "  type,"
                          "  language,"
                          "  audiotype,"
                          "  fpsscale,"
                          "  fpsrate,"
                          "  height,"
                          "  width,"
                          "  aspect,"
                          "  channels,"
                          "  samplerate,"
                          "  bitrate,"
                          "  bitspersample,"
                          "  blockalign,"
                          "  parsed,"
                          "  subtitlingtype,"
                          "  compositionpageid,"
                          "  ancillarypageid,"
                          "  sps,"
                          "  pps,"
                          "  vps "
                          "FROM "
                          "  channelcache "
                          "WHERE"
                          "  channeluid=%i",
                          channeluid
                      );

    if(s == NULL) {
        return StreamBundle();
    }

    StreamBundle bundle;

    while(sqlite3_step(s) == SQLITE_ROW) {
        StreamInfo info;
        info.m_pid = sqlite3_column_int(s, 0);
        info.m_content = (StreamInfo::Content)sqlite3_column_int(s, 1);
        info.m_type = (StreamInfo::Type)sqlite3_column_int(s, 2);
        strncpy(info.m_language, (const char*)sqlite3_column_text(s, 3), sizeof(info.m_language));
        info.m_audioType = sqlite3_column_int(s, 4);
        info.m_fpsScale = sqlite3_column_int(s, 5);
        info.m_fpsRate = sqlite3_column_int(s, 6);
        info.m_height = sqlite3_column_int(s, 7);
        info.m_width = sqlite3_column_int(s, 8);
        info.m_aspect = sqlite3_column_int(s, 9);
        info.m_channels = sqlite3_column_int(s, 10);
        info.m_sampleRate = sqlite3_column_int(s, 11);
        info.m_bitRate = sqlite3_column_int(s, 12);
        info.m_bitsPerSample = sqlite3_column_int(s, 13);
        info.m_blockAlign = sqlite3_column_int(s, 14);
        info.m_parsed = (sqlite3_column_int(s, 15) == 1);
        info.m_subTitlingType = sqlite3_column_int(s, 16);
        info.m_compositionPageId = sqlite3_column_int(s, 17);
        info.m_ancillaryPageId = sqlite3_column_int(s, 18);

        info.m_spsLength = sqlite3_column_bytes(s, 19);
        memcpy(info.m_sps, sqlite3_column_text(s, 19), info.m_spsLength);

        info.m_ppsLength = sqlite3_column_bytes(s, 20);
        memcpy(info.m_pps, sqlite3_column_text(s, 20), info.m_ppsLength);

        info.m_vpsLength = sqlite3_column_bytes(s, 21);
        memcpy(info.m_vps, sqlite3_column_text(s, 21), info.m_vpsLength);

        bundle.addStream(info);
    }

    sqlite3_finalize(s);
    return bundle;
}

void ChannelCache::enable(const cChannel* channel, bool enabled) {
    enable(createChannelUid(channel), enabled);
}

void ChannelCache::enable(uint32_t channeluid, bool enabled) {
    exec(
        "INSERT OR REPLACE INTO enabledchannels(channeluid, enabled) VALUES(%i, %i)",
        channeluid,
        (int)enabled
    );
}

bool ChannelCache::isEnabled(const cChannel* channel) {
    return isEnabled(createChannelUid(channel));
}

bool ChannelCache::isEnabled(uint32_t channeluid) {
    sqlite3_stmt* s = query(
                          "SELECT enabled WHERE channeluid=%i",
                          channeluid
                      );

    if(s == NULL) {
        return false;
    }

    if(sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s);
        return false;
    }

    bool rc = (sqlite3_column_int(s, 0) == 1);

    sqlite3_finalize(s);
    return rc;
}
