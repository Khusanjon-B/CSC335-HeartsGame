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
// touch m_connections or the database at the same moment.
// ---------------------------------------------------------------------------

#include "Server.h"
#include "Log.h"
#include "PasswordHasher.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <algorithm>

// ---- input rules (change them here) ----
static constexpr int kMinPinLength = 4;
static constexpr int kMaxPinLength = 8;

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

Server::Server(Database& db, QObject* parent)
    : QObject(parent)
    , m_db(db)
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
    if (conn.playerId)
        m_db.markLoggedOut(*conn.playerId);

    conn.playerId.reset();   // back to anonymous; the socket stays open
    conn.username.clear();
    conn.verified = false;
}

void Server::logoutEveryone()
{
    for (Connection& c : m_connections)
        if (c.verified)
            logoutConnection(c);
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
        sendError(conn, 0, parseError.error != QJsonParseError::NoError
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
        sendError(conn, 0, "every message needs a \"requestId\" that is a positive whole number");
        return;
    }

    // 4. Find the handler for this command and call it.
    const auto it = m_handlers.constFind(request.command);
    if (it == m_handlers.constEnd()) {
        sendError(conn, request.requestId, "unknown command: \"" + request.command + "\"");
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

    if (username.isEmpty() || email.isEmpty() || password.isEmpty() || pin.isEmpty())
        return replyResult(conn, request, false, "username, email, password and pin are all required");
    if (!looksLikeEmail(email))
        return replyResult(conn, request, false, "that email address doesn't look valid");
    if (!isValidPin(pin))
        return replyResult(conn, request, false,
                           QString("pin must be a string of %1-%2 digits").arg(kMinPinLength).arg(kMaxPinLength));
    if (m_db.findPlayer(username))
        return replyResult(conn, request, false, "username already taken");

    // Hash both secrets (each gets its own random salt) and save.
    const int newId = m_db.createPlayer(username, email,
                                        PasswordHasher::hash(password),
                                        PasswordHasher::hash(pin));
    if (newId < 0)
        return replyResult(conn, request, false, "could not create account");

    // Registering doesn't log you in. The client sends "login" next.
    logMessage("[Auth] registered \"" + username + "\" as player " + QString::number(newId));
    replyResult(conn, request, true, "account created, you can now log in",
                {{"playerId", newId}});
}

void Server::handleLogin(Connection& conn, const Message& request)
{
    const QString username = request.body.value("username").toString().trimmed();
    const QString password = request.body.value("password").toString();

    if (conn.verified)
        return replyResult(conn, request, false, "already logged in as " + conn.username);
    if (username.isEmpty() || password.isEmpty())
        return replyResult(conn, request, false, "username and password are required");

    // Hash what they typed with the stored salt and compare to the stored hash.
    // Same message for "no such user" and "wrong password", so nobody can
    // use this to find out which usernames exist.
    const std::optional<PlayerRecord> player = m_db.findPlayer(username);
    if (!player || !PasswordHasher::verify(password, player->passwordHash, player->passwordSalt)) {
        logMessage("[Auth] login failed for " + conn.label() + " as \"" + username + "\"");
        return replyResult(conn, request, false, "wrong username or password");
    }

    // One login per account at a time.
    if (findConnectionByPlayer(player->id))
        return replyResult(conn, request, false,
                           "this account is already logged in from another connection");

    // Success: attach the player to this connection, then update the DB.
    conn.playerId = player->id;
    conn.username = player->username;
    conn.verified = true;
    m_db.markLoggedIn(player->id);

    logMessage("[Auth] " + conn.ip + " logged in as \"" + conn.username +
               "\" (player " + QString::number(player->id) + ")");
    replyResult(conn, request, true, "welcome " + conn.username,
                {{"playerId", player->id}, {"username", conn.username}});
}

void Server::handleLogout(Connection& conn, const Message& request)
{
    if (!conn.verified)
        return replyResult(conn, request, false, "you are not logged in");

    logMessage("[Auth] \"" + conn.username + "\" logged out");
    logoutConnection(conn);
    replyResult(conn, request, true, "logged out");
}

void Server::handleRecoverPassword(Connection& conn, const Message& request)
{
    const QJsonObject& b = request.body;
    const QString username    = b.value("username").toString().trimmed();
    const QString email       = b.value("email").toString().trimmed().toLower();
    const QString pin         = b.value("pin").toString();
    const QString newPassword = b.value("newPassword").toString();

    if (username.isEmpty() || email.isEmpty() || pin.isEmpty() || newPassword.isEmpty())
        return replyResult(conn, request, false,
                           "username, email, pin and newPassword are all required");

    // All three (username, email, pin) must match. One shared failure
    // message, so nobody can tell which part was wrong.
    const std::optional<PlayerRecord> player = m_db.findPlayer(username);
    if (!player || player->email != email ||
        !PasswordHasher::verify(pin, player->pinHash, player->pinSalt)) {
        logMessage("[Auth] password recovery failed for \"" + username + "\" from " + conn.label());
        return replyResult(conn, request, false, "those recovery details don't match our records");
    }

    // New password gets a new salt too.
    if (!m_db.updatePassword(player->id, PasswordHasher::hash(newPassword)))
        return replyResult(conn, request, false, "could not update password");

    logMessage("[Auth] password reset for \"" + username + "\"");
    replyResult(conn, request, true, "password changed, you can now log in");
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

void Server::replyResult(Connection& conn, const Message& request, bool success,
                         const QString& text, QJsonObject extra)
{
    extra["success"] = success;
    extra["message"] = text;
    sendMessage(conn, request.command + "_result", request.requestId, extra);
}

void Server::sendError(Connection& conn, qint64 requestId, const QString& text)
{
    sendMessage(conn, "error", requestId, {{"message", text}});
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
