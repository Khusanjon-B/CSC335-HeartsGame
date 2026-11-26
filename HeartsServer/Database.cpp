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

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// Bump this whenever you change a table layout, and add an upgrade step for
// the old version in prepareSchema().
//   v3: players with lockout columns
//   v4: + matches and match_players (players unchanged)
static constexpr int kSchemaVersion = 4;

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

    q.exec("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'players'");
    const bool tableExists = q.next();

    // 2a. Version 0/1 (the very first layout: plain-text passwords, no email
    //     or pin) can't be upgraded, so it's renamed out of the way and kept.
    if (tableExists && fileVersion < 2) {
        q.exec("DROP TABLE IF EXISTS players_old");   // only keep one backup
        if (!q.exec("ALTER TABLE players RENAME TO players_old")) {
            logMessage("[DB] could not back up old players table: " + q.lastError().text());
            return false;
        }
        logMessage("[DB] old players table (schema v" + QString::number(fileVersion) +
                   ") renamed to players_old; those accounts must register again");
    }

    // 2b. Version 2 -> 3: just add the two lockout columns. Existing
    //     accounts are kept and start with 0 failed attempts, unlocked.
    if (tableExists && fileVersion == 2) {
        if (!q.exec("ALTER TABLE players ADD COLUMN failed_attempts INTEGER NOT NULL DEFAULT 0") ||
            !q.exec("ALTER TABLE players ADD COLUMN locked_until TEXT")) {
            logMessage("[DB] could not upgrade players table to v3: " + q.lastError().text());
            return false;
        }
        logMessage("[DB] players table upgraded from schema v2 to v3 (lockout columns added)");
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
        "  connected        INTEGER NOT NULL DEFAULT 0,"
        "  failed_attempts  INTEGER NOT NULL DEFAULT 0,"
        "  locked_until     TEXT"
        ")");
    if (!ok) {
        logMessage("[DB] could not create players table: " + q.lastError().text());
        return false;
    }

    // 3b. Match tables (v4). A v3 file simply doesn't have them yet, so
    //     "IF NOT EXISTS" creates them and nothing else needs upgrading.
    const bool matchesOk =
        q.exec("CREATE TABLE IF NOT EXISTS matches ("
               "  match_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
               "  played_at  TEXT    NOT NULL,"
               "  rounds     INTEGER NOT NULL"
               ")") &&
        q.exec("CREATE TABLE IF NOT EXISTS match_players ("
               "  match_id   INTEGER NOT NULL REFERENCES matches(match_id),"
               "  player_id  INTEGER NOT NULL REFERENCES players(player_id),"
               "  seat       INTEGER NOT NULL,"
               "  score      INTEGER NOT NULL,"
               "  place      INTEGER NOT NULL,"
               "  PRIMARY KEY (match_id, player_id)"
               ")");
    if (!matchesOk) {
        logMessage("[DB] could not create match tables: " + q.lastError().text());
        return false;
    }
    if (tableExists && fileVersion == 3)
        logMessage("[DB] upgraded from schema v3 to v4 (match tables added)");

    // 4. Remember that this file now has the current layout.
    q.exec("PRAGMA user_version = " + QString::number(kSchemaVersion));
    return true;
}

// ===========================================================================
//  Accounts
// ===========================================================================

std::optional<PlayerRecord> Database::findPlayer(const QString& username)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT player_id, username, email, password_hash, password_salt, "
              "       pin_hash, pin_salt, connected, failed_attempts, locked_until "
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
    p.id             = q.value(0).toInt();
    p.username       = q.value(1).toString();
    p.email          = q.value(2).toString();
    p.passwordHash   = q.value(3).toString();
    p.passwordSalt   = q.value(4).toString();
    p.pinHash        = q.value(5).toString();
    p.pinSalt        = q.value(6).toString();
    p.connected      = q.value(7).toBool();
    p.failedAttempts = q.value(8).toInt();
    if (!q.value(9).isNull())
        p.lockedUntil = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
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

bool Database::emailInUse(const QString& email)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM players WHERE email = ? LIMIT 1");
    q.addBindValue(email);
    if (!q.exec()) {
        logMessage("[DB] emailInUse failed: " + q.lastError().text());
        return false;
    }
    return q.next();   // a row came back = the email is taken
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

// ===========================================================================
//  Login status
// ===========================================================================

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

// ===========================================================================
//  Failed logins / lockout
// ===========================================================================

int Database::recordFailedLogin(int playerId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET failed_attempts = failed_attempts + 1 WHERE player_id = ?");
    q.addBindValue(playerId);
    if (!q.exec()) {
        logMessage("[DB] recordFailedLogin failed: " + q.lastError().text());
        return -1;
    }

    // Read the new value back.
    q.prepare("SELECT failed_attempts FROM players WHERE player_id = ?");
    q.addBindValue(playerId);
    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toInt();
}

void Database::resetFailedLogins(int playerId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET failed_attempts = 0, locked_until = NULL WHERE player_id = ?");
    q.addBindValue(playerId);
    if (!q.exec())
        logMessage("[DB] resetFailedLogins failed: " + q.lastError().text());
}

