// ---------------------------------------------------------------------------
// Server.cpp - see Server.h for the message format and how to add commands.
//
// HOW THIS RUNS ON ONE THREAD (no locks needed):
// Qt sockets are asynchronous. Instead of waiting in a loop for data, we
// connect signals to our functions, and Qt's event loop (app.exec() in
// main) calls them when something happens:
//
//   QTcpServer::newConnection  -> onNewConnection()  a client connected
//   QTcpSocket::readyRead      -> onReadyRead()      bytes arrived
//   QTcpSocket::disconnected   -> onDisconnected()   a client left
//
// They all run on the main thread, one at a time, so two of them can never
// touch m_connections or the database at the same moment. The monitor
// window (MonitorWindow) runs on that same thread too.
//
// Sections in this file:
//   Connection bookkeeping   - connect / read / disconnect, logout helpers
//   Monitoring GUI queries   - what the admin window reads (view*, unlock)
//   Receiving                - JSON parsing, then one handler per command
//   Sending                  - sendMessage + the reply helpers
//
// Whenever something the admin can see changes, we `emit stateChanged()`
// so the monitor window redraws.
// ---------------------------------------------------------------------------

#include "Server.h"
#include "ErrorCodes.h"
#include "Log.h"
#include "PasswordHasher.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <algorithm>

// ---- input rules (change them here) ----
// (The lockout numbers are in server.ini instead; see Config.h.)
static constexpr int kMinPinLength      = 4;
static constexpr int kMaxPinLength      = 8;
static constexpr int kMinPasswordLength = 9;   // client rule: "longer than 8 characters"

// Username: letters, digits, _ and - only (team notes 9/29), so nothing ever
// needs escaping. Matches the client's validUsername().
static bool isValidUsername(const QString& username)
{
    static const QRegularExpression pattern(R"(^[A-Za-z0-9_-]+$)");
    return pattern.match(username).hasMatch();
}

// Password: at least kMinPasswordLength characters, with at least one letter,
// one digit and one special character. Matches the client's validPassword(),
// so the server enforces the same rule even if a client skips the check.
static bool isValidPassword(const QString& password)
{
    static const QRegularExpression letter("[A-Za-z]");
    static const QRegularExpression digit("[0-9]");
    static const QRegularExpression special("[^A-Za-z0-9]");
    return password.size() >= kMinPasswordLength &&
           letter.match(password).hasMatch() &&
           digit.match(password).hasMatch() &&
           special.match(password).hasMatch();
}

// Shown to the player when a password fails isValidPassword().
static QString passwordRuleText()
{
    return QString("password must be at least %1 characters and include a letter, "
                   "a digit and a special character").arg(kMinPasswordLength);
}

// Loose email check: something@something.something, no spaces.
static bool looksLikeEmail(const QString& email)
{
    static const QRegularExpression pattern(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
    return pattern.match(email).hasMatch();
}

// Pin = kMinPinLength..kMaxPinLength digits, sent as a JSON string ("0042").
// (A string, not a number, so leading zeros aren't lost.)
static bool isValidPin(const QString& pin)
{
    if (pin.size() < kMinPinLength || pin.size() > kMaxPinLength)
        return false;
    return std::all_of(pin.begin(), pin.end(), [](QChar c) { return c.isDigit(); });
}

// Turns a message into one log line, e.g.
//   127.0.0.1:53211 (alice)  >  #7 login {"password":"***","username":"alice"}
// ">" = from the client, "<" = to the client. Secrets are hidden.
static QString describe(const Connection& conn, const Message& msg)
{
    QJsonObject fields = redactSecrets(msg.body);
    fields.remove("command");     // already shown separately
    fields.remove("requestId");

    const QString arrow = (msg.direction == Message::Direction::FromClient) ? "  >  " : "  <  ";
    return conn.label() + arrow + "#" + QString::number(msg.requestId) + " " + msg.command +
           " " + QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact));
}

Server::Server(Database& db, const ServerConfig& config, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_config(config)
{
    registerHandlers();

    // When a client finishes connecting, Qt emits newConnection -> we run
    // onNewConnection().
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

void Server::registerHandlers()
{
    // Command name -> function. C++ can't switch on strings, so this lookup
    // table does that job. Add new commands here.
    m_handlers["register"]         = &Server::handleRegister;
    m_handlers["login"]            = &Server::handleLogin;
    m_handlers["logout"]           = &Server::handleLogout;
    m_handlers["recover_password"] = &Server::handleRecoverPassword;
    m_handlers["change_password"]  = &Server::handleChangePassword;
    m_handlers["ping"]             = &Server::handlePing;
    m_handlers["start_lobby"]      = &Server::handleStartLobby;
    m_handlers["get_lobbies"]      = &Server::handleGetLobbies;
    m_handlers["join_lobby"]       = &Server::handleJoinLobby;
    m_handlers["leave_lobby"]      = &Server::handleLeaveLobby;
    m_handlers["start_game"]       = &Server::handleStartGame;
    m_handlers["pass_cards"]       = &Server::handlePassCards;
    m_handlers["play_card"]        = &Server::handlePlayCard;
    m_handlers["get_match_history"] = &Server::handleGetMatchHistory;
}

bool Server::start(quint16 port)
{
    // AnyIPv4 = 0.0.0.0 = accept on every network card, so other machines on
    // the LAN can connect.
    if (!m_tcpServer.listen(QHostAddress::AnyIPv4, port)) {
        logMessage("listen failed: " + m_tcpServer.errorString());
        return false;
    }

    logMessage("listening on port " + QString::number(port) + " at:");
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
            logMessage("  " + addr.toString());
    }
    return true;
}

// ===========================================================================
//  Connection bookkeeping
// ===========================================================================

void Server::onNewConnection()
{
    // Several clients can be waiting, so take all of them.
    while (m_tcpServer.hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer.nextPendingConnection();

        Connection conn;   // starts anonymous: no playerId, not verified
        conn.socket = socket;
        conn.ip = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
        m_connections.push_back(conn);

        logMessage("connected     " + conn.ip);

        // The lambdas capture `socket` so we know which client the signal
        // came from.
        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket] { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this,
                [this, socket] { onDisconnected(socket); });
    }
    emit stateChanged();   // connected count changed
}

