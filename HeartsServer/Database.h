#pragma once
// ---------------------------------------------------------------------------
// Database.h - a small wrapper around the SQLite database file.
//
// The rest of the server never writes SQL. It calls methods like
// findPlayer() or markLoggedIn(), and all the SQL lives in Database.cpp.
//
// SQLite comes built into Qt (the Qt6::Sql module, "QSQLITE" driver).
// The whole database is one file, hearts.db, next to the .exe.
//
// Table: players   (schema version 2)
//   player_id        INTEGER  auto-numbered primary key
//   username         TEXT     unique
//   email            TEXT     stored lowercase
//   password_hash    TEXT     SHA-256(password_salt + password), hex
//   password_salt    TEXT     random, hex
//   pin_hash         TEXT     SHA-256(pin_salt + pin), hex (for recovery)
//   pin_salt         TEXT     random, hex
//   last_logged_in   TEXT     ISO timestamp, e.g. "2026-11-16T23:41:07"
//   last_logged_out  TEXT     ISO timestamp
//   connected        INTEGER  0 or 1
//
// SCHEMA VERSION: SQLite has a built-in number slot, "PRAGMA user_version",
// that we use to remember which table layout the file has. If it's older
// than kSchemaVersion, the old players table is renamed to players_old
// (nothing is deleted) and a fresh one is created. When you change the
// table layout, bump kSchemaVersion in Database.cpp.
// ---------------------------------------------------------------------------

#include "PasswordHasher.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>

// One row of the players table, as plain C++ values.
struct PlayerRecord
{
    int id = -1;
    QString username;
    QString email;
    QString passwordHash;
    QString passwordSalt;
    QString pinHash;
    QString pinSalt;
    bool connected = false;
};

class Database
{
public:
    ~Database();

    // Opens (or creates) the file and makes sure the table layout is current.
    // Returns false if anything goes wrong (the reason is logged).
    bool open(const QString& filePath);

    // Looks up a player by username. std::nullopt if not found.
    std::optional<PlayerRecord> findPlayer(const QString& username);

    // Inserts a new player. Returns the new player_id, or -1 on failure.
    // The password and pin arrive already hashed (see PasswordHasher).
    int createPlayer(const QString& username, const QString& email,
                     const PasswordHasher::Hashed& password,
                     const PasswordHasher::Hashed& pin);

    // Replaces a player's password hash + salt. Used by password recovery.
    bool updatePassword(int playerId, const PasswordHasher::Hashed& password);

    // connected = 1, last_logged_in = now
    void markLoggedIn(int playerId);

    // connected = 0, last_logged_out = now
    void markLoggedOut(int playerId);

    // connected = 0 for everyone. Called at startup in case the server
    // crashed while people were logged in.
    void resetAllConnected();

private:
    // Checks the schema version, moves an old table aside if needed, and
    // creates the players table.
    bool prepareSchema();

    QSqlDatabase m_db;
};
