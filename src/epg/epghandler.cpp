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

EpgHandler::EpgHandler() {
    createDb();
}

bool EpgHandler::HandleEvent(cEvent* Event) {

    // skip entries from the past
    if(Event->EndTime() <= time(NULL)) {
        return false;
    }

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

    // insert new epg entry
    exec(
        "INSERT OR IGNORE INTO epgindex("
        "  docid,"
        "  eventid,"
        "  timestamp,"
        "  channelid,"
        "  channelname,"
        "  title,"
        "  subject,"
        "  description,"
        "  endtime"
        ") "
        "VALUES(%i, %u, %llu, %Q, %Q, %Q, %Q, %Q, %llu)",
        docId,
        Event->EventID(),
        (uint64_t)Event->StartTime(),
        (const char*)Event->ChannelID().ToString(),
        channelName.c_str(),
        Event->Title() ? Event->Title() : "",
        Event->ShortText() ? Event->ShortText() : "",
        Event->Description() ? Event->Description() : "",
        (uint64_t)Event->EndTime()
    );

    return false;
}

bool EpgHandler::BeginSegmentTransfer(const cChannel *Channel, bool OnlyRunningStatus) {
    if(!OnlyRunningStatus) {
        begin();
    }

    return false;
}

bool EpgHandler::EndSegmentTransfer(bool Modified, bool OnlyRunningStatus) {
    if(!OnlyRunningStatus) {
        commit();
    }

    return false;
}

void EpgHandler::createDb() {
    std::string schema =
        "CREATE TABLE IF NOT EXISTS epgindex (\n"
        "  docid INTEGER PRIMARY KEY,\n"
        "  eventid INTEGER NOT NULL,\n"
        "  timestamp INTEGER NOT NULL,\n"
        "  channelname TEXT NOT NULL,\n"
        "  channelid TEXT NOT NULL,\n"
        "  title TEXT NOT NULL,\n"
        "  subject TEXT NOT NULL,\n"
        "  description TEXT NOT NULL,\n"
        "  endtime INTEGER NOT NULL,\n"
        "  url TEXT NOT NULL DEFAULT ''\n"
        ");\n"
        "CREATE INDEX IF NOT EXISTS epgindex_timestamp on epgindex(timestamp);\n";

    if(exec(schema) != SQLITE_OK) {
        esyslog("Unable to create database schema for epg search");
    }

    // update older versions of epgindex
    if(!tableHasColumn("epgindex", "eventid")) {
        exec("ALTER TABLE epgindex ADD COLUMN eventid INTEGER NOT NULL");
    }
    if(!tableHasColumn("epgindex", "endtime")) {
        exec("DELETE FROM epgindex");
        exec("ALTER TABLE epgindex ADD COLUMN title TEXT NOT NULL DEFAULT ''");
        exec("ALTER TABLE epgindex ADD COLUMN subject TEXT NOT NULL DEFAULT ''");
        exec("ALTER TABLE epgindex ADD COLUMN description TEXT NOT NULL DEFAULT ''");
        exec("ALTER TABLE epgindex ADD COLUMN endtime INTEGER NOT NULL DEFAULT 0");
        exec("DROP TABLE epgsearch");
        exec("CREATE INDEX IF NOT EXISTS epgindex_channelid on epgindex(channelid)");
    }

    if(!tableHasColumn("epgindex", "url")) {
        exec("ALTER TABLE epgindex ADD COLUMN url TEXT NOT NULL DEFAULT ''");
    }

        // new epgsearch table
    schema =
        "CREATE VIRTUAL TABLE IF NOT EXISTS epgsearch USING fts4(\n"
        "  content=\"epgindex\",\n"
        "  title, subject\n"
        ");\n"
        "CREATE TRIGGER IF NOT EXISTS epgindex_bu BEFORE UPDATE ON epgindex BEGIN\n"
        "  DELETE FROM epgsearch WHERE docid=old.docid;\n"
        "END;\n"
        "CREATE TRIGGER IF NOT EXISTS epgindex_bd BEFORE DELETE ON epgindex BEGIN\n"
        "  DELETE FROM epgsearch WHERE docid=old.docid;\n"
        "END;\n"
        "CREATE TRIGGER IF NOT EXISTS epgindex_au AFTER UPDATE ON epgindex BEGIN\n"
        "  INSERT INTO epgsearch(docid, title, subject) VALUES(new.docid, new.title, new.subject);\n"
        "END;\n"
        "CREATE TRIGGER IF NOT EXISTS epgindex_ai AFTER INSERT ON epgindex BEGIN\n"
        "  INSERT INTO epgsearch(docid, title, subject) VALUES(new.docid, new.title, new.subject);\n"
        "END;\n";

    if(exec(schema) != SQLITE_OK) {
        esyslog("Unable to create new epgsearch fts table !");
    }
}

void EpgHandler::cleanup() {
    isyslog("removing outdated epg entries");

    time_t now = time(NULL);
    exec("DELETE FROM epgindex WHERE endtime < %llu", (uint64_t)now);
}

void EpgHandler::triggerCleanup() {
    std::thread t([ = ]() {
        cleanup();
    });

    t.detach();
}