void Server::onReadyRead(QTcpSocket* socket)
{
    // TCP is a stream: one readyRead can hold half a message or several.
    // canReadLine() is only true once a whole '\n'-terminated line is here.
    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Look the Connection up fresh each time instead of keeping a
        // pointer; pointers into a std::vector break when it grows.
        Connection* conn = findConnection(socket);
        if (!conn)
            return;

        processLine(*conn, line);
    }
}

void Server::onDisconnected(QTcpSocket* socket)
{
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
                           [socket](const Connection& c) { return c.socket == socket; });
    if (it == m_connections.end())
        return;

    logMessage("disconnected  " + it->label() +
               "  (" + QString::number(it->history.size()) + " messages in history)");

    if (it->verified)
        logoutConnection(*it);   // record the logout in the DB

    m_connections.erase(it);

    // Free the socket once Qt is done with the current signal. Deleting it
    // right now, while it's still emitting, could crash.
    socket->deleteLater();
    emit stateChanged();   // connected count changed
}

Connection* Server::findConnection(QTcpSocket* socket)
{
    for (Connection& c : m_connections)
        if (c.socket == socket)
            return &c;
    return nullptr;
}

Connection* Server::findConnectionByPlayer(int playerId)
{
    for (Connection& c : m_connections)
        if (c.playerId && *c.playerId == playerId)
            return &c;
    return nullptr;
}

void Server::logoutConnection(Connection& conn)
{
    // Logging out (or disconnecting) also leaves any lobby, so the other
    // players are told and a host's lobby is disbanded (use case 10).
    leaveLobby(conn);

    if (conn.playerId)
        m_db.markLoggedOut(*conn.playerId);

    conn.playerId.reset();   // back to anonymous; the socket stays open
    conn.username.clear();
    conn.verified = false;
    emit stateChanged();     // logged-in list changed
}

void Server::logoutEveryone()
{
    for (Connection& c : m_connections)
        if (c.verified)
            logoutConnection(c);
}

// ===========================================================================
//  Server Monitoring GUI queries (see Server.h)
// ===========================================================================

std::vector<Server::ClientInfo> Server::viewConnectedUsers() const
{
    std::vector<ClientInfo> result;
    for (const Connection& c : m_connections)
        result.push_back({c.ip, c.username, c.verified});
    return result;
}

std::vector<Server::ClientInfo> Server::viewLoggedInUsers() const
{
    std::vector<ClientInfo> result;
    for (const Connection& c : m_connections)
        if (c.verified)
            result.push_back({c.ip, c.username, true});
    return result;
}

std::optional<int> Server::viewRegisteredUsers()
{
    return m_db.registeredPlayerCount();
}

std::optional<std::vector<LockedAccount>> Server::viewLockedAccounts()
{
    return m_db.lockedAccounts();
}

bool Server::unlockAccount(const QString& username)
{
    const bool ok = m_db.unlockAccount(username);
    if (ok) {
        logMessage("[Admin] unlocked account \"" + username + "\"");
        emit stateChanged();
    }
    return ok;
}

// ===========================================================================
//  Receiving
// ===========================================================================

void Server::processLine(Connection& conn, const QByteArray& line)
{
    // 1. Parse the text as JSON. QJsonObject then works like a map:
    //    body["username"].toString(), body.contains("pin"), ...
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        // We can't read a requestId from broken JSON, so the reply uses 0.
        // The raw text is logged (cut to 200 chars) to help debug clients.
        logMessage(conn.label() + "  >  [unreadable] " + QString::fromUtf8(line.left(200)));
        sendError(conn, 0, ErrorCode::InvalidJson,
                  parseError.error != QJsonParseError::NoError
                      ? "invalid JSON: " + parseError.errorString()
                      : "message must be a JSON object");
        return;
    }

    // 2. Wrap it in a Message, save it in this client's history, log it.
    const Message request = makeMessage(Message::Direction::FromClient, doc.object());
    conn.record(request);
    logMessage(describe(conn, request));

    // 3. Every client message needs a positive whole-number requestId.
    if (request.requestId <= 0) {
        sendError(conn, 0, ErrorCode::MissingRequestId,
                  "every message needs a \"requestId\" that is a positive whole number");
        return;
    }

    // 4. Find the handler for this command and call it.
    const auto it = m_handlers.constFind(request.command);
    if (it == m_handlers.constEnd()) {
        sendError(conn, request.requestId, ErrorCode::UnknownCommand,
                  "unknown command: \"" + request.command + "\"");
        return;
    }

    // (this->*handler)(...) calls a member function through a pointer.
    // Same as e.g. handleLogin(conn, request).
    const Handler handler = it.value();
    (this->*handler)(conn, request);
}
// handleRegister requests
void Server::handleRegister(Connection& conn, const Message& request)
{
    const QJsonObject& b = request.body;
    const QString username = b.value("username").toString().trimmed();
    const QString email    = b.value("email").toString().trimmed().toLower();
    const QString password = b.value("password").toString();
    const QString pin      = b.value("pin").toString();

    // Checked in this order; the first problem found is the one reported.
    if (username.isEmpty() || email.isEmpty() || password.isEmpty() || pin.isEmpty())
        return replyFail(conn, request, ErrorCode::MissingFields,
                         "username, email, password and pin are all required");
    if (!isValidUsername(username))
        return replyFail(conn, request, ErrorCode::InvalidUsername,
                         "username can only use letters, digits, _ and -");
    if (!isValidPassword(password))
        return replyFail(conn, request, ErrorCode::InvalidPassword, passwordRuleText());
    if (!looksLikeEmail(email))
        return replyFail(conn, request, ErrorCode::InvalidEmail,
                         "that email address doesn't look valid");
    if (!isValidPin(pin))
        return replyFail(conn, request, ErrorCode::InvalidPin,
                         QString("pin must be a string of %1-%2 digits").arg(kMinPinLength).arg(kMaxPinLength));
    if (m_db.findPlayer(username))
        return replyFail(conn, request, ErrorCode::InvalidCredentials, "username already taken");
    // "One email = one account" (team notes 10/1, test case 2.2).
    if (m_db.emailInUse(email))
        return replyFail(conn, request, ErrorCode::InvalidCredentials, "email already in use");

    // Hash both secrets (each gets its own random salt) and save.
    const int newId = m_db.createPlayer(username, email,
                                        PasswordHasher::hash(password),
                                        PasswordHasher::hash(pin));
    if (newId < 0)
        return replyFail(conn, request, ErrorCode::ServerError, "could not create account");

    // Registering doesn't log you in. The client sends "login" next.
    logMessage("[Auth] registered \"" + username + "\" as player " + QString::number(newId));
    replyOk(conn, request, "account created, you can now log in", {{"playerId", newId}});
    emit stateChanged();   // registered count changed
}

