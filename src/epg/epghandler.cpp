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

#include <robotv/robotvchannels.h>
#include <tools/hash.h>
#include "epghandler.h"

EpgHandler::EpgHandler() : m_storage(roboTV::Storage::getInstance()) {
    createDb();
}

bool EpgHandler::HandleEvent(cEvent* Event) {
    std::string channelName;

    RoboTVChannels& c = RoboTVChannels::instance();
    c.lock(false);
    cChannels* channels = c.get();
    cChannel* channel = channels->GetByChannelID(Event->ChannelID());

    if(channel != nullptr) {
        channelName = channel->Name();
    }

    c.unlock();

    std::string docIdString = (const char*)Event->ChannelID().ToString();
    docIdString += "-" + std::to_string(Event->EventID());

    int docId = createStringHash(docIdString.c_str());

    m_storage.exec(
        "INSERT OR REPLACE INTO epgindex(docid,eventid,timestamp,channelid,channelname) VALUES(%i, %u, %llu, %Q, %Q)",
        docId,
        Event->EventID(),
        (uint64_t)Event->StartTime(),
        (const char*)Event->ChannelID().ToString(),
        channelName.c_str()
    );

    m_storage.exec(
        "INSERT OR REPLACE INTO epgsearch(docid, title, subject) VALUES(%i, %Q, %Q)",
        docId,
        Event->Title() ? Event->Title() : "",
        Event->ShortText() ? Event->ShortText() : ""
    );
    return false;
}

void EpgHandler::createDb() {
    std::string schema =
        "CREATE TABLE IF NOT EXISTS epgindex (\n"
        "  docid INTEGER PRIMARY KEY,\n"
        "  eventid INTEGER NOT NULL,\n"
        "  timestamp INTEGER NOT NULL,\n"
        "  channelname TEXT NOT NULL,\n"
        "  channelid TEXT NOT NULL\n"
        ");\n"
        "CREATE INDEX IF NOT EXISTS epgindex_timestamp on epgindex(timestamp);\n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS epgsearch USING fts4(\n"
        "  content=\"\",\n"
        "  title,\n"
        "  subject\n"
        ");\n";

    if(m_storage.exec(schema) != SQLITE_OK) {
        esyslog("Unable to create database schema for epg search");
    }

    // update older version of table
    if(!m_storage.tableHasColumn("epgindex", "eventid")) {
        m_storage.exec("ALTER TABLE epgindex ADD COLUMN eventid INTEGER NOT NULL");
    }
}

void EpgHandler::cleanup() {
    isyslog("removing outdated epg entries");

    time_t now = time(NULL);
    m_storage.exec("DELETE FROM epgsearch WHERE docid IN (SELECT docid FROM epgindex WHERE timestamp < %llu)", (uint64_t)now);
    m_storage.exec("DELETE FROM epgindex WHERE timestamp < %llu", (uint64_t)now);
}

void EpgHandler::triggerCleanup() {
    std::thread t([ = ]() {
        cleanup();
    });

    t.detach();
}
