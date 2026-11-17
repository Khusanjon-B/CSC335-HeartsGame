#pragma once
// ---------------------------------------------------------------------------
// Connection.h - everything the server knows about ONE connected client.
//
// The Server keeps a std::vector<Connection> called m_connections.
//   - A Connection is added the moment a client opens a TCP connection.
//   - It starts out "anonymous": no playerId, no username, verified = false.
//   - A successful "login" fills in playerId/username/verified.
//   - "logout" clears them again (the socket stays open).
//   - When the socket disconnects, the Connection is removed.
//
// To track something new per client (current game, seat, ...), add a field
// here.
// ---------------------------------------------------------------------------

#include "Message.h"

#include <QString>
#include <QTcpSocket>
#include <deque>
#include <optional>

struct Connection
{
    // How many messages to keep in `history` per client. Older ones are
    // dropped so a long session can't use unlimited memory.
    static constexpr std::size_t kMaxHistory = 100;

    // The Qt socket for this client. The Server manages its lifetime.
    QTcpSocket* socket = nullptr;

    // "ip:port", e.g. "192.168.1.20:53211".
    QString ip;

    // players.player_id this connection is logged in as.
    // std::optional = may be empty (C++'s version of null):
    //   if (conn.playerId)  -> logged in, the id is *conn.playerId
    //   if (!conn.playerId) -> nobody logged in
    std::optional<int> playerId;

    // Username of the logged-in player (empty until login).
    QString username;

    // True once the client has logged in successfully.
    bool verified = false;

    // The last kMaxHistory messages to and from this client, oldest first.
    // Passwords and pins are already replaced with "***".
    // (A deque is like a vector that can also remove from the front cheaply.)
    std::deque<Message> history;

    // Adds a message to history (with secrets hidden), dropping the oldest
    // if we're over the limit.
    void record(Message msg)
    {
        msg.body = redactSecrets(msg.body);
        history.push_back(msg);
        while (history.size() > kMaxHistory)
            history.pop_front();
    }

    // For log lines: "192.168.1.20:53211" or "192.168.1.20:53211 (alice)".
    QString label() const
    {
        return verified ? ip + " (" + username + ")" : ip;
    }
};