void Server::handleLogin(Connection& conn, const Message& request)
{
    const QString username = request.body.value("username").toString().trimmed();
    const QString password = request.body.value("password").toString();

    if (conn.verified)
        return replyFail(conn, request, ErrorCode::AlreadyLoggedIn,
                         "already logged in as " + conn.username);
    if (username.isEmpty() || password.isEmpty())
        return replyFail(conn, request, ErrorCode::MissingFields,
                         "username and password are required");

    // Unknown username: there's no account to count failures against, so
    // just fail. Same code and message as a wrong password, so nobody can
    // use this to find out which usernames exist.
    std::optional<PlayerRecord> player = m_db.findPlayer(username);
    if (!player) {
        logMessage("[Auth] login failed for " + conn.label() + " as \"" + username + "\" (no such user)");
        return replyFail(conn, request, ErrorCode::InvalidCredentials, "wrong username or password");
    }

    // ---- LOCKOUT (issue #54) ----
    // 1. Still locked? Refuse, even if the password is right.
    if (player->isLocked()) {
        logMessage("[Auth] login refused for \"" + username + "\": account locked until " +
                   player->lockedUntil.toString("hh:mm:ss"));
        return replyLocked(conn, request, *player);
    }

    // 2. Lock has expired (5 minutes passed)? Start counting from zero again.
    if (player->lockedUntil.isValid()) {
        m_db.resetFailedLogins(player->id);
        player->failedAttempts = 0;
        player->lockedUntil = QDateTime();
    }

    // 3. Wrong password: count it, and lock the account on the 3rd one.
    //    Hash what they typed with the stored salt and compare to the stored hash.
    if (!PasswordHasher::verify(password, player->passwordHash, player->passwordSalt)) {
        const int attempts = m_db.recordFailedLogin(player->id);
        logMessage("[Auth] login failed for " + conn.label() + " as \"" + username +
                   "\" (failed attempt " + QString::number(attempts) + " of " +
                   QString::number(m_config.maxFailedLogins) + ")");

        if (attempts >= m_config.maxFailedLogins) {
            player->lockedUntil = QDateTime::currentDateTime().addSecs(m_config.lockoutMinutes * 60);
            player->failedAttempts = attempts;
            m_db.lockAccount(player->id, player->lockedUntil);
            logMessage("[Auth] account \"" + username + "\" LOCKED for " +
                       QString::number(m_config.lockoutMinutes) + " minutes");
            emit stateChanged();   // locked-out list changed
            return replyLocked(conn, request, *player);
        }

        QJsonObject extra{{"failed_attempts", attempts}};
        return replyFail(conn, request, ErrorCode::InvalidCredentials,
                         QString("wrong username or password (%1 of %2 attempts used)")
                             .arg(attempts).arg(m_config.maxFailedLogins),
                         extra);
    }

    // One login per account at a time.
    if (findConnectionByPlayer(player->id))
        return replyFail(conn, request, ErrorCode::AccountInUse,
                         "this account is already logged in from another connection");

    // Success: clear any failed attempts, attach the player to this
    // connection, then update the DB.
    if (player->failedAttempts > 0)
        m_db.resetFailedLogins(player->id);
    conn.playerId = player->id;
    conn.username = player->username;
    conn.verified = true;
    m_db.markLoggedIn(player->id);
    emit stateChanged();   // logged-in list changed

    logMessage("[Auth] " + conn.ip + " logged in as \"" + conn.username +
               "\" (player " + QString::number(player->id) + ")");
    replyOk(conn, request, "welcome " + conn.username,
            {{"playerId", player->id}, {"username", conn.username}});
}

// LOGOUT (issue #59)
//   client:  {"command":"logout","requestId":N}
//   server:  {"command":"logout_result","requestId":N,"success":true,"error":"","message":"logged out"}
//
// Logging out ends the player's session but keeps the TCP connection open,
// so the client can show the login screen and log in again (as anyone)
// without reconnecting.
//
// If the connection isn't logged in, the reply is success:false with
// NOT_LOGGED_IN. Either way, after this reply the server considers the
// connection logged out, so the client can safely switch to its logged-out
// screen on any logout_result. (The use case asks that the client never be
// left falsely showing an authenticated session.)
void Server::handleLogout(Connection& conn, const Message& request)
{
    if (!conn.verified)
        return replyFail(conn, request, ErrorCode::NotLoggedIn, "you are not logged in");

    logMessage("[Auth] \"" + conn.username + "\" logged out");
    logoutConnection(conn);   // clears playerId/username/verified + DB: connected=0, last_logged_out=now
    replyOk(conn, request, "logged out");
}

void Server::handleRecoverPassword(Connection& conn, const Message& request)
{
    const QJsonObject& b = request.body;
    const QString username    = b.value("username").toString().trimmed();
    const QString email       = b.value("email").toString().trimmed().toLower();
    const QString pin         = b.value("pin").toString();
    const QString newPassword = b.value("newPassword").toString();

    if (username.isEmpty() || email.isEmpty() || pin.isEmpty() || newPassword.isEmpty())
        return replyFail(conn, request, ErrorCode::MissingFields,
                         "username, email, pin and newPassword are all required");

    // All three (username, email, pin) must match. One shared failure
    // message, so nobody can tell which part was wrong.
    const std::optional<PlayerRecord> player = m_db.findPlayer(username);
    if (!player || player->email != email ||
        !PasswordHasher::verify(pin, player->pinHash, player->pinSalt)) {
        logMessage("[Auth] password recovery failed for \"" + username + "\" from " + conn.label());
        return replyFail(conn, request, ErrorCode::RecoveryFailed,
                         "those recovery details don't match our records");
    }

    // Team notes 10/27: no password recovery while the account is logged in
    // somewhere. (After a reset, the player has to log in again.)
    if (findConnectionByPlayer(player->id))
        return replyFail(conn, request, ErrorCode::AccountInUse,
                         "this account is logged in right now; log out before recovering the password");

    if (!isValidPassword(newPassword))
        return replyFail(conn, request, ErrorCode::InvalidPassword, passwordRuleText());

    // New password gets a new salt too.
    if (!m_db.updatePassword(player->id, PasswordHasher::hash(newPassword)))
        return replyFail(conn, request, ErrorCode::ServerError, "could not update password");

    logMessage("[Auth] password reset for \"" + username + "\"");
    replyOk(conn, request, "password changed, you can now log in");
}

