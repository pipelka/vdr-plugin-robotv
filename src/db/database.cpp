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

#include "database.h"
#include "config/config.h"

#include <chrono>
#include <stdlib.h>

using namespace RoboTV;

Database::Database() : m_db(NULL) {
}

Database::~Database() {
    Close();
}

bool Database::Open(const std::string& db) {
    {
        std::lock_guard<std::mutex> lock(m_lock);

        if(m_db != NULL) {
            return false;
        }

        INFOLOG("Opening database: %s", db.c_str());
        int rc = sqlite3_open_v2(db.c_str(), &m_db,
                                 SQLITE_OPEN_SHAREDCACHE |
                                 SQLITE_OPEN_FULLMUTEX |
                                 SQLITE_OPEN_READWRITE |
                                 SQLITE_OPEN_CREATE,
                                 NULL);

        if(rc != SQLITE_OK) {
            ERRORLOG("Can't open database: %s", sqlite3_errmsg(m_db));
            sqlite3_close(m_db);
            m_db = NULL;
            return false;
        }
    }

    Exec("SELECT icu_load_collation('utf-8', 'unicode');");
    return (Exec("PRAGMA journal_mode = WAL;") == SQLITE_OK);
}

bool Database::Close() {
    std::lock_guard<std::mutex> lock(m_lock);

    if(m_db == NULL) {
        return false;
    }

    INFOLOG("Closing database.");

    sqlite3_close_v2(m_db);
    m_db = NULL;

    return true;
}

bool Database::IsOpen() {
    std::lock_guard<std::mutex> lock(m_lock);
    return (m_db != NULL);
}

char* Database::PrepareQueryBuffer(const std::string& query, va_list ap) {
    return sqlite3_vmprintf(query.c_str(), ap);
}

void Database::ReleaseQueryBuffer(char* querybuffer) {
    sqlite3_free(querybuffer);
}

int Database::Exec(const std::string& query, ...) {
    std::lock_guard<std::mutex> lock(m_lock);

    if(m_db == NULL) {
        return SQLITE_NOTADB;
    }

    va_list ap;
    va_start(ap, &query);
    char* querybuffer = PrepareQueryBuffer(query, ap);

    char* errmsg = NULL;
    std::chrono::milliseconds duration(10);

    int rc = SQLITE_OK;

    for(;;) {
        if(errmsg != NULL) {
            sqlite3_free(errmsg);
            errmsg = NULL;
        }

        rc = sqlite3_exec(m_db, querybuffer, NULL, NULL, &errmsg);

        if(rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            std::this_thread::sleep_for(duration);
        }
        else {
            break;
        }
    }

    ReleaseQueryBuffer(querybuffer);

    if(rc != SQLITE_OK && errmsg != NULL) {
        ERRORLOG("SQLite: %s", errmsg);
        sqlite3_free(errmsg);
    }

    return rc;
}

sqlite3_stmt* Database::Query(const std::string& query, ...) {
    std::lock_guard<std::mutex> lock(m_lock);

    if(m_db == NULL) {
        return NULL;
    }

    va_list ap;
    va_start(ap, &query);
    char* querybuffer = PrepareQueryBuffer(query, ap);

    sqlite3_stmt* stmt = NULL;
    int rc = SQLITE_OK;
    std::chrono::milliseconds duration(10);

    for(;;) {
        rc = sqlite3_prepare_v2(m_db, querybuffer, -1, &stmt, NULL);

        if(rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            std::this_thread::sleep_for(duration);
        }
        else {
            break;
        }
    }

    ReleaseQueryBuffer(querybuffer);

    if(rc != SQLITE_OK) {
        ERRORLOG("SQLite: %s", sqlite3_errmsg(m_db));

        if(stmt != NULL) {
            sqlite3_finalize(stmt);
        }

        return NULL;
    }

    return stmt;
}

sqlite3_blob* Database::OpenBlob(const std::string& table, const std::string& column, int64_t rowid, bool write) {
    std::lock_guard<std::mutex> lock(m_lock);

    if(m_db == NULL) {
        return NULL;
    }

    sqlite3_blob* blob = NULL;
    int rc = sqlite3_blob_open(m_db, "main", table.c_str(), column.c_str(), rowid, write, &blob);

    if(rc != SQLITE_OK || blob == NULL) {
        return NULL;
    }

    return blob;
}

bool Database::Begin() {
    return (Exec("BEGIN;") == SQLITE_OK);
}

bool Database::Commit() {
    return (Exec("COMMIT;") == SQLITE_OK);
}

bool Database::Rollback() {
    return (Exec("ROLLBACK;") == SQLITE_OK);
}

bool Database::TableHasColumn(const std::string& table, const std::string& column) {
    sqlite3_stmt* s = Query("PRAGMA table_info(%s);", table.c_str());

    if(s == NULL) {
        return false;
    }

    bool columnFound = false;

    while(sqlite3_step(s) == SQLITE_ROW) {
        std::string col = (const char*)sqlite3_column_text(s, 1);

        if(col == column) {
            columnFound = true;
            break;
        }
    }

    sqlite3_finalize(s);
    return columnFound;
}
