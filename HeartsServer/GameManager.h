#pragma once
// ---------------------------------------------------------------------------
// GameManager.h - one game session ("Game Manager" in the class diagram,
// use case 11 / issue #64).
//
// The Server creates one GameManager per game (team notes 10/6: "Game
// manager created per game by the server"). It has:
//   - a unique lobbyId
//   - a Lobby (who's in it, host first)
//   - a state: WaitingForPlayers -> InGame -> Finished
//   - a Table with the seats, cards and scores, created when the game
//     starts (issues #71, #70)
//
//   start_lobby --> [WaitingForPlayers] --(4 players + host sends start_game)--> [InGame] <-----+
//                         |    ^                                                    |          |
//          join / leave --+----+                     someone reaches 100 (#69) --> [Finished]  |
//                                                     all 4 vote play_again (#67) -------------+
//   host leaves (any state) --> disbanded
//   anyone leaves InGame/Finished on purpose --> disbanded (a 3-player Hearts game can't go on)
//
// PAUSED (issue #88, test case 19.2): while InGame, a non-host player whose
// connection drops is marked "disconnected" but keeps their seat. The game
// is paused until they log back in (or the host disbands it).
//
// The GameManager only knows game rules and state. It never sends network
// messages; the Server does that after asking the GameManager what changed.
// That keeps this class (and Table) easy to test on their own.
// ---------------------------------------------------------------------------

#include "Lobby.h"
#include "Table.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <set>

class GameManager
{
public:
    enum class State {
        WaitingForPlayers,   // in the lobby, people can join
        InGame,              // started; the lobby is closed to new players
        Finished             // someone reached the game-over score (use case 20)
    };

    // Why addPlayer() refused someone. Each maps to a protocol error code.
    enum class JoinResult { Ok, Full, AlreadyStarted, AlreadyInLobby };

    // Why startGame() refused. Each maps to a protocol error code.
    enum class StartResult { Ok, NotHost, NotFull, AlreadyStarted };

    // Creates a new session with `host` in seat 1 ("GameManager() // Also
    // adds player as host" in the class diagram). `gameOverScore` is passed
    // on to the Table (server.ini, default 100).
    GameManager(int lobbyId, const LobbyPlayer& host, int gameOverScore);

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

    // The game is over (use case 20): InGame -> Finished. The Table is kept
    // so the final scores can still be read.
    void endGame() { m_state = State::Finished; }

    // ---- disconnects (issue #88) ----

    // A seated player's connection dropped mid-game: pause the game.
    void markDisconnected(int playerId) { m_disconnected.insert(playerId); }
    // They logged back in.
    void markReconnected(int playerId) { m_disconnected.erase(playerId); }
    bool isDisconnected(int playerId) const { return m_disconnected.count(playerId) > 0; }
    // Someone is missing, so nobody can play.
    bool isPaused() const { return !m_disconnected.empty(); }
    // Usernames of the missing players, in seat order.
    QStringList disconnectedNames() const;

    // ---- play again (issue #67, use case 21) ----

    // Why voteToPlayAgain() refused. Each maps to a protocol error code.
    enum class VoteResult { Ok, GameNotOver, AlreadyVoted };

    // One player votes yes. Only after the game has Finished.
    VoteResult voteToPlayAgain(int playerId);
    QStringList playAgainVoters() const;   // who has voted yes, in seat order
    bool allVotedToPlayAgain() const { return m_playAgainVotes.size() == Lobby::kMaxPlayers; }

    // All 4 voted yes: a brand-new Table with the same players, scores back
    // to 0, round 1 dealt (sequence diagram "playAgain()"). Back to InGame.
    void playAgain();

    // The table, once the game has started; nullptr while still in the lobby.
    Table*       table() { return m_table.get(); }
    const Table* table() const { return m_table.get(); }

    // Short description for get_lobbies and the monitor window:
    // {"lobbyId":1,"host":"alice","playerCount":3}
    QJsonObject summary() const;

    // "Waiting for players" / "In game - round 1, passing left" /
    // "In game - round 1, trick 5" / "Paused - waiting for bob" /
    // "Game over after 6 round(s), 2/4 voted to play again", for the monitor window.
    QString stateText() const;

private:
    int   m_lobbyId;
    int   m_gameOverScore;
    Lobby m_lobby;
    State m_state = State::WaitingForPlayers;
    std::unique_ptr<Table> m_table;   // created by startGame()
    std::set<int> m_disconnected;     // playerIds that dropped mid-game
    std::set<int> m_playAgainVotes;   // playerIds that voted yes
};