// CHANGE PASSWORD (use case 4, issue #86)
//   client:  {"command":"change_password","requestId":N,
//             "currentPassword":"Hearts#2026","newPassword":"N3wHearts#26"}
//   server:  change_password_result
//
// Unlike recover_password (for players who are logged OUT and forgot their
// password), this is for a logged-IN player who knows their password.
// Checks, in order (test cases 4.1, 4.2, 4.3):
//   1. logged in, both fields present
//   2. current password is right          else INVALID_CREDENTIALS
//   3. new password follows the rules     else INVALID_PASSWORD
//   4. new password differs from current  else SAME_PASSWORD
// A wrong current password does NOT count toward the login lockout; the
// player is already logged in, so there's nothing to protect by locking.
void Server::handleChangePassword(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;

    const QString currentPassword = request.body.value("currentPassword").toString();
    const QString newPassword     = request.body.value("newPassword").toString();
    if (currentPassword.isEmpty() || newPassword.isEmpty())
        return replyFail(conn, request, ErrorCode::MissingFields,
                         "currentPassword and newPassword are both required");

    // Use case 4 exception: "Account systems could not be reached."
    const std::optional<PlayerRecord> player = m_db.findPlayer(conn.username);
    if (!player)
        return replyFail(conn, request, ErrorCode::ServerError, "Account systems could not be reached.");

    if (!PasswordHasher::verify(currentPassword, player->passwordHash, player->passwordSalt)) {
        logMessage("[Auth] change password failed for \"" + conn.username + "\" (wrong current password)");
        return replyFail(conn, request, ErrorCode::InvalidCredentials, "your current password is incorrect");
    }
    if (!isValidPassword(newPassword))
        return replyFail(conn, request, ErrorCode::InvalidPassword, passwordRuleText());
    if (newPassword == currentPassword)
        return replyFail(conn, request, ErrorCode::SamePassword,
                         "the new password must be different from the current one");

    // New salt + hash, same as register and recovery.
    if (!m_db.updatePassword(player->id, PasswordHasher::hash(newPassword)))
        return replyFail(conn, request, ErrorCode::ServerError, "Account systems could not be reached.");

    logMessage("[Auth] \"" + conn.username + "\" changed their password");
    replyOk(conn, request, "password changed");
}

// PING (use case 22, issue #89)
//   client:  {"command":"ping","requestId":N}
//   server:  {"command":"ping_result","requestId":N,"success":true,"error":"",
//             "message":"pong","serverTime":"2026-11-25T14:03:00"}
//
// Lets the client check the server is still there. Works before login.
// Use case 22: the client shows an error after 3 unanswered pings, and may
// resend any request up to 5 times (with the SAME requestId, so the reply
// can be matched up).
void Server::handlePing(Connection& conn, const Message& request)
{
    replyOk(conn, request, "pong",
            {{"serverTime", QDateTime::currentDateTime().toString(Qt::ISODate)}});
}

// ===========================================================================
//  Game sessions / lobbies (issue #64 "Create and maintain game sessions",
//  issue #63 "Host leaves lobby"; use cases 8, 9, 10, 11)
//
//  Messages follow the team's JSON doc (start_lobby, join_lobby,
//  get_lobbies, player_joined, player_left, host_left), with lobbyId as a
//  number. Everyone's "playerNum" is their index in "players" + 1, so the
//  host is always playerNum 1 and players[0].
// ===========================================================================

bool Server::requireLogin(Connection& conn, const Message& request)
{
    if (conn.verified)
        return true;
    replyFail(conn, request, ErrorCode::NotLoggedIn, "you need to log in first");
    return false;
}

GameManager* Server::findGame(int lobbyId)
{
    for (auto& game : m_games)
        if (game->lobbyId() == lobbyId)
            return game.get();
    return nullptr;
}

void Server::sendToLobby(const GameManager& game, const QString& command, QJsonObject fields,
                         int skipPlayerId)
{
    // Server-initiated, so a negative id; the same id for every copy.
    const qint64 id = m_nextServerRequestId--;
    fields["lobbyId"] = game.lobbyId();
    for (const LobbyPlayer& p : game.lobby().players()) {
        if (p.playerId == skipPlayerId)
            continue;
        if (Connection* c = findConnectionByPlayer(p.playerId))
            sendMessage(*c, command, id, fields);
    }
}

// start_lobby  {}  ->  start_lobby_result {lobbyId, playerNum, host, players}
// Use case 8 (Start a New Lobby) + use case 11 steps 1-2 (create a session
// with a unique id). The requester becomes the host.
void Server::handleStartLobby(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;
    if (conn.lobbyId)
        return replyFail(conn, request, ErrorCode::AlreadyInLobby,
                         "you're already in lobby " + QString::number(*conn.lobbyId));

    // Use case 11 extension: concurrent-game limit (server.ini, default 1).
    if (static_cast<int>(m_games.size()) >= m_config.maxConcurrentGames)
        return replyFail(conn, request, ErrorCode::CouldNotCreateLobby,
                         "no additional games can be created at this time");

    const int lobbyId = m_nextLobbyId++;
    m_games.push_back(std::make_unique<GameManager>(lobbyId, LobbyPlayer{*conn.playerId, conn.username},
                                                    m_config.gameOverScore));
    conn.lobbyId = lobbyId;
    const GameManager& game = *m_games.back();

    logMessage("[Game] \"" + conn.username + "\" created lobby " + QString::number(lobbyId));
    replyOk(conn, request, "lobby created, waiting for players",
            {{"lobbyId", lobbyId}, {"playerNum", 1},
             {"host", conn.username}, {"players", game.lobby().playerNames()}});
    emit stateChanged();
}

// get_lobbies  {}  ->  get_lobbies_result {lobbies: [{lobbyId, host, playerCount}, ...]}
// Team notes 9/29: every lobby is listed publicly; anyone can join one with
// fewer than 4 players. Games that already started aren't listed.
void Server::handleGetLobbies(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;

    QJsonArray lobbies;
    for (const auto& game : m_games)
        if (game->state() == GameManager::State::WaitingForPlayers)
            lobbies.append(game->summary());

    replyOk(conn, request,
            lobbies.isEmpty() ? "no open lobbies" : QString("%1 open lobby(s)").arg(lobbies.size()),
            {{"lobbies", lobbies}});
}

