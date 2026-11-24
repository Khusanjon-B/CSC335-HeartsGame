#pragma once
// ---------------------------------------------------------------------------
// Server.h - the TCP server: accepts clients, reads their JSON messages,
// passes each one to the right handler, and sends replies.
//
// The full protocol is in PROTOCOL.md. Short version:
//
// MESSAGE FORMAT (both directions): one JSON object per line, ending in '\n'.
// Every message has "command" and "requestId" (see Message.h for the id rules).
//
//   client -> server   {"command":"login","requestId":7,"username":"alice","password":"pw"}
//   server -> client   {"command":"login_result","requestId":7,"success":true,"error":"",
//                       "message":"welcome alice","playerId":1,"username":"alice"}
//
// Commands:
//   "register"          {username, email, password, pin}         -> "register_result"
//   "login"             {username, password}                     -> "login_result"
//   "logout"            {}                                       -> "logout_result"
//   "recover_password"  {username, email, pin, newPassword}      -> "recover_password_result"
// Every reply is the command name + "_result", with "success", "error" and
// "message". "error" is "" on success, or a code from ErrorCodes.h on failure.
// Problems with the message itself get {"command":"error", "requestId", "error", "message"}.
// Console broadcasts go out as {"command":"server_message", "requestId":<negative>, "text"}.
//
// HOW TO ADD A NEW COMMAND (for example "play_card"):
//   1. Declare   void handlePlayCard(Connection& conn, const Message& request);
//   2. Write it in Server.cpp: read fields from request.body, then call
//        replyOk(conn, request, "text", {extra fields});         on success
//        replyFail(conn, request, ErrorCode::Something, "text"); on failure
//   3. Add one line in registerHandlers():
//         m_handlers["play_card"] = &Server::handlePlayCard;
//   4. Document it in PROTOCOL.md.
// ---------------------------------------------------------------------------

#include "Config.h"
#include "Connection.h"
#include "Database.h"
#include "Message.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <optional>
#include <vector>

class Server : public QObject
{
    Q_OBJECT   // lets this class use Qt signals/slots

public:
    // The Server uses (but doesn't own) the Database created in main().
    // `config` holds the server.ini settings (lockout limits, max games...).
    explicit Server(Database& db, const ServerConfig& config, QObject* parent = nullptr);

    // Starts listening on every IPv4 network card. Returns false on failure.
    bool start(quint16 port);

    // Sends a server_message to every connected client (console + GUI).
    void broadcast(const QString& text);

    // Logs out every logged-in player in the database. Called on shutdown.
    void logoutEveryone();

    // ---- Server Monitoring GUI (admin) queries ----
    // Named after the "Server Side App" methods in the class diagram.
    // They only READ state (except unlockAccount), and run on the main
    // thread like everything else, so no locks are needed.

    // One connected client, as the admin sees it.
    struct ClientInfo
    {
        QString ip;         // "192.168.1.20:53211"
        QString username;   // empty if not logged in
        bool    loggedIn = false;
    };

    std::vector<ClientInfo> viewConnectedUsers() const;   // every open connection (#72)
    std::vector<ClientInfo> viewLoggedInUsers() const;    // only logged-in ones (#65)
    std::optional<int> viewRegisteredUsers();             // account count (#74); nullopt = DB error
    std::optional<std::vector<LockedAccount>> viewLockedAccounts();   // (#73); nullopt = DB error
    bool unlockAccount(const QString& username);          // admin unlock (use case 7.2)

signals:
    // Emitted whenever something the admin can see changes: a client
    // connects/disconnects, logs in/out, registers, gets locked or unlocked.
    // The monitor window connects to this to refresh itself.
    void stateChanged();

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

    // Replies to a request with "<command>_result". Every reply has:
    //   success  true/false
    //   error    "" on success, an ErrorCode on failure
    //   message  human-readable text (for logs or display, not for logic)
    // replyOk / replyFail are the two shortcuts handlers actually call.
    void replyResult(Connection& conn, const Message& request, const QString& errorCode,
                     const QString& text, QJsonObject extra);
    void replyOk(Connection& conn, const Message& request, const QString& text,
                 QJsonObject extra = {});
    void replyFail(Connection& conn, const Message& request, const QString& errorCode,
                   const QString& text, QJsonObject extra = {});

    // login_result for a locked account: ACCOUNT_LOCKED + failed_attempts
    // + lockedUntil, with the minutes left in the message.
    void replyLocked(Connection& conn, const Message& request, const PlayerRecord& player);

    // Sends {"command":"error","requestId":id,"error":code,"message":text}.
    // Used when the message itself is broken, so there's no handler to reply.
    void sendError(Connection& conn, qint64 requestId, const QString& errorCode,
                   const QString& text);

    // ---- data ----
    Database&               m_db;
    ServerConfig            m_config;        // copy of the server.ini settings
    QTcpServer              m_tcpServer;
    std::vector<Connection> m_connections;   // one entry per connected client
    QHash<QString, Handler> m_handlers;      // "login" -> &Server::handleLogin
    qint64                  m_nextServerRequestId = -1;   // -1, -2, -3, ...
};
