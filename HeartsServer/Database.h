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
// Table: players   (schema version 3)
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
//   failed_attempts  INTEGER  consecutive failed logins (reset on success)   [v3]
//   locked_until     TEXT     ISO timestamp; locked while this is in the
//                             future, NULL/past = not locked                   [v3]
//
// SCHEMA VERSION: SQLite has a built-in number slot, "PRAGMA user_version",
// that remembers which table layout the file has. See prepareSchema() in
// Database.cpp for how older files are upgraded.
//
// ERRORS: methods the Server Monitoring GUI uses return std::optional.
// std::nullopt means "the database couldn't be reached / the query failed",
// so the GUI can show "An error occurred while trying to reach the database."
// ---------------------------------------------------------------------------

#include "PasswordHasher.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <optional>
#include <vector>

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
    int failedAttempts = 0;
    QDateTime lockedUntil;   // invalid (isNull) = never locked

    // True while the lock time is still in the future.
    bool isLocked() const
    {
        return lockedUntil.isValid() && lockedUntil > QDateTime::currentDateTime();
    }
};

// One entry in the locked-accounts list shown to the admin.
struct LockedAccount
{
    QString username;
    QDateTime lockedUntil;
};

class Database
{
public:
    ~Database();

    // Opens (or creates) the file and makes sure the table layout is current.
    // Returns false if anything goes wrong (the reason is logged).
    bool open(const QString& filePath);

    // ---- accounts ----

    // Looks up a player by username. std::nullopt if not found.
    std::optional<PlayerRecord> findPlayer(const QString& username);

    // Inserts a new player. Returns the new player_id, or -1 on failure.
    // The password and pin arrive already hashed (see PasswordHasher).
    int createPlayer(const QString& username, const QString& email,
                     const PasswordHasher::Hashed& password,
                     const PasswordHasher::Hashed& pin);

    // Replaces a player's password hash + salt. Used by password recovery.
    bool updatePassword(int playerId, const PasswordHasher::Hashed& password);

    // ---- login status ----

    // connected = 1, last_logged_in = now
    void markLoggedIn(int playerId);

    // connected = 0, last_logged_out = now
    void markLoggedOut(int playerId);

    // connected = 0 for everyone. Called at startup in case the server
    // crashed while people were logged in.
    void resetAllConnected();

    // ---- failed logins / lockout (issue #54) ----

    // Adds 1 to failed_attempts and returns the new count (-1 on error).
    int recordFailedLogin(int playerId);

    // Sets failed_attempts = 0 and clears locked_until. Used after a
    // successful login and when an expired lock is noticed.
    void resetFailedLogins(int playerId);

    // Sets locked_until. The account can't log in until then.
    void lockAccount(int playerId, const QDateTime& until);

    // Admin unlock (use case 7.2): clears the lock and the failed count.
    // Returns false if the query failed or no such username exists.
    bool unlockAccount(const QString& username);

    // ---- Server Monitoring GUI queries ----

    // Total number of registered accounts (issue #74).
    std::optional<int> registeredPlayerCount();

    // Accounts whose lock is still in the future, soonest unlock first (issue #73).
    std::optional<std::vector<LockedAccount>> lockedAccounts();

private:
    // Checks the schema version and creates/upgrades the players table.
    bool prepareSchema();

    QSqlDatabase m_db;
};