// join_lobby {lobbyId}  ->  join_lobby_result {lobbyId, playerNum, host, players}
// and everyone already in the lobby gets player_joined {lobbyId, username, players}.
// Use case 9 + use case 11 step 3 (assign joining players, up to four).
void Server::handleJoinLobby(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;

    const qint64 lobbyId = request.body.value("lobbyId").toInteger(-1);
    if (lobbyId < 0)
        return replyFail(conn, request, ErrorCode::MissingFields, "lobbyId (a number) is required");
    if (conn.lobbyId)
        return replyFail(conn, request, ErrorCode::AlreadyInLobby,
                         "you're already in lobby " + QString::number(*conn.lobbyId));

    GameManager* game = findGame(static_cast<int>(lobbyId));
    if (!game)
        return replyFail(conn, request, ErrorCode::LobbyNotFound, "that lobby doesn't exist");

    switch (game->addPlayerToLobby({*conn.playerId, conn.username})) {
    case GameManager::JoinResult::AlreadyStarted:
        return replyFail(conn, request, ErrorCode::GameAlreadyStarted, "that game has already started");
    case GameManager::JoinResult::Full:
        return replyFail(conn, request, ErrorCode::LobbyFull, "that lobby is full");
    case GameManager::JoinResult::AlreadyInLobby:
        return replyFail(conn, request, ErrorCode::AlreadyInLobby, "you're already in that lobby");
    case GameManager::JoinResult::Ok:
        break;
    }

    conn.lobbyId = game->lobbyId();
    const Lobby& lobby = game->lobby();
    logMessage("[Game] \"" + conn.username + "\" joined lobby " + QString::number(game->lobbyId()) +
               QString(" (%1/%2)").arg(lobby.playerCount()).arg(Lobby::kMaxPlayers));

    replyOk(conn, request, "joined lobby",
            {{"lobbyId", game->lobbyId()}, {"playerNum", lobby.playerNum(*conn.playerId)},
             {"host", lobby.host().username}, {"players", lobby.playerNames()}});
    // Tell everyone else (use case 11 step 4: keep each player's state current).
    sendToLobby(*game, "player_joined",
                {{"username", conn.username}, {"players", lobby.playerNames()}},
                *conn.playerId);
    emit stateChanged();
}

// leave_lobby {}  ->  leave_lobby_result
// Uses leaveLobby(), the same path as logout and disconnect.
void Server::handleLeaveLobby(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;
    if (!conn.lobbyId)
        return replyFail(conn, request, ErrorCode::NotInLobby, "you're not in a lobby");

    leaveLobby(conn);
    replyOk(conn, request, "left the lobby");
}

void Server::leaveLobby(Connection& conn)
{
    if (!conn.lobbyId || !conn.playerId)
        return;
    const int lobbyId = *conn.lobbyId;
    conn.lobbyId.reset();

    GameManager* game = findGame(lobbyId);
    if (!game)
        return;

    const bool wasHost = game->removePlayer(*conn.playerId);

    if (wasHost) {
        // Use case 10 / issue #63: the host left, so the session is
        // disbanded. Everyone still in it gets host_left and goes back to
        // the home screen (their lobbyId is cleared).
        logMessage("[Game] host \"" + conn.username + "\" left lobby " + QString::number(lobbyId) +
                   "; lobby disbanded");
        sendToLobby(*game, "host_left",
                    {{"username", conn.username},
                     {"message", "The host left, so the game was disbanded."}});
        for (const LobbyPlayer& p : game->lobby().players())
            if (Connection* c = findConnectionByPlayer(p.playerId))
                c->lobbyId.reset();

        m_games.erase(std::remove_if(m_games.begin(), m_games.end(),
                                     [lobbyId](const auto& g) { return g->lobbyId() == lobbyId; }),
                      m_games.end());   // deletes the GameManager
    } else {
        // Use case 11 extension: a player left before the session filled
        // up. Keep the lobby open and tell the others (who's left, new seats).
        // (If the game had already started, the gameplay issues will decide
        //  whether to wait for them to come back; see test case 19.2.)
        logMessage("[Game] \"" + conn.username + "\" left lobby " + QString::number(lobbyId));
        sendToLobby(*game, "player_left",
                    {{"username", conn.username}, {"players", game->lobby().playerNames()}});
    }
    emit stateChanged();
}

// start_game {}  ->  start_game_result, then everyone gets
// game_started {lobbyId, players}. Host only, lobby must be full (4).
// Team notes 9/29: "Host can start the game". Use case 11 step 5.
void Server::handleStartGame(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;
    GameManager* game = conn.lobbyId ? findGame(*conn.lobbyId) : nullptr;
    if (!game)
        return replyFail(conn, request, ErrorCode::NotInLobby, "you're not in a lobby");

    switch (game->startGame(*conn.playerId)) {
    case GameManager::StartResult::NotHost:
        return replyFail(conn, request, ErrorCode::NotHost, "only the host can start the game");
    case GameManager::StartResult::NotFull:
        return replyFail(conn, request, ErrorCode::LobbyNotFull,
                         QString("not enough players to start the game (%1/%2)")
                             .arg(game->lobby().playerCount()).arg(Lobby::kMaxPlayers));
    case GameManager::StartResult::AlreadyStarted:
        return replyFail(conn, request, ErrorCode::GameAlreadyStarted, "the game has already started");
    case GameManager::StartResult::Ok:
        break;
    }

    logMessage("[Game] lobby " + QString::number(game->lobbyId()) + " started by \"" +
               conn.username + "\"");
    replyOk(conn, request, "game started");

    // Use case 16 (#71): seats = the players order, every score 0.
    QJsonArray scores;
    for (int score : game->table()->scores())
        scores.append(score);
    sendToLobby(*game, "game_started", {{"players", game->lobby().playerNames()}, {"scores", scores}});

    // Use case 17 (#70): round 1 was dealt by startGame(); tell each player.
    announceRound(*game);
}

