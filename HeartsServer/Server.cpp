// ---------------------------------------------------------------------------
// Server.cpp - see Server.h for the message format and how to add commands.
//
// HOW THIS RUNS ON ONE THREAD (no locks needed):
// Qt sockets are asynchronous. We never sit in a loop waiting for data.
// Instead we connect signals to our functions, and Qt's event loop
// (app.exec() in main) calls them when something happens:
//
//   QTcpServer::newConnection  -> onNewConnection()  a client connected
//   QTcpSocket::readyRead      -> onReadyRead()      bytes arrived
//   QTcpSocket::disconnected   -> onDisconnected()   a client left
//
// All of these run on the main thread, one at a time, so two of them can
// never touch m_connections or the database at the same moment. That's why
// we don't need mutexes. Each call finishes fast and returns, so the event
// loop is free to handle the next client right away.
// ---------------------------------------------------------------------------

#include "Server.h"
#include "Log.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <algorithm>

Server::Server(Database& db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{
    registerHandlers();

    // When a client finishes connecting, Qt emits newConnection. We hook it
    // to our onNewConnection() member function.
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

void Server::registerHandlers()
{
    // Command name (the "command" field in the JSON) -> function to run.
    // C++ can't switch on strings, so this lookup table does the job of
    // the switch statement. Add new commands here.
    m_handlers["register"] = &Server::handleRegister;
    m_handlers["login"]    = &Server::handleLogin;
    m_handlers["logout"]   = &Server::handleLogout;
}

bool Server::start(quint16 port)
{
    // AnyIPv4 = 0.0.0.0, which accepts connections on every network card,
    // so other machines on the LAN can reach us.
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
    // Several clients can connect at once, so take every one that's waiting.
    while (m_tcpServer.hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer.nextPendingConnection();

        // Create the Connection record. It starts anonymous: no playerId,
        // not verified.
        Connection conn;
        conn.socket = socket;
        conn.ip = socket->peerAddress().toString() + ":" +
                  QString::number(socket->peerPort());
        m_connections.push_back(conn);

        logMessage("connected     " + conn.ip);

        // Hook up this socket's signals. The lambdas capture `socket` so we
        // know which client the signal came from.
        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket] { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this,
                [this, socket] { onDisconnected(socket); });
    }
}

void Server::onReadyRead(QTcpSocket* socket)
{
    // TCP is a stream, so one readyRead can hold half a message or several
    // messages. canReadLine() is only true once a full '\n'-terminated
    // line is buffered. Partial lines wait for the next readyRead.
    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Look the Connection up fresh for every line rather than keeping a
        // pointer around. (A pointer into a std::vector can become invalid
        // when the vector grows.)
        Connection* conn = findConnection(socket);
        if (!conn)
            return;

        // NOTE: this prints passwords in plain text. Fine while debugging,
        // remove it before this is used for real.
        logMessage(conn->ip + "  > " + QString::fromUtf8(line));
        processLine(*conn, line);
    }
}

void Server::onDisconnected(QTcpSocket* socket)
{
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
                           [socket](const Connection& c) { return c.socket == socket; });
    if (it == m_connections.end())
        return;

    logMessage("disconnected  " + it->ip +
               (it->verified ? "  (" + it->username + ")" : QString()));

    // If they were logged in, record the logout in the database.
    if (it->verified)
        logoutConnection(*it);

    m_connections.erase(it);

    // deleteLater() frees the socket once Qt is done with the current signal.
    // Deleting it right now, while it's still emitting, could crash.
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

    // Back to anonymous. The socket itself stays connected.
    conn.playerId.reset();
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
//  Message handling
// ===========================================================================

void Server::processLine(Connection& conn, const QByteArray& line)
{
    // Qt's built-in JSON parser. QJsonObject works like a map:
    //   msg["username"].toString(), msg.contains("password"), etc.
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        sendError(conn, "invalid JSON: " + parseError.errorString());
        return;
    }
    if (!doc.isObject()) {
        sendError(conn, "message must be a JSON object like {\"command\":\"login\", ...}");
        return;
    }

    const QJsonObject msg = doc.object();
    const QString command = msg.value("command").toString();

    // Find the handler for this command and call it.
    const auto it = m_handlers.constFind(command);
    if (it == m_handlers.constEnd()) {
        sendError(conn, "unknown command: \"" + command + "\"");
        return;
    }

    // `it.value()` is a member-function pointer, and (this->*ptr)(args)
    // calls it on this object. Same as calling handleLogin(conn, msg) directly.
    const Handler handler = it.value();
    (this->*handler)(conn, msg);
}

