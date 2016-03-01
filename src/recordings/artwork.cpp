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
#include "recordings/artwork.h"
#include "config/config.h"

Artwork::Artwork() : m_storage(roboTV::Storage::getInstance()) {
    createDb();
}

Artwork::~Artwork() {
}

void Artwork::createDb() {
    std::string schema =
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
        "CREATE UNIQUE INDEX IF NOT EXISTS artwork_content on artwork(contenttype, title);\n";

    if(m_storage.exec(schema) != SQLITE_OK) {
        ERRORLOG("Unable to create database schema for artwork");
    }
}

bool Artwork::get(int contentType, const std::string& title, std::string& posterUrl, std::string& backdropUrl) {
    sqlite3_stmt* s = m_storage.query(
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
    if(m_storage.exec(
                "INSERT OR IGNORE INTO artwork(contenttype, title, posterurl, backgroundurl, externalId) VALUES(%i, %Q, %Q, %Q, %i);",
                contentType,
                title.c_str(),
                posterUrl.c_str(),
                backdropUrl.c_str(),
                externalId) == SQLITE_OK) {
        return true;
    }

    return m_storage.exec(
               "UPDATE artwork SET posterurl=%Q, backgroundurl=%Q, externalId=%i WHERE contenttype=%i AND title=%Q",
               posterUrl.c_str(),
               backdropUrl.c_str(),
               externalId,
               contentType,
               title.c_str()) == SQLITE_OK;
}

void Artwork::cleanup(int afterDays) {
    m_storage.exec(
        "DELETE FROM artwork WHERE julianday('now') - julianday(timestamp) > %i AND backgroundurl=''",
        afterDays
    );
}

void Artwork::triggerCleanup(int afterDays) {
    std::thread t([ = ]() {
        cleanup(afterDays);
    });

    t.detach();
}
