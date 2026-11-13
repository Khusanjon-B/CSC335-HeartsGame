// ---------------------------------------------------------------------------
// Database.cpp - all of the SQL lives in this file.
//
// How Qt runs a query:
//   QSqlQuery q(m_db);                         // a query on our database
//   q.prepare("SELECT ... WHERE username = ?"); // SQL with ? placeholders
//   q.addBindValue(username);                  // fills the 1st ?, then 2nd...
//   q.exec();                                  // runs it, false on error
//   while (q.next()) { q.value(0) ... }        // step through result rows
//
// Always use ? placeholders and never paste user input straight into the SQL
// string. Placeholders block SQL injection, so a username like
// "x'; DROP TABLE players;--" is stored as plain text and never run as SQL.
// ---------------------------------------------------------------------------

#include "Database.h"
#include "Log.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// Current time as text, e.g. "2026-11-09T23:41:07". Stored as TEXT in SQLite.
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
    // "QSQLITE" picks Qt's built-in SQLite driver.
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(filePath);   // SQLite creates the file if it's missing

    if (!m_db.open()) {
        logMessage("[DB] could not open " + filePath + ": " + m_db.lastError().text());
        return false;
    }

    logMessage("[DB] opened " + filePath);
    return createTables();
}

bool Database::createTables()
{
    QSqlQuery q(m_db);

    // IF NOT EXISTS means this is a no-op after the first run, so existing
    // players are kept. If you change the columns here, delete hearts.db
    // so it gets recreated (fine while developing).
    const bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS players ("
        "  player_id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username         TEXT    NOT NULL UNIQUE,"
        "  password         TEXT    NOT NULL,"
        "  last_logged_in   TEXT,"
        "  last_logged_out  TEXT,"
        "  connected        INTEGER NOT NULL DEFAULT 0"
        ")");

    if (!ok)
        logMessage("[DB] could not create players table: " + q.lastError().text());
    return ok;
}

std::optional<PlayerRecord> Database::findPlayer(const QString& username)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT player_id, username, password, connected "
              "FROM players WHERE username = ?");
    q.addBindValue(username);

    if (!q.exec()) {
        logMessage("[DB] findPlayer failed: " + q.lastError().text());
        return std::nullopt;
    }
    if (!q.next())               // no rows means no player with that name
        return std::nullopt;

    // Column numbers match the order in the SELECT above.
    PlayerRecord p;
    p.id        = q.value(0).toInt();
    p.username  = q.value(1).toString();
    p.password  = q.value(2).toString();
    p.connected = q.value(3).toBool();
    return p;
}

int Database::createPlayer(const QString& username, const QString& password)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO players (username, password) VALUES (?, ?)");
    q.addBindValue(username);
    q.addBindValue(password);   // TODO: store a salted hash instead of plain text

    if (!q.exec()) {
        // Most likely the UNIQUE constraint: the username already exists.
        logMessage("[DB] createPlayer failed: " + q.lastError().text());
        return -1;
    }

    // SQLite gives us the player_id it just generated.
    return q.lastInsertId().toInt();
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
