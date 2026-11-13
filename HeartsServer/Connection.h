#pragma once
// ---------------------------------------------------------------------------
// Connection.h - everything the server knows about ONE connected client.
//
// The Server keeps a std::vector<Connection> called m_connections.
//   - A Connection is added the moment a client opens a TCP connection.
//   - It starts out "anonymous": no playerId, no username, verified = false.
//   - A successful "login" command fills in playerId/username/verified.
//   - A "logout" command clears them again (the socket stays open).
//   - When the socket disconnects, the Connection is removed from the vector.
//
// To track something new per client (current game, seat number, etc.),
// add a field here; nothing else has to change.
// ---------------------------------------------------------------------------

#include <QString>
#include <QTcpSocket>
#include <optional>

struct Connection
{
    // The Qt socket used to talk to this client. The Server owns the socket's
    // lifetime; this is just a handle to it.
    QTcpSocket* socket = nullptr;

    // "ip:port" of the client, e.g. "192.168.1.20:53211". Used in log lines.
    QString ip;

    // The players.player_id from the database that this connection is logged
    // in as. std::optional means it can be "empty" (the C++ version of null):
    //   if (conn.playerId)       -> logged in, value is *conn.playerId
    //   if (!conn.playerId)      -> nobody logged in yet
    std::optional<int> playerId;

    // Username of the logged-in player (empty until login succeeds).
    QString username;

    // True once the client has logged in with a correct username + password.
    // Use this to block commands that need a logged-in player.
    bool verified = false;
};