// ===========================================================================
//  In-game: table setup and passing (issue #71 / use case 16,
//  issue #70 / use case 17)
//
//  After start_game:
//    everyone   <- game_started  {players, scores:[0,0,0,0]}         (#71)
//    each seat  <- round_started {round, passDirection, passTo, hand} (#70, private)
//    each seat  -> pass_cards    {cards:["QS","AH","2D"]}
//    everyone   <- player_passed {username}           (so UIs can show who's done)
//    ...once all 4 have passed:
//    each seat  <- cards_received {from, cards, hand}                 (private)
//    everyone   <- player_turn   {username, playerNum, trick:1}  (2C holder leads)
//  In a hold round (every 4th), round_started has passDirection "hold" and
//  player_turn follows straight away.
// ===========================================================================

void Server::sendToSeat(const GameManager& game, int seatIndex, const QString& command,
                        QJsonObject fields, qint64 requestId)
{
    const Table* table = game.table();
    if (!table)
        return;
    fields["lobbyId"] = game.lobbyId();
    if (Connection* c = findConnectionByPlayer(table->seat(seatIndex).playerId))
        sendMessage(*c, command, requestId, fields);
}

static QJsonArray cardsToJson(const std::vector<Card>& cards)
{
    QJsonArray array;
    for (const Card& c : cards)
        array.append(c.code());
    return array;
}

void Server::announceRound(GameManager& game)
{
    Table& table = *game.table();
    const bool hold = table.passDirection() == Table::PassDirection::Hold;

    // One id shared by all four private messages, like a broadcast.
    const qint64 id = m_nextServerRequestId--;
    for (int seat = 0; seat < Table::kSeats; ++seat) {
        QJsonObject fields{
            {"round",         table.roundNumber()},
            {"passDirection", Table::directionName(table.passDirection())},
            {"hand",          cardsToJson(table.hand(seat))},
        };
        // Who your 3 cards go to (empty on a hold round).
        fields["passTo"] = hold ? QString() : table.seat(table.passTarget(seat)).username;
        sendToSeat(game, seat, "round_started", fields, id);
    }
    logMessage(QString("[Game] lobby %1: round %2 dealt, passing %3")
                   .arg(game.lobbyId()).arg(table.roundNumber())
                   .arg(Table::directionName(table.passDirection())));

    if (hold)
        announceTurn(game, table.leadSeat());
    emit stateChanged();
}

void Server::announceTurn(GameManager& game, int seatIndex)
{
    const Table& table = *game.table();
    const GameState& state = table.gameState();

    // Everyone gets the same player_turn (one shared id). The copy for the
    // player whose turn it is also lists the cards they may play, so the
    // client can grey out the rest (use case 18 extension a: "prompt the
    // player to choose a valid card").
    const qint64 id = m_nextServerRequestId--;
    for (int seat = 0; seat < Table::kSeats; ++seat) {
        QJsonObject fields{
            {"username",  table.seat(seatIndex).username},
            {"playerNum", seatIndex + 1},
            {"trick",     state.trickNumber()},
            {"leading",   state.isLeading()},   // true = this card starts the trick
        };
        if (seat == seatIndex)
            fields["legalCards"] = cardsToJson(state.legalCards(seat));
        sendToSeat(game, seat, "player_turn", fields, id);
    }
}

bool Server::findSeat(Connection& conn, const Message& request, GameManager*& game, int& seat)
{
    if (!requireLogin(conn, request))
        return false;
    game = conn.lobbyId ? findGame(*conn.lobbyId) : nullptr;
    const Table* table = game ? game->table() : nullptr;
    seat = table ? table->seatOf(*conn.playerId) : -1;
    if (seat < 0) {
        replyFail(conn, request, ErrorCode::NotInGame, "you're not in a game");
        return false;
    }
    return true;
}

// pass_cards {cards:[3 card codes]}  ->  pass_cards_result
void Server::handlePassCards(Connection& conn, const Message& request)
{
    GameManager* game = nullptr;
    int seat = -1;
    if (!findSeat(conn, request, game, seat))
        return;
    Table* table = game->table();

    // Read the card codes. Anything that isn't a real card is rejected.
    std::vector<Card> cards;
    for (const QJsonValue& v : request.body.value("cards").toArray()) {
        const std::optional<Card> card = Card::fromCode(v.toString());
        if (!card)
            return replyFail(conn, request, ErrorCode::InvalidCards,
                             "\"" + v.toString() + "\" isn't a card (use codes like \"QS\", \"10H\")");
        cards.push_back(*card);
    }

    switch (table->submitPass(seat, cards)) {
    case Table::PassResult::NotPassingPhase:
        return replyFail(conn, request, ErrorCode::NotPassingPhase, "there's no passing right now");
    case Table::PassResult::AlreadyPassed:
        return replyFail(conn, request, ErrorCode::AlreadyPassed, "you already passed your cards");
    case Table::PassResult::WrongCount:
        return replyFail(conn, request, ErrorCode::InvalidCards,
                         QString("pass exactly %1 cards").arg(Table::kCardsToPass));
    case Table::PassResult::NotInHand:
        return replyFail(conn, request, ErrorCode::InvalidCards, "you can only pass cards from your hand");
    case Table::PassResult::Duplicate:
        return replyFail(conn, request, ErrorCode::InvalidCards, "you picked the same card twice");
    case Table::PassResult::Ok:
        break;
    }

    replyOk(conn, request, "cards chosen, waiting for the other players");
    sendToLobby(*game, "player_passed", {{"username", conn.username}}, *conn.playerId);

    if (!table->allPassed())
        return;

    // Everyone has picked: move the cards, then tell each player privately
    // what they got and show their new hand.
    table->applyPasses();
    const qint64 id = m_nextServerRequestId--;
    for (int s = 0; s < Table::kSeats; ++s)
        sendToSeat(*game, s, "cards_received",
                   {{"from",  table->seat(table->passSource(s)).username},
                    {"cards", cardsToJson(table->receivedCards(s))},
                    {"hand",  cardsToJson(table->hand(s))}},
                   id);
    logMessage(QString("[Game] lobby %1: cards passed").arg(game->lobbyId()));

    // Use case 17 step 3: the 2 of Clubs holder leads the first trick.
    announceTurn(*game, table->leadSeat());
    emit stateChanged();
}

