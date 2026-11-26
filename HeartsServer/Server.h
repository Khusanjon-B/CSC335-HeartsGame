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
//   "change_password"   {currentPassword, newPassword}           -> "change_password_result"
//   "ping"              {}                                       -> "ping_result"
//   ...plus the lobby and game commands listed in PROTOCOL.md.
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
#include "GameManager.h"
#include "Message.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <memory>
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

    // One game session, as the admin sees it ("displayHeartsGames()").
    struct GameInfo
    {
        int         lobbyId = 0;
        QString     state;     // "Waiting for players" / "In game"
        QStringList players;   // seat order, host first
    };
    std::vector<GameInfo> viewHeartsGames() const;

signals:
    // Emitted whenever something the admin can see changes: a client
    // connects/disconnects, logs in/out, registers, gets locked or unlocked,
    // or a lobby/game is created, joined, left, started or disbanded.
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
    void handleChangePassword(Connection& conn, const Message& request);   // use case 4 (#86)
    void handlePing(Connection& conn, const Message& request);             // use case 22 (#89)

    // ---- game sessions / lobbies (issue #64, #63; use cases 8-11) ----
    void handleStartLobby(Connection& conn, const Message& request);   // create + become host
    void handleGetLobbies(Connection& conn, const Message& request);   // public list of open lobbies
    void handleJoinLobby(Connection& conn, const Message& request);    // join by lobbyId
    void handleLeaveLobby(Connection& conn, const Message& request);   // leave (host leaving disbands)
    void handleStartGame(Connection& conn, const Message& request);    // host starts a full lobby

    // ---- in-game (issues #71, #70, #66; use cases 16, 17, 18) ----
    void handlePassCards(Connection& conn, const Message& request);    // pick 3 cards to pass
    void handlePlayCard(Connection& conn, const Message& request);     // play one card to the trick

    // Tells each player privately about the new round: their hand and the
    // pass direction (round_started). If it's a hold round, play starts at once.
    void announceRound(GameManager& game);

    // Tells everyone whose turn it is (player_turn). The player whose turn
    // it is also gets "legalCards": the cards the rules let them play now.
    void announceTurn(GameManager& game, int seatIndex);

    // Called after the 13th trick of a round: scores it (round_ended), then
    // deals the next round, or ends the game if someone reached 100 (#69).
    void finishRound(GameManager& game);

    // The game is over: saves the match to the DB and sends game_ended with
    // the final scores, places and winners (#68, use case 20).
    void endGame(GameManager& game);

    // ---- match history (issue #87, use case 6) ----
    void handleGetMatchHistory(Connection& conn, const Message& request);

    // ---- play again (issue #67, use case 21) ----
    // Vote yes ({vote:true}, the default) or no. When all 4 say yes, a new
    // game starts with the same players; a "no" ends it for everyone.
    void handlePlayAgain(Connection& conn, const Message& request);

    // ---- disconnects mid-game (issue #88, test case 19.2) ----

    // Called when a logged-in connection drops. If it's a non-host seated
    // at a running game, the game is paused (player_disconnected) and their
    // seat is kept. Otherwise nothing happens here and the normal leave
    // rules apply.
    void pauseForDisconnect(Connection& conn);

    // Called after every successful login. If this player has a seat in a
    // paused game, puts them back (game_rejoined) and resumes it once
    // nobody is missing.
    void rejoinGame(Connection& conn);

    // Host gives up waiting for a disconnected player: game_stopped for all.
    void handleDisbandGame(Connection& conn, const Message& request);

    // Replies GAME_PAUSED and returns true if the game is waiting for someone.
    bool refuseIfPaused(Connection& conn, const Message& request, const GameManager& game);

    // Sends `command` to everyone in the session, takes them all out of it,
    // and deletes the GameManager. Used for host_left and game_stopped.
    void disbandGame(GameManager& game, const QString& command, QJsonObject fields);

    // Finds the game and seat this connection is playing in. Replies
    // NOT_IN_GAME and returns false if there isn't one.
    bool findSeat(Connection& conn, const Message& request, GameManager*& game, int& seat);

    // Sends a server-initiated message to ONE seat at the table.
    void sendToSeat(const GameManager& game, int seatIndex, const QString& command,
                    QJsonObject fields, qint64 requestId);

    // Replies NOT_LOGGED_IN and returns false if this connection isn't logged in.
    bool requireLogin(Connection& conn, const Message& request);

    // The session with this id, or nullptr.
    GameManager* findGame(int lobbyId);

    // Takes `conn` out of its lobby and tells the others. If it was the host,
    // the whole session is disbanded (host_left). Used by leave_lobby,
    // logout and disconnect, so all three behave the same.
    void leaveLobby(Connection& conn);

    // Sends one server-initiated event to every player in the session
    // (optionally skipping one), all with the same negative requestId.
    void sendToLobby(const GameManager& game, const QString& command, QJsonObject fields,
                     int skipPlayerId = -1);

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

    // Every game session (lobby or running game). At most
    // m_config.maxConcurrentGames of them. unique_ptr = the Server owns them,
    // and erasing one from the vector deletes it.
    std::vector<std::unique_ptr<GameManager>> m_games;
    int m_nextLobbyId = 1;   // lobby ids are 1, 2, 3, ... and never reused while running
};