void Server::handleRegister(Connection& conn, const QJsonObject& msg)
{
    const QString username = msg.value("username").toString().trimmed();
    const QString password = msg.value("password").toString();

    QJsonObject reply;
    reply["command"] = "register_result";

    if (username.isEmpty() || password.isEmpty()) {
        reply["success"] = false;
        reply["message"] = "username and password are required";
        sendJson(conn, reply);
        return;
    }

    if (m_db.findPlayer(username)) {
        reply["success"] = false;
        reply["message"] = "username already taken";
        sendJson(conn, reply);
        return;
    }

    const int newId = m_db.createPlayer(username, password);
    if (newId < 0) {
        reply["success"] = false;
        reply["message"] = "could not create account";
        sendJson(conn, reply);
        return;
    }

    // Registering doesn't log you in. The client sends "login" next.
    logMessage("[Auth] registered \"" + username + "\" as player " + QString::number(newId));
    reply["success"]  = true;
    reply["message"]  = "account created, you can now log in";
    reply["playerId"] = newId;
    sendJson(conn, reply);
}

void Server::handleLogin(Connection& conn, const QJsonObject& msg)
{
    const QString username = msg.value("username").toString().trimmed();
    const QString password = msg.value("password").toString();

    QJsonObject reply;
    reply["command"] = "login_result";

    // Small helper so every failure is reported the same way.
    auto fail = [&](const QString& why) {
        logMessage("[Auth] login failed for " + conn.ip + ": " + why);
        reply["success"] = false;
        reply["message"] = why;
        sendJson(conn, reply);
    };

    if (conn.verified) {
        fail("already logged in as " + conn.username);
        return;
    }
    if (username.isEmpty() || password.isEmpty()) {
        fail("username and password are required");
        return;
    }

    // Look the player up and compare passwords.
    // (Plain-text comparison for now. With hashing, this would compare
    //  hash(password) against the stored hash.)
    const std::optional<PlayerRecord> player = m_db.findPlayer(username);
    if (!player || player->password != password) {
        // Same message for "no such user" and "wrong password", so nobody
        // can use this to find out which usernames exist.
        fail("wrong username or password");
        return;
    }

    // One login per account at a time.
    if (findConnectionByPlayer(player->id)) {
        fail("this account is already logged in from another connection");
        return;
    }

    // Success: attach the player to this connection, then update the DB.
    conn.playerId = player->id;
    conn.username = player->username;
    conn.verified = true;
    m_db.markLoggedIn(player->id);

    logMessage("[Auth] " + conn.ip + " logged in as \"" + conn.username +
               "\" (player " + QString::number(player->id) + ")");

    reply["success"]  = true;
    reply["message"]  = "welcome " + conn.username;
    reply["playerId"] = player->id;
    reply["username"] = conn.username;
    sendJson(conn, reply);
}

void Server::handleLogout(Connection& conn, const QJsonObject& /*msg*/)
{
    QJsonObject reply;
    reply["command"] = "logout_result";

    if (!conn.verified) {
        reply["success"] = false;
        reply["message"] = "you are not logged in";
        sendJson(conn, reply);
        return;
    }

    logMessage("[Auth] \"" + conn.username + "\" logged out");
    logoutConnection(conn);

    reply["success"] = true;
    reply["message"] = "logged out";
    sendJson(conn, reply);
}

// ===========================================================================
//  Sending
// ===========================================================================

void Server::sendJson(Connection& conn, const QJsonObject& obj)
{
    // Compact = everything on one line. Then add the '\n' that marks the end
    // of the message, which is what the client should split on.
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    conn.socket->write(data);
}

void Server::sendError(Connection& conn, const QString& text)
{
    logMessage("[Error] to " + conn.ip + ": " + text);
    QJsonObject reply;
    reply["command"] = "error";
    reply["message"] = text;
    sendJson(conn, reply);
}

void Server::broadcast(const QString& text)
{
    if (m_connections.empty()) {
        logMessage("[System] No clients connected to receive broadcast.");
        return;
    }

    logMessage("[Broadcast] " + text);
    QJsonObject msg;
    msg["command"] = "server_message";
    msg["text"]    = text;

    for (Connection& c : m_connections)
        if (c.socket->state() == QAbstractSocket::ConnectedState)
            sendJson(c, msg);
}
