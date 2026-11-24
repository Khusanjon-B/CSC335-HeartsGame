#pragma once
// ---------------------------------------------------------------------------
// Lobby.h - the waiting room for one game ("Lobby" in the class diagram).
//
// Team notes: "Lobby = waiting room until 4 players gathered to start."
// A Lobby is just the ordered list of players in it:
//
//   players[0] = the host (always), players[1..3] = everyone else in join order
//
// A player's "playerNum" is their position + 1, so the host is always
// playerNum 1 (matching the client's Player.isHost(): playerNum == 1).
// When someone leaves, the players after them move up one place, so there
// are never gaps. The server sends the updated list every time it changes.
//
// The Lobby only stores who's in it. Rules about WHO may join or start
// (full, already started, host only...) live in GameManager.
// ---------------------------------------------------------------------------

#include <QJsonArray>
#include <QString>
#include <algorithm>
#include <vector>

// One player sitting in a lobby.
struct LobbyPlayer
{
    int     playerId = -1;   // players.player_id in the database
    QString username;
};

class Lobby
{
public:
    static constexpr int kMaxPlayers = 4;   // Hearts is a 4-player game

    // A lobby always starts with its host in seat 1.
    explicit Lobby(const LobbyPlayer& host) { m_players.push_back(host); }

    const std::vector<LobbyPlayer>& players() const { return m_players; }
    const LobbyPlayer& host() const { return m_players.front(); }
    int  playerCount() const { return static_cast<int>(m_players.size()); }
    bool isFull() const { return playerCount() >= kMaxPlayers; }

    bool hasPlayer(int playerId) const { return indexOf(playerId) >= 0; }
    bool isHost(int playerId) const { return host().playerId == playerId; }

    // 1-based seat number (1 = host), or 0 if not in the lobby.
    int playerNum(int playerId) const { return indexOf(playerId) + 1; }

    // Adds a player to the next seat. Returns false if the lobby is full.
    bool addPlayer(const LobbyPlayer& player)
    {
        if (isFull())
            return false;
        m_players.push_back(player);
        return true;
    }

    // Removes a player; everyone after them moves up one seat.
    // (Removing the host is allowed, but GameManager disbands the lobby
    //  when that happens, so a lobby never continues without its host.)
    void removePlayer(int playerId)
    {
        m_players.erase(std::remove_if(m_players.begin(), m_players.end(),
                                       [playerId](const LobbyPlayer& p) { return p.playerId == playerId; }),
                        m_players.end());
    }

    // Usernames in seat order, e.g. ["Ryan", "Khusanjon", "Carlo"]. This is
    // the "players" array the protocol doc shows.
    QJsonArray playerNames() const
    {
        QJsonArray names;
        for (const LobbyPlayer& p : m_players)
            names.append(p.username);
        return names;
    }

private:
    // Position in m_players, or -1 if not found.
    int indexOf(int playerId) const
    {
        for (int i = 0; i < playerCount(); ++i)
            if (m_players[i].playerId == playerId)
                return i;
        return -1;
    }

    std::vector<LobbyPlayer> m_players;   // [0] = host
};
