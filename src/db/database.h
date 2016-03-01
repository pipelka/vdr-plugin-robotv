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

#ifndef ROBOTV_DATABASE_H
#define ROBOTV_DATABASE_H

#include "sqlite3.h"

#include <thread>
#include <mutex>
#include <string>

namespace roboTV {

/** @short Database backend.
 * This class build the interface to SQLite
 */

class Database {
public:

    /** @short Database constructor.
     */
    Database();

    /** @short Database destructor.
     */
    virtual ~Database();

    /** @short Open a SQLite database file.
     * @param db path to database file
     * @return true - on success
     */
    bool open(const std::string& db);

    /** @short Close an open database.
     * @return true - on success
     */
    bool close();

    /** @short Check if database is already opened.
     * @return true - database is already open
     */
    bool isOpen();

    /** @short Execute a SQL statement.
     * This funtion doesn't return resultsets. For "SELECT" style queries please use Query().
     * @param query The SQL statement to execute. Accepts "printf" style formatting.
     * @return the SQLite return code.
     */
    int exec(const std::string& query, ...);

    /** @short Begin transaction.
     * Starts a new transaction block. Writing many configuration values can
     * be accelerated by using Begin() and Commit()
     */
    bool begin();

    /** @short Commit transaction.
     * Commits a previously started transaction block. Writing many configuration values can
     * be accelerated by using Begin() and Commit()
     */
    bool commit();

    /** @short Rollback transaction.
     * Discards the current transaction block.
     */
    bool rollback();

    /** @short Execute a SQL query.
     * Executes the statement and returns a SQLite resultset.
     * @param query The SQL statement to execute. Accepts "printf" style formatting.
     * @return pointer to sqlite3_stmt containing the resultset or NULL on failure
     */
    sqlite3_stmt* query(const std::string& query, ...);

    /** @short Open a blob.
     * Opens a binary large object stored in a database table.
     * @param table name of the database table
     * @param column name of the column
     * @param rowid rowid of the row containing the blob
     * @param write enable write access.
     * @return pointer to the sqlite3_blob structure or NULL on failure.
     */
    sqlite3_blob* openBlob(const std::string& table, const std::string& column, int64_t rowid, bool write = false);

    /** @short Check for columns.
     * Check if a specific table contains the given column.
     * @param table name of the table
     * @param column name of the column to check
     * return true if table contains column, otherwise false
     */
    bool tableHasColumn(const std::string& table, const std::string& column);

private:

    char* prepareQueryBuffer(const std::string& query, va_list ap);

    void releaseQueryBuffer(char* querybuffer);

    sqlite3* m_db;

    std::mutex m_lock;
};

} // namespace RoboTV

#endif // ROBOTV_DATABASE_H
