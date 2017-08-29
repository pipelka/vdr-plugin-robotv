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

#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>

#include "config/config.h"
#include "recordingscache.h"
#include "tools/hash.h"

RecordingsCache::RecordingsCache() {
    // create db schema
    createDb();
}

void RecordingsCache::update(const cRecordings* recordings) {
    for(const cRecording* recording = recordings->First(); recording; recording = recordings->Next(recording)) {
        add(recording);
    }
}

RecordingsCache::~RecordingsCache() {
}

RecordingsCache& RecordingsCache::instance() {
    static RecordingsCache singleton;
    return singleton;
}

uint32_t RecordingsCache::update(uint32_t uid, const cRecording* recording) {
    cString filename = recording->FileName();
    uint32_t newUid = createStringHash(filename);

    // update existing record
    exec(
            "UPDATE recordings set recid=%u, filename=%Q WHERE recid=%u;",
            newUid,
            (const char*)filename,
            uid);

    return newUid;
}

uint32_t RecordingsCache::add(const cRecording* recording) {
    cString filename = recording->FileName();
    uint32_t uid = createStringHash(filename);

    // try to update existing record
    exec(
        "INSERT OR IGNORE INTO recordings(recid, filename) VALUES(%u, %Q);",
        uid,
        (const char*)filename);

    // insert full text search entry
    exec(
        "INSERT OR IGNORE INTO fts_recordings(docid, title, subject, description) VALUES(%u, %Q, %Q, %Q);",
        uid,
        !isempty(recording->Info()->Title()) ? recording->Info()->Title() : "",
        !isempty(recording->Info()->ShortText()) ? recording->Info()->ShortText() : "",
        !isempty(recording->Info()->Description()) ? recording->Info()->Description() : "");

    return uid;
}

const cRecording* RecordingsCache::lookup(const cRecordings* recordings, const std::string& fileName) {
    return recordings->GetByName(fileName.c_str());
}

const cRecording* RecordingsCache::lookup(const cRecordings* recordings, uint32_t uid) {
    return const_cast<cRecording*>(lookup((cRecordings*)recordings, uid));
}

cRecording* RecordingsCache::lookup(cRecordings* recordings, uint32_t uid) {
    dsyslog("%s - lookup uid: %08x", __FUNCTION__, uid);

    sqlite3_stmt* s = query("SELECT filename FROM recordings WHERE recid=%u;", uid);

    if(s == NULL) {
        dsyslog("%s - not found !", __FUNCTION__);
        return NULL;
    }

    cString filename;

    if(sqlite3_step(s) == SQLITE_ROW) {
        filename = (const char*)sqlite3_column_text(s, 0);
    }

    sqlite3_finalize(s);

    if(isempty(filename)) {
        dsyslog("%s - empty filename for uid: %08x !", __FUNCTION__, uid);
        return NULL;
    }

    dsyslog("%s - filename: %s", __FUNCTION__, (const char*)filename);

    cRecording* r = recordings->GetByName(filename);
    dsyslog("%s - recording %s", __FUNCTION__, (r == NULL) ? "not found !" : "found");

    return r;
}

void RecordingsCache::setPlayCount(uint32_t uid, int count) {
    exec(
        "UPDATE recordings SET playcount=%i WHERE recid=%u;",
        count,
        uid);
}

void RecordingsCache::setLastPlayedPosition(uint32_t uid, uint64_t position) {
    exec(
        "UPDATE recordings SET position=%llu WHERE recid=%u;",
        position,
        uid);
}

void RecordingsCache::setPosterUrl(uint32_t uid, const char* url) {
    exec(
        "UPDATE recordings SET posterurl=%Q WHERE recid=%u;",
        url,
        uid);
}

void RecordingsCache::setBackgroundUrl(uint32_t uid, const char* url) {
    exec(
        "UPDATE recordings SET backgroundurl=%Q WHERE recid=%u;",
        url,
        uid);
}

void RecordingsCache::setMovieID(uint32_t uid, uint32_t id) {
    exec(
        "UPDATE recordings SET externalid=%u WHERE recid=%u;",
        id,
        uid);
}

