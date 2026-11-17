// ---------------------------------------------------------------------------
// Database.cpp - all of the SQL lives in this file.
//
// How Qt runs a query:
//   QSqlQuery q(m_db);                          // a query on our database
//   q.prepare("SELECT ... WHERE username = ?");  // SQL with ? placeholders
//   q.addBindValue(username);                   // fills the 1st ?, then 2nd...
//   q.exec();                                   // runs it, false on error
//   while (q.next()) { q.value(0) ... }         // step through result rows
//
// User input always goes in through ? placeholders, never pasted into the
// SQL text. That way SQLite treats it as data and can't run it as SQL
// (this is what prevents "SQL injection").
// ---------------------------------------------------------------------------

#include "Database.h"
#include "Log.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// Bump this whenever you change the players table layout below.
static constexpr int kSchemaVersion = 2;

// Current time as text, e.g. "2026-11-16T23:41:07".
static QString nowString()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

Database::~Database()
{
    if (m_db.isOpen())
        m_db.close();
}

bool Database::open(const QString& filePath)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");   // Qt's built-in SQLite driver
    m_db.setDatabaseName(filePath);                // created if missing

    if (!m_db.open()) {
        logMessage("[DB] could not open " + filePath + ": " + m_db.lastError().text());
        return false;
    }

    logMessage("[DB] opened " + filePath);
    return prepareSchema();
}

bool Database::prepareSchema()
{
    QSqlQuery q(m_db);

    // 1. Which layout does this file have? A brand-new file says 0.
    q.exec("PRAGMA user_version");
    const int fileVersion = q.next() ? q.value(0).toInt() : 0;

    if (fileVersion > kSchemaVersion) {
        logMessage("[DB] this database is from a newer version of the server; refusing to touch it");
        return false;
    }

    // 2. Old layout? Rename the old table so its data is kept but out of
    //    the way. (Version 0 with an existing players table = the first
    //    layout, before we started numbering versions.)
    if (fileVersion < kSchemaVersion) {
        q.exec("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'players'");
        if (q.next()) {
            q.exec("DROP TABLE IF EXISTS players_old");   // only keep one backup
            if (!q.exec("ALTER TABLE players RENAME TO players_old")) {
                logMessage("[DB] could not back up old players table: " + q.lastError().text());
                return false;
            }
            logMessage("[DB] old players table (schema v" + QString::number(fileVersion) +
                       ") renamed to players_old; those accounts must register again");
        }
    }

    // 3. Create the current table (does nothing if it already exists).
    const bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS players ("
        "  player_id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username         TEXT    NOT NULL UNIQUE,"
        "  email            TEXT    NOT NULL,"
        "  password_hash    TEXT    NOT NULL,"
        "  password_salt    TEXT    NOT NULL,"
        "  pin_hash         TEXT    NOT NULL,"
        "  pin_salt         TEXT    NOT NULL,"
        "  last_logged_in   TEXT,"
        "  last_logged_out  TEXT,"
        "  connected        INTEGER NOT NULL DEFAULT 0"
        ")");
    if (!ok) {
        logMessage("[DB] could not create players table: " + q.lastError().text());
        return false;
    }

    // 4. Remember that this file now has the current layout.
    q.exec("PRAGMA user_version = " + QString::number(kSchemaVersion));
    return true;
}

std::optional<PlayerRecord> Database::findPlayer(const QString& username)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT player_id, username, email, password_hash, password_salt, "
              "       pin_hash, pin_salt, connected "
              "FROM players WHERE username = ?");
    q.addBindValue(username);

    if (!q.exec()) {
        logMessage("[DB] findPlayer failed: " + q.lastError().text());
        return std::nullopt;
    }
    if (!q.next())               // no rows = no such player
        return std::nullopt;

    // Column numbers match the order in the SELECT above.
    PlayerRecord p;
    p.id           = q.value(0).toInt();
    p.username     = q.value(1).toString();
    p.email        = q.value(2).toString();
    p.passwordHash = q.value(3).toString();
    p.passwordSalt = q.value(4).toString();
    p.pinHash      = q.value(5).toString();
    p.pinSalt      = q.value(6).toString();
    p.connected    = q.value(7).toBool();
    return p;
}

int Database::createPlayer(const QString& username, const QString& email,
                           const PasswordHasher::Hashed& password,
                           const PasswordHasher::Hashed& pin)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO players "
              "(username, email, password_hash, password_salt, pin_hash, pin_salt) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(username);
    q.addBindValue(email);
    q.addBindValue(password.hash);
    q.addBindValue(password.salt);
    q.addBindValue(pin.hash);
    q.addBindValue(pin.salt);

    if (!q.exec()) {
        logMessage("[DB] createPlayer failed: " + q.lastError().text());
        return -1;
    }
    return q.lastInsertId().toInt();   // the player_id SQLite just generated
}

bool Database::updatePassword(int playerId, const PasswordHasher::Hashed& password)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET password_hash = ?, password_salt = ? WHERE player_id = ?");
    q.addBindValue(password.hash);
    q.addBindValue(password.salt);
    q.addBindValue(playerId);

    if (!q.exec()) {
        logMessage("[DB] updatePassword failed: " + q.lastError().text());
        return false;
    }
    return true;
}

void Database::markLoggedIn(int playerId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET connected = 1, last_logged_in = ? WHERE player_id = ?");
    q.addBindValue(nowString());
    q.addBindValue(playerId);
    if (!q.exec())
        logMessage("[DB] markLoggedIn failed: " + q.lastError().text());
}

void Database::markLoggedOut(int playerId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET connected = 0, last_logged_out = ? WHERE player_id = ?");
    q.addBindValue(nowString());
    q.addBindValue(playerId);
    if (!q.exec())
        logMessage("[DB] markLoggedOut failed: " + q.lastError().text());
}

void Database::resetAllConnected()
{
    QSqlQuery q(m_db);
    if (!q.exec("UPDATE players SET connected = 0"))
        logMessage("[DB] resetAllConnected failed: " + q.lastError().text());
}
