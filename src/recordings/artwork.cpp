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
#include <vdr/channels.h>
#include <tools/hash.h>
#include "recordings/artwork.h"

Artwork::Artwork() {
    createDb();
}

Artwork::~Artwork() {
}

void Artwork::createDb() {
    const char* schema =
        "CREATE TABLE IF NOT EXISTS artwork (\n"
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "  contenttype INTEGER NOT NULL,\n"
        "  title TEXT NOT NULL,\n"
        "  externalid INTEGER,\n"
        "  posterurl TEXT,\n"
        "  backgroundurl TEXT\n,"
        "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL\n"
        ");\n"

        "CREATE INDEX IF NOT EXISTS artwork_externalid on artwork(externalid);\n"
        "CREATE UNIQUE INDEX IF NOT EXISTS artwork_content on artwork(contenttype, title);\n"

        "DROP TABLE IF EXISTS epgsearch;\n"
        "DROP TABLE IF EXISTS epgindex;\n"

        "CREATE TABLE IF NOT EXISTS epgartwork (\n"
        "  eventid INTEGER NOT NULL,\n"
        "  channeluid INTEGER NOT NULL,\n"
        "  contentid INTEGER NOT NULL,\n"
        "  timestamp INTEGER NOT NULL,\n"
        "  url TEXT,\n"
        "  posterurl TEXT,\n"
        "  PRIMARY KEY(channeluid, eventid)\n"
        ");\n";

    if(exec(schema) != SQLITE_OK) {
        esyslog("Unable to create database schema for artwork");
    }
}

bool Artwork::get(int contentType, const std::string& title, std::string& posterUrl, std::string& backdropUrl) {
    sqlite3_stmt* s = query(
                          "SELECT posterurl, backgroundurl FROM artwork WHERE contenttype=%i AND title=%Q;",
                          contentType,
                          title.c_str());

    if(s == NULL) {
        return false;
    }

    if(sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s);
        return false;
    }

    posterUrl = (const char*)sqlite3_column_text(s, 0);
    backdropUrl = (const char*)sqlite3_column_text(s, 1);

    sqlite3_finalize(s);

    return true;
}

bool Artwork::set(int contentType, const std::string& title, const std::string& posterUrl, const std::string& backdropUrl, int externalId = 0) {
    // try to insert new record
    if(exec(
                "INSERT OR IGNORE INTO artwork(contenttype, title, posterurl, backgroundurl, externalId) VALUES(%i, %Q, %Q, %Q, %i);",
                contentType,
                title.c_str(),
                posterUrl.c_str(),
                backdropUrl.c_str(),
                externalId) == SQLITE_OK) {
        return true;
    }

    return exec(
               "UPDATE artwork SET posterurl=%Q, backgroundurl=%Q, externalId=%i WHERE contenttype=%i AND title=%Q",
               posterUrl.c_str(),
               backdropUrl.c_str(),
               externalId,
               contentType,
               title.c_str()) == SQLITE_OK;
}

void Artwork::cleanup(int afterDays) {

    // remove empty artwork
    exec(
        "DELETE FROM artwork WHERE julianday('now') - julianday(timestamp) > %i AND backgroundurl=''",
        afterDays
    );

    // remove old entries
    exec("DELETE FROM artwork WHERE julianday('now') - julianday(timestamp) > 31");

    // remove outdated epg artwork (older than 3 days"
    uint64_t timestamp = (uint64_t)time(nullptr) - (60 * 60 * 24 * 3);
    exec("DELETE FROM epgartwork WHERE timestamp < %llu", timestamp);
}

void Artwork::triggerCleanup(int afterDays) {
    std::thread t([ = ]() {
        cleanup(afterDays);
    });

    t.detach();
}

bool Artwork::setEpgImage(const Artwork::Holder& holder) {
    dsyslog(
        "set epg image (channelUid: %i, eventid: %i) '%s' (contentid: %i)",
        holder.channelUid,
        holder.eventId,
        holder.backdropUrl.c_str(),
        holder.contentId);
    dsyslog("timestamp: %li", holder.timestamp);
    dsyslog("backgroundurl: '%s'", holder.backdropUrl.c_str());

    return exec(
            "INSERT OR REPLACE INTO epgartwork(eventid,channeluid,contentid,timestamp,url,posterurl) "
            "VALUES(%i,%i,%i,%i,%Q,%Q);",
            holder.eventId,
            holder.channelUid,
            holder.contentId,
            holder.timestamp,
            holder.backdropUrl.c_str(),
            holder.posterUrl.c_str()
    ) == SQLITE_OK;
}

bool Artwork::getEpgImage(uint32_t channelUid, uint32_t eventId, Artwork::Holder& holder) {
    holder.backdropUrl = "x";
    holder.posterUrl = "x";

    sqlite3_stmt* s = query(
        "SELECT url, posterurl, contentid FROM epgartwork WHERE eventid=%i AND channeluid=%i;",
        eventId,
        channelUid);

    if(s == NULL) {
        return false;
    }

    if(sqlite3_step(s) != SQLITE_ROW) {
        sqlite3_finalize(s);
        return false;
    }

    holder.backdropUrl = (const char*)sqlite3_column_text(s, 0);
    holder.posterUrl = (const char*)sqlite3_column_text(s, 1);
    holder.contentId = (uint32_t)sqlite3_column_int(s, 2);
    holder.eventId = eventId;
    holder.channelUid = channelUid;

    sqlite3_finalize(s);

    return true;
}