int RecordingsCache::getPlayCount(uint32_t uid) {
    sqlite3_stmt* s = query("SELECT playcount FROM recordings WHERE recid=%u;", uid);

    if(s == NULL) {
        return 0;
    }

    int count = 0;

    if(sqlite3_step(s) == SQLITE_ROW) {
        count = sqlite3_column_int(s, 0);
    }

    sqlite3_finalize(s);
    return count;
}

cString RecordingsCache::getPosterUrl(uint32_t uid) {
    cString url = "";
    sqlite3_stmt* s = query("SELECT posterurl FROM recordings WHERE recid=%u;", uid);

    if(s == NULL) {
        return url;
    }

    if(sqlite3_step(s) == SQLITE_ROW) {
        const char* u = (const char*)sqlite3_column_text(s, 0);

        if(u != NULL) {
            url = u;
        }
    }

    sqlite3_finalize(s);
    return url;
}

cString RecordingsCache::getBackgroundUrl(uint32_t uid) {
    cString url = "";
    sqlite3_stmt* s = query("SELECT backgroundurl FROM recordings WHERE recid=%u;", uid);

    if(s == NULL) {
        return url;
    }

    if(sqlite3_step(s) == SQLITE_ROW) {
        const char* u = (const char*)sqlite3_column_text(s, 0);

        if(u != NULL) {
            url = u;
        }
    }

    sqlite3_finalize(s);
    return url;
}

uint64_t RecordingsCache::getLastPlayedPosition(uint32_t uid) {
    sqlite3_stmt* s = query("SELECT position FROM recordings WHERE recid=%u;", uid);

    if(s == NULL) {
        return 0;
    }

    uint64_t position = 0;

    if(sqlite3_step(s) == SQLITE_ROW) {
        position = sqlite3_column_int64(s, 0);
    }

    sqlite3_finalize(s);
    return position;
}

void RecordingsCache::triggerCleanup() {
    std::thread t([=]() {
        LOCK_RECORDINGS_READ;
        gc(Recordings);
    });

    t.detach();
}

void RecordingsCache::gc(const cRecordings* recordings) {
    RecordingsCache storage;

    storage.begin();
    storage.exec("DELETE FROM fts_recordings;");
    storage.update(recordings);

    sqlite3_stmt* s = storage.query("SELECT recid, filename FROM recordings;");

    if(s == nullptr) {
        storage.rollback();
        return;
    }

    // check all recordings in the cache

    while(sqlite3_step(s) == SQLITE_ROW) {
        uint32_t recid = (uint32_t)sqlite3_column_int(s, 0);
        const char* filename = (const char*)sqlite3_column_text(s, 1);

        // remove orphaned entry
        if(recordings->GetByName(filename) == nullptr) {
            isyslog("removing outdated recording '%s' from cache", filename);
            storage.exec("DELETE FROM recordings WHERE recid=%u;", recid);
        }
    }

    storage.commit();
    sqlite3_finalize(s);
}

void RecordingsCache::createDb() {
    std::string schema =
        "CREATE TABLE IF NOT EXISTS recordings (\n"
        "  recid INTEGER PRIMARY KEY,\n"
        "  filename TEXT NOT NULL,\n"
        "  position BIGINT DEFAULT 0,\n"
        "  playcount INTEGER DEFAULT 0,\n"
        "  posterurl TEXT,\n"
        "  backgroundurl TEXT,\n"
        "  externalid INTEGER\n"
        ");\n"
        "CREATE INDEX IF NOT EXISTS recordings_externalid on recordings(externalid);\n"
        "CREATE UNIQUE INDEX IF NOT EXISTS recordings_filename on recordings(filename);\n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS fts_recordings USING fts4(title, subject, description);\n";

    if(exec(schema) != SQLITE_OK) {
        esyslog("Unable to create database schema for recordings");
    }
}

void RecordingsCache::search(const char* searchTerm, std::function<void(uint32_t)> resultCallback) {
    if(searchTerm == NULL) {
        return;
    }

    auto stmt = query(
                    "SELECT docid "
                    "FROM fts_recordings "
                    "WHERE fts_recordings MATCH %Q "
                    "LIMIT 100",
                    searchTerm);

    if(stmt == NULL) {
        return;
    }

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        uint32_t recid = (uint32_t)sqlite3_column_int(stmt, 0);
        resultCallback(recid);
    }

    sqlite3_finalize(stmt);
}