// ===========================================================================
//  In-game: playing tricks (issue #66 / use case 18 "Adjudicate a Trick")
//
//    player     -> play_card    {card:"QS"}
//    player     <- play_card_result {hand}                 (your hand without it)
//    everyone   <- card_played  {username, playerNum, card, trick, cardsOnTable, heartsBroken}
//    then either
//    everyone   <- player_turn  {username, playerNum, trick, leading, [legalCards]}   next player
//    or, after the 4th card:
//    everyone   <- trick_won    {username, playerNum, trick, cards, points}
//    everyone   <- player_turn  ...the winner leads the next trick
//  After trick 13 the round is scored (finishRound, issue #69).
// ===========================================================================

// play_card {card}  ->  play_card_result
void Server::handlePlayCard(Connection& conn, const Message& request)
{
    GameManager* game = nullptr;
    int seat = -1;
    if (!findSeat(conn, request, game, seat))
        return;
    Table* table = game->table();

    const QString code = request.body.value("card").toString();
    const std::optional<Card> card = Card::fromCode(code);
    if (!card)
        return replyFail(conn, request, ErrorCode::InvalidCards,
                         "\"" + code + "\" isn't a card (use codes like \"QS\", \"10H\")");

    // Use case 18 step 1: validate against the Hearts rules (GameState.h).
    // Use case 18 extension a: reject an invalid play; the player picks again.
    switch (table->playCard(seat, *card)) {
    case GameState::PlayResult::NotYourTurn:
        return replyFail(conn, request, ErrorCode::NotYourTurn,
                         table->phase() == Table::Phase::Passing ? "cards are still being passed"
                                                                 : "it's not your turn");
    case GameState::PlayResult::NotInHand:
        return replyFail(conn, request, ErrorCode::CardNotInHand, "you don't have that card");
    case GameState::PlayResult::MustLeadTwoOfClubs:
        return replyFail(conn, request, ErrorCode::MustLeadTwoOfClubs,
                         "the first trick must be led with the 2 of Clubs");
    case GameState::PlayResult::MustFollowSuit:
        return replyFail(conn, request, ErrorCode::MustFollowSuit,
                         "you must follow suit (play a card of the suit that was led)");
    case GameState::PlayResult::NoPointsFirstTrick:
        return replyFail(conn, request, ErrorCode::NoPointsFirstTrick,
                         "you can't play a heart or the Queen of Spades on the first trick");
    case GameState::PlayResult::HeartsNotBroken:
        return replyFail(conn, request, ErrorCode::HeartsNotBroken,
                         "you can't lead hearts until hearts have been broken");
    case GameState::PlayResult::Ok:
        break;
    }

    const GameState& state = table->gameState();
    replyOk(conn, request, "card played", {{"hand", cardsToJson(state.hand(seat))}});

    // Use case 18 step 2: show everyone the trick so far.
    QJsonArray onTable;
    for (const GameState::PlayedCard& p : state.getPlayedCards())
        onTable.append(QJsonObject{{"username", table->seat(p.seat).username},
                                   {"playerNum", p.seat + 1},
                                   {"card", p.card.code()}});
    const int trick = state.trickNumber();
    sendToLobby(*game, "card_played",
                {{"username", conn.username}, {"playerNum", seat + 1}, {"card", card->code()},
                 {"trick", trick}, {"cardsOnTable", onTable}, {"heartsBroken", state.heartsBroken()}});

    // Use case 18 step 3: next player, until all four have played.
    if (!state.trickComplete()) {
        announceTurn(*game, state.getActivePlayer());
        emit stateChanged();
        return;
    }

    // Use case 18 step 4: the winner takes the trick.
    const GameState::TrickResult result = table->determineTrickWinner();
    const QString winner = table->seat(result.winnerSeat).username;
    sendToLobby(*game, "trick_won",
                {{"username", winner}, {"playerNum", result.winnerSeat + 1}, {"trick", trick},
                 {"cards", cardsToJson(result.cards)}, {"points", result.points}});
    logMessage(QString("[Game] lobby %1: trick %2 won by \"%3\" (%4 point(s))")
                   .arg(game->lobbyId()).arg(trick).arg(winner).arg(result.points));

    if (table->roundOver())
        finishRound(*game);
    else
        announceTurn(*game, result.winnerSeat);   // the winner leads the next trick
    emit stateChanged();
}

// ===========================================================================
//  Scoring and game end (issue #69 / use case 19, issue #68 / use case 20)
//
//    everyone <- round_ended {round, roundPoints, scores, shotTheMoon}
//    then, if nobody has reached 100:
//    each seat <- round_started ...the next round (pass direction rotates)
//    or, if someone has:
//    everyone <- game_ended {players, scores, places, winners, matchId, saved}
// ===========================================================================

static QJsonArray intsToJson(const std::array<int, Table::kSeats>& values)
{
    QJsonArray array;
    for (int v : values)
        array.append(v);
    return array;
}

void Server::finishRound(GameManager& game)
{
    Table& table = *game.table();

    // Use case 19 steps 1-2: score the round (with Shoot the Moon) and show
    // everyone the new totals.
    const Table::RoundScore round = table.updateScore();
    const QString shooter = round.moonShooter >= 0 ? table.seat(round.moonShooter).username : QString();
    sendToLobby(game, "round_ended",
                {{"round",       table.roundNumber()},
                 {"roundPoints", intsToJson(round.points)},
                 {"scores",      intsToJson(table.scores())},
                 {"shotTheMoon", shooter}});   // "" if nobody did
    logMessage(QString("[Game] lobby %1: round %2 scored, totals %3/%4/%5/%6%7")
                   .arg(game.lobbyId()).arg(table.roundNumber())
                   .arg(table.scores()[0]).arg(table.scores()[1])
                   .arg(table.scores()[2]).arg(table.scores()[3])
                   .arg(shooter.isEmpty() ? QString() : ", \"" + shooter + "\" shot the moon"));

    // Use case 19 step 3: nobody at 100 yet -> deal the next round
    // (use case 17 again). Otherwise the game is over (use case 20).
    if (!table.gameOver()) {
        table.startNewRound();
        announceRound(game);
        return;
    }
    endGame(game);
}

