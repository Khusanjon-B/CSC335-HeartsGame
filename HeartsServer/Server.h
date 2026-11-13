#pragma once
// ---------------------------------------------------------------------------
// Server.h - the TCP server: accepts clients, reads their JSON messages, and
// sends each one to the right handler function.
//
// MESSAGE FORMAT (both directions): one JSON object per line, ending in '\n'.
//
//   client -> server   {"command":"login","username":"alice","password":"pw"}
//   server -> client   {"command":"login_result","success":true,
//                       "message":"welcome alice","playerId":1,"username":"alice"}
//
// Commands we handle right now:
//   "register"  {username, password} -> "register_result"  (creates an account)
//   "login"     {username, password} -> "login_result"
//   "logout"    {}                   -> "logout_result"
// Anything we can't understand gets back {"command":"error","message":"..."}.
// Console broadcasts go out as {"command":"server_message","text":"..."}.
//
// HOW TO ADD A NEW COMMAND (for example "play_card"):
//   1. Declare   void handlePlayCard(Connection& conn, const QJsonObject& msg);
//   2. Write it in Server.cpp (read fields from msg, then reply with sendJson).
//   3. Add one line in registerHandlers():
//         m_handlers["play_card"] = &Server::handlePlayCard;
// ---------------------------------------------------------------------------

#include "Connection.h"
#include "Database.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <vector>

class Server : public QObject
{
    // Q_OBJECT lets this class use Qt signals/slots. We don't declare any
    // custom ones yet, but it's here for when we do.
    Q_OBJECT

public:
    // The Server doesn't own the Database; it just uses the one from main().
    explicit Server(Database& db, QObject* parent = nullptr);

    // Starts listening on every IPv4 network card. Returns false on failure.
    bool start(quint16 port);

    // Sends a server_message to every connected client (used by the console).
    void broadcast(const QString& text);

    // Logs out every logged-in player in the database. Called on shutdown.
    void logoutEveryone();

private:
    // A "handler" is a pointer to one of our handleXxx member functions.
    // Every handler takes the same two arguments: who sent the message, and
    // the parsed JSON.
    using Handler = void (Server::*)(Connection& conn, const QJsonObject& msg);

    // Fills m_handlers: command name -> handler function.
    void registerHandlers();

    // ---- connection bookkeeping ----
    void onNewConnection();                    // a client just connected
    void onReadyRead(QTcpSocket* socket);      // bytes arrived from a client
    void onDisconnected(QTcpSocket* socket);   // a client went away

    // Finds the Connection that owns this socket (nullptr if none).
    Connection* findConnection(QTcpSocket* socket);
    // Finds the Connection logged in as this player (nullptr if none).
    Connection* findConnectionByPlayer(int playerId);

    // Clears login info on a connection and updates the database.
    void logoutConnection(Connection& conn);

    // ---- message handling ----
    // Parses one line of text as JSON and passes it to the right handler.
    void processLine(Connection& conn, const QByteArray& line);

    void handleRegister(Connection& conn, const QJsonObject& msg);
    void handleLogin(Connection& conn, const QJsonObject& msg);
    void handleLogout(Connection& conn, const QJsonObject& msg);

    // ---- sending ----
    // Converts a JSON object to one line of text and writes it to the client.
    void sendJson(Connection& conn, const QJsonObject& obj);
    // Shortcut for {"command":"error","message":text}.
    void sendError(Connection& conn, const QString& text);

    // ---- data ----
    Database&               m_db;          // shared database (owned by main)
    QTcpServer              m_tcpServer;   // Qt's listening socket
    std::vector<Connection> m_connections; // one entry per connected client
    QHash<QString, Handler> m_handlers;    // "login" -> &Server::handleLogin
};
