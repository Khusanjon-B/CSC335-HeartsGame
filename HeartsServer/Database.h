#pragma once
// ---------------------------------------------------------------------------
// Database.h - a small wrapper around the SQLite database file.
//
// The rest of the server never writes SQL directly. It calls methods like
// findPlayer() or markLoggedIn(), and all the SQL lives in Database.cpp.
// If the table changes later, Database.cpp is the only file to edit.
//
// SQLite support comes from Qt itself (the Qt6::Sql module and its built-in
// "QSQLITE" driver), so no separate SQLite install or setup is needed.
// The whole database is one file (hearts.db) created next to the .exe.
//
// Table: players
//   player_id        INTEGER  auto-numbered primary key (1, 2, 3, ...)
//   username         TEXT     unique, can't be empty
//   password         TEXT     PLAIN TEXT FOR NOW (debugging), hash this later
//   last_logged_in   TEXT     ISO timestamp, e.g. "2026-11-09T23:41:07"
//   last_logged_out  TEXT     ISO timestamp
//   connected        INTEGER  0 or 1 (SQLite has no real bool type)
// ---------------------------------------------------------------------------

#include <QSqlDatabase>
#include <QString>
#include <optional>

// One row of the players table, as plain C++ values.
struct PlayerRecord
{
    int id = -1;
    QString username;
    QString password;
    bool connected = false;
};

class Database
{
public:
    ~Database();

    // Opens (or creates) the database file and makes sure the players table
    // exists. Returns false if anything goes wrong; the reason is logged.
    bool open(const QString& filePath);

    // Looks up a player by username. Returns std::nullopt if there's no such
    // player (or the query failed).
    std::optional<PlayerRecord> findPlayer(const QString& username);

    // Inserts a new player. Returns the new player_id, or -1 on failure
    // (for example, the username is already taken).
    int createPlayer(const QString& username, const QString& password);

    // Sets connected = 1 and last_logged_in = now.
    void markLoggedIn(int playerId);

    // Sets connected = 0 and last_logged_out = now.
    void markLoggedOut(int playerId);

    // Sets connected = 0 for every player. Called at startup: if the server
    // crashed last time, some rows could still say connected = 1, but
    // nobody can be connected to a server that just started.
    void resetAllConnected();

private:
    // Runs CREATE TABLE IF NOT EXISTS for every table we use.
    bool createTables();

    QSqlDatabase m_db;
};