void Server::endGame(GameManager& game)
{
    game.endGame();
    const Table& table = *game.table();
    const std::array<int, Table::kSeats> places = table.places();

    // Use case 20 step 2: record the match for every player ("saveMatch" in
    // the class diagram). Wins/losses are counted from these rows.
    std::vector<MatchPlayer> rows;
    QJsonArray players, winners;
    for (int s = 0; s < Table::kSeats; ++s) {
        rows.push_back({table.seat(s).playerId, table.seat(s).username, s, table.scores()[s], places[s]});
        players.append(table.seat(s).username);
        if (places[s] == 1)
            winners.append(table.seat(s).username);
    }
    const std::optional<int> matchId = m_db.saveMatch(table.roundNumber(), rows);

    // Use case 20 step 1: show the final results and the choice to play
    // again or leave (play again is issue #67). Use case 20 exception: if
    // the match couldn't be saved, say so, but still show the results.
    sendToLobby(game, "game_ended",
                {{"players", players},
                 {"scores",  intsToJson(table.scores())},
                 {"places",  intsToJson(places)},
                 {"winners", winners},
                 {"rounds",  table.roundNumber()},
                 {"matchId", matchId ? *matchId : -1},
                 {"saved",   matchId.has_value()},
                 {"message", matchId ? QString("Game over!")
                                     : QString("An error occurred while trying to reach the database.")}});

    QStringList winnerNames;
    for (const QJsonValue& w : winners)
        winnerNames << w.toString();
    logMessage(QString("[Game] lobby %1: game over after %2 round(s); winner(s): %3; %4")
                   .arg(game.lobbyId()).arg(table.roundNumber()).arg(winnerNames.join(", "))
                   .arg(matchId ? "saved as match " + QString::number(*matchId)
                                : QString("COULD NOT SAVE the match")));
    emit stateChanged();
}

// get_match_history {}  ->  get_match_history_result {matches, stats}
// Use case 6 / issue #87 ("getMatchHistory" in the class diagram).
void Server::handleGetMatchHistory(Connection& conn, const Message& request)
{
    if (!requireLogin(conn, request))
        return;

    const auto history = m_db.getMatchHistory(*conn.playerId);
    if (!history)   // use case 6 exception
        return replyFail(conn, request, ErrorCode::ServerError, "Account systems could not be reached.");

    QJsonArray matches;
    int wins = 0;
    for (const MatchRecord& m : *history) {
        QJsonArray players;
        int myScore = 0, myPlace = 0;
        for (const MatchPlayer& p : m.players) {
            players.append(QJsonObject{{"username", p.username}, {"score", p.score}, {"place", p.place}});
            if (p.playerId == *conn.playerId) {
                myScore = p.score;
                myPlace = p.place;
            }
        }
        if (myPlace == 1)
            ++wins;
        matches.append(QJsonObject{
            {"matchId",  m.matchId},
            {"playedAt", m.playedAt.toString(Qt::ISODate)},
            {"rounds",   m.rounds},
            {"players",  players},       // seat order
            {"score",    myScore},       // this player's final score
            {"place",    myPlace},       // this player's place (1 = won)
            {"won",      myPlace == 1},
        });
    }

    // Statistics ("MatchHistory: matchCount, winLosePercentage").
    const int played = static_cast<int>(history->size());
    const QJsonObject stats{
        {"gamesPlayed",   played},
        {"wins",          wins},
        {"losses",        played - wins},
        {"winPercentage", played == 0 ? 0 : qRound(100.0 * wins / played)},
    };

    // Test case 6.1: no matches -> empty list and a clear message.
    replyOk(conn, request,
            played == 0 ? "No matches played yet." : QString("%1 match(es) played").arg(played),
            {{"matches", matches}, {"stats", stats}});
}

std::vector<Server::GameInfo> Server::viewHeartsGames() const
{
    std::vector<GameInfo> result;
    for (const auto& game : m_games) {
        GameInfo info;
        info.lobbyId = game->lobbyId();
        info.state = game->stateText();
        for (const LobbyPlayer& p : game->lobby().players())
            info.players << p.username;
        result.push_back(info);
    }
    return result;
}

// ===========================================================================
//  Sending
// ===========================================================================

void Server::sendMessage(Connection& conn, const QString& command, qint64 requestId,
                         QJsonObject fields)
{
    fields["command"]   = command;
    fields["requestId"] = requestId;

    const Message msg = makeMessage(Message::Direction::ToClient, fields);
    conn.record(msg);
    logMessage(describe(conn, msg));

    // Compact = one line; '\n' marks the end of the message.
    QByteArray data = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    data.append('\n');
    conn.socket->write(data);
}

void Server::replyResult(Connection& conn, const Message& request, const QString& errorCode,
                         const QString& text, QJsonObject extra)
{
    // success is derived from the error code, so the two can never disagree:
    // no error code means success.
    extra["success"] = errorCode.isEmpty();
    extra["error"]   = errorCode;   // "" on success
    extra["message"] = text;
    sendMessage(conn, request.command + "_result", request.requestId, extra);
}

void Server::replyOk(Connection& conn, const Message& request, const QString& text,
                     QJsonObject extra)
{
    replyResult(conn, request, ErrorCode::None, text, extra);
}

void Server::replyFail(Connection& conn, const Message& request, const QString& errorCode,
                       const QString& text, QJsonObject extra)
{
    replyResult(conn, request, errorCode, text, extra);
}

void Server::replyLocked(Connection& conn, const Message& request, const PlayerRecord& player)
{
    // Whole minutes left, rounded up, so it never says "0 minutes".
    const qint64 secondsLeft = QDateTime::currentDateTime().secsTo(player.lockedUntil);
    const qint64 minutesLeft = (secondsLeft + 59) / 60;

    QJsonObject extra;
    extra["failed_attempts"] = player.failedAttempts;
    extra["lockedUntil"]     = player.lockedUntil.toString(Qt::ISODate);
    replyFail(conn, request, ErrorCode::AccountLocked,
              QString("account locked after %1 failed attempts, try again in %2 minute(s)")
                  .arg(m_config.maxFailedLogins).arg(minutesLeft),
              extra);
}

void Server::sendError(Connection& conn, qint64 requestId, const QString& errorCode,
                       const QString& text)
{
    sendMessage(conn, "error", requestId, {{"error", errorCode}, {"message", text}});
}

void Server::broadcast(const QString& text)
{
    if (m_connections.empty()) {
        logMessage("[System] No clients connected to receive broadcast.");
        return;
    }

    // One server-made id per broadcast, shared by every copy of it.
    const qint64 id = m_nextServerRequestId--;
    for (Connection& c : m_connections)
        if (c.socket->state() == QAbstractSocket::ConnectedState)
            sendMessage(c, "server_message", id, {{"text", text}});
}
