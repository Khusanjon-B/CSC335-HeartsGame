#pragma once
// ---------------------------------------------------------------------------
// GameManager.h - one game session ("Game Manager" in the class diagram,
// use case 11 / issue #64).
//
// The Server creates one GameManager per game (team notes 10/6: "Game
// manager created per game by the server"). It has:
//   - a unique lobbyId
//   - a Lobby (who's in it, host first)
//   - a state: WaitingForPlayers -> InGame
//   - a Table with the seats, cards and scores, created when the game
//     starts (issues #71, #70)
//
//   start_lobby --> [WaitingForPlayers] --(4 players + host sends start_game)--> [InGame]
//                         |    ^
//          join / leave --+----+     host leaves (either state) --> disbanded
//
// The GameManager only knows game rules and state. It never sends network
// messages; the Server does that after asking the GameManager what changed.
// That keeps this class (and Table) easy to test on their own.
// ---------------------------------------------------------------------------

#include "Lobby.h"
#include "Table.h"

#include <QJsonObject>
#include <QString>
#include <memory>

class GameManager
{
public:
    enum class State {
        WaitingForPlayers,   // in the lobby, people can join
        InGame               // started; the lobby is closed to new players
    };

    // Why addPlayer() refused someone. Each maps to a protocol error code.
    enum class JoinResult { Ok, Full, AlreadyStarted, AlreadyInLobby };

    // Why startGame() refused. Each maps to a protocol error code.
    enum class StartResult { Ok, NotHost, NotFull, AlreadyStarted };

    // Creates a new session with `host` in seat 1 ("GameManager() // Also
    // adds player as host" in the class diagram).
    GameManager(int lobbyId, const LobbyPlayer& host);

    int          lobbyId() const { return m_lobbyId; }
    State        state() const { return m_state; }
    const Lobby& lobby() const { return m_lobby; }

    // "addPlayerToLobby()" in the class diagram.
    JoinResult addPlayerToLobby(const LobbyPlayer& player);

    // Removes a player. Returns true if they were the HOST, which means the
    // caller must disband the whole session (use case 10).
    bool removePlayer(int playerId);

    // Host asks to start. Needs a full lobby (4 players) and the WaitingForPlayers state.
    // On success the Table is created (use case 16) and round 1 is dealt (use case 17).
    StartResult startGame(int requestingPlayerId);

    // The table, once the game has started; nullptr while still in the lobby.
    Table*       table() { return m_table.get(); }
    const Table* table() const { return m_table.get(); }

    // Short description for get_lobbies and the monitor window:
    // {"lobbyId":1,"host":"alice","playerCount":3}
    QJsonObject summary() const;

    // "Waiting for players" / "In game - round 1, passing left", for the monitor window.
    QString stateText() const;

private:
    int   m_lobbyId;
    Lobby m_lobby;
    State m_state = State::WaitingForPlayers;
    std::unique_ptr<Table> m_table;   // created by startGame()
};