void Database::lockAccount(int playerId, const QDateTime& until)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET locked_until = ? WHERE player_id = ?");
    q.addBindValue(until.toString(Qt::ISODate));
    q.addBindValue(playerId);
    if (!q.exec())
        logMessage("[DB] lockAccount failed: " + q.lastError().text());
}

bool Database::unlockAccount(const QString& username)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE players SET failed_attempts = 0, locked_until = NULL WHERE username = ?");
    q.addBindValue(username);
    if (!q.exec()) {
        logMessage("[DB] unlockAccount failed: " + q.lastError().text());
        return false;
    }
    return q.numRowsAffected() > 0;   // 0 rows = no such username
}

// ===========================================================================
//  Server Monitoring GUI queries
// ===========================================================================

std::optional<int> Database::registeredPlayerCount()
{
    QSqlQuery q(m_db);
    if (!q.exec("SELECT COUNT(*) FROM players") || !q.next()) {
        logMessage("[DB] registeredPlayerCount failed: " + q.lastError().text());
        return std::nullopt;
    }
    return q.value(0).toInt();
}

std::optional<std::vector<LockedAccount>> Database::lockedAccounts()
{
    // ISO timestamps sort correctly as text, so "locked_until > now" works
    // as a plain string comparison in SQL.
    QSqlQuery q(m_db);
    q.prepare("SELECT username, locked_until FROM players "
              "WHERE locked_until IS NOT NULL AND locked_until > ? "
              "ORDER BY locked_until");
    q.addBindValue(nowString());
    if (!q.exec()) {
        logMessage("[DB] lockedAccounts failed: " + q.lastError().text());
        return std::nullopt;
    }

    std::vector<LockedAccount> result;
    while (q.next())
        result.push_back({q.value(0).toString(),
                          QDateTime::fromString(q.value(1).toString(), Qt::ISODate)});
    return result;
}

// ===========================================================================
//  Matches (issue #68, #87)
// ===========================================================================

std::optional<int> Database::saveMatch(int rounds, const std::vector<MatchPlayer>& players)
{
    // A transaction: either the match and all 4 player rows are saved, or
    // none of them are (rollback), so a half-saved match can't exist.
    if (!m_db.transaction()) {
        logMessage("[DB] saveMatch: could not start transaction: " + m_db.lastError().text());
        return std::nullopt;
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO matches (played_at, rounds) VALUES (?, ?)");
    q.addBindValue(nowString());
    q.addBindValue(rounds);
    if (!q.exec()) {
        logMessage("[DB] saveMatch failed: " + q.lastError().text());
        m_db.rollback();
        return std::nullopt;
    }
    const int matchId = q.lastInsertId().toInt();

    for (const MatchPlayer& p : players) {
        QSqlQuery row(m_db);
        row.prepare("INSERT INTO match_players (match_id, player_id, seat, score, place) "
                    "VALUES (?, ?, ?, ?, ?)");
        row.addBindValue(matchId);
        row.addBindValue(p.playerId);
        row.addBindValue(p.seat);
        row.addBindValue(p.score);
        row.addBindValue(p.place);
        if (!row.exec()) {
            logMessage("[DB] saveMatch failed: " + row.lastError().text());
            m_db.rollback();
            return std::nullopt;
        }
    }

    if (!m_db.commit()) {
        logMessage("[DB] saveMatch: commit failed: " + m_db.lastError().text());
        m_db.rollback();
        return std::nullopt;
    }
    return matchId;
}

std::optional<std::vector<MatchRecord>> Database::getMatchHistory(int playerId)
{
    // Every player row of every match this player was in, newest match
    // first, seats in order. One query, grouped into matches below.
    QSqlQuery q(m_db);
    q.prepare("SELECT m.match_id, m.played_at, m.rounds, "
              "       mp.player_id, p.username, mp.seat, mp.score, mp.place "
              "FROM matches m "
              "JOIN match_players mp ON mp.match_id = m.match_id "
              "JOIN players p        ON p.player_id = mp.player_id "
              "WHERE m.match_id IN (SELECT match_id FROM match_players WHERE player_id = ?) "
              "ORDER BY m.match_id DESC, mp.seat");
    q.addBindValue(playerId);
    if (!q.exec()) {
        logMessage("[DB] getMatchHistory failed: " + q.lastError().text());
        return std::nullopt;
    }

    std::vector<MatchRecord> matches;
    while (q.next()) {
        const int matchId = q.value(0).toInt();
        if (matches.empty() || matches.back().matchId != matchId) {
            MatchRecord m;
            m.matchId  = matchId;
            m.playedAt = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
            m.rounds   = q.value(2).toInt();
            matches.push_back(m);
        }
        MatchPlayer p;
        p.playerId = q.value(3).toInt();
        p.username = q.value(4).toString();
        p.seat     = q.value(5).toInt();
        p.score    = q.value(6).toInt();
        p.place    = q.value(7).toInt();
        matches.back().players.push_back(p);
    }
    return matches;
}
