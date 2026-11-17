#pragma once
// ---------------------------------------------------------------------------
// Server.h - the TCP server: accepts clients, reads their JSON messages,
// passes each one to the right handler, and sends replies.
//
// MESSAGE FORMAT (both directions): one JSON object per line, ending in '\n'.
// Every message has "command" and "requestId" (see Message.h for the id rules).
//
//   client -> server   {"command":"login","requestId":7,"username":"alice","password":"pw"}
//   server -> client   {"command":"login_result","requestId":7,"success":true,
//                       "message":"welcome alice","playerId":1,"username":"alice"}
//
// Commands:
//   "register"          {username, email, password, pin}         -> "register_result"
//   "login"             {username, password}                     -> "login_result"
//   "logout"            {}                                       -> "logout_result"
//   "recover_password"  {username, email, pin, newPassword}      -> "recover_password_result"
// Every reply is the command name + "_result", with "success" and "message".
// Problems with the message itself get {"command":"error", "requestId", "message"}.
// Console broadcasts go out as {"command":"server_message", "requestId":<negative>, "text"}.
//
// HOW TO ADD A NEW COMMAND (for example "play_card"):
//   1. Declare   void handlePlayCard(Connection& conn, const Message& request);
//   2. Write it in Server.cpp: read fields from request.body, then call
//      replyResult(conn, request, true/false, "text", {extra fields}).
//   3. Add one line in registerHandlers():
//         m_handlers["play_card"] = &Server::handlePlayCard;
// ---------------------------------------------------------------------------

#include "Connection.h"
#include "Database.h"
#include "Message.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <vector>

class Server : public QObject
{
    Q_OBJECT   // lets this class use Qt signals/slots

public:
    // The Server uses (but doesn't own) the Database created in main().
    explicit Server(Database& db, QObject* parent = nullptr);

    // Starts listening on every IPv4 network card. Returns false on failure.
    bool start(quint16 port);

    // Sends a server_message to every connected client (used by the console).
    void broadcast(const QString& text);

    // Logs out every logged-in player in the database. Called on shutdown.
    void logoutEveryone();

private:
    // Every handler has the same shape: who sent it, and what they sent.
    using Handler = void (Server::*)(Connection& conn, const Message& request);

    // Fills m_handlers: command name -> handler function.
    void registerHandlers();

    // ---- connection bookkeeping ----
    void onNewConnection();                    // a client connected
    void onReadyRead(QTcpSocket* socket);      // bytes arrived from a client
    void onDisconnected(QTcpSocket* socket);   // a client left

    Connection* findConnection(QTcpSocket* socket);   // socket -> Connection
    Connection* findConnectionByPlayer(int playerId);  // player_id -> Connection
    void logoutConnection(Connection& conn);          // clear login + update DB

    // ---- receiving ----
    // Parses one line as JSON, checks it, records it, and calls its handler.
    void processLine(Connection& conn, const QByteArray& line);

    void handleRegister(Connection& conn, const Message& request);
    void handleLogin(Connection& conn, const Message& request);
    void handleLogout(Connection& conn, const Message& request);
    void handleRecoverPassword(Connection& conn, const Message& request);

    // ---- sending ----
    // The ONE place messages leave the server: adds command + requestId,
    // records the message in conn.history, logs it, and writes it.
    void sendMessage(Connection& conn, const QString& command, qint64 requestId,
                     QJsonObject fields = {});

    // Reply to a request with "<command>_result", success, message, plus
    // any extra fields. Used by all the handlers.
    void replyResult(Connection& conn, const Message& request, bool success,
                     const QString& text, QJsonObject extra = {});

    // Shortcut for {"command":"error","requestId":id,"message":text}.
    void sendError(Connection& conn, qint64 requestId, const QString& text);

    // ---- data ----
    Database&               m_db;
    QTcpServer              m_tcpServer;
    std::vector<Connection> m_connections;   // one entry per connected client
    QHash<QString, Handler> m_handlers;      // "login" -> &Server::handleLogin
    qint64                  m_nextServerRequestId = -1;   // -1, -2, -3, ...
};
