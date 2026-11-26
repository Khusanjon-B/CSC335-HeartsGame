// ---------------------------------------------------------------------------
// GameManager.cpp - see GameManager.h.
// ---------------------------------------------------------------------------

#include "GameManager.h"

GameManager::GameManager(int lobbyId, const LobbyPlayer& host, int gameOverScore)
    : m_lobbyId(lobbyId)
    , m_gameOverScore(gameOverScore)
    , m_lobby(host)   // the host takes seat 1
{
}

GameManager::JoinResult GameManager::addPlayerToLobby(const LobbyPlayer& player)
{
    // Team notes 10/1: "once the game starts, the lobby closes."
    if (m_state != State::WaitingForPlayers)
        return JoinResult::AlreadyStarted;
    if (m_lobby.hasPlayer(player.playerId))
        return JoinResult::AlreadyInLobby;
    if (!m_lobby.addPlayer(player))
        return JoinResult::Full;
    return JoinResult::Ok;
}

bool GameManager::removePlayer(int playerId)
{
    const bool wasHost = m_lobby.isHost(playerId);
    m_lobby.removePlayer(playerId);
    return wasHost;
}

GameManager::StartResult GameManager::startGame(int requestingPlayerId)
{
    if (m_state != State::WaitingForPlayers)
        return StartResult::AlreadyStarted;
    // Team notes 9/29: the host is the one who starts the game.
    if (!m_lobby.isHost(requestingPlayerId))
        return StartResult::NotHost;
    // Use case 8: "Not enough players in lobby to start the game".
    if (!m_lobby.isFull())
        return StartResult::NotFull;

    m_state = State::InGame;
    // Use case 16: seat the four players (lobby order) with scores 0.
    m_table = std::make_unique<Table>(m_lobby.players(), m_gameOverScore);
    // Use case 17: shuffle and deal round 1.
    m_table->startNewRound();
    return StartResult::Ok;
}

QStringList GameManager::disconnectedNames() const
{
    QStringList names;
    for (const LobbyPlayer& p : m_lobby.players())
        if (isDisconnected(p.playerId))
            names << p.username;
    return names;
}

GameManager::VoteResult GameManager::voteToPlayAgain(int playerId)
{
    if (m_state != State::Finished)
        return VoteResult::GameNotOver;
    if (!m_playAgainVotes.insert(playerId).second)   // insert() says false if already there
        return VoteResult::AlreadyVoted;
    return VoteResult::Ok;
}

QStringList GameManager::playAgainVoters() const
{
    QStringList names;
    for (const LobbyPlayer& p : m_lobby.players())
        if (m_playAgainVotes.count(p.playerId))
            names << p.username;
    return names;
}

void GameManager::playAgain()
{
    // Use case 21 step 2 / test case 22: same players, same seats, scores
    // back to 0 (use case 16 again), then round 1 (use case 17).
    m_playAgainVotes.clear();
    m_table = std::make_unique<Table>(m_lobby.players(), m_gameOverScore);
    m_table->startNewRound();
    m_state = State::InGame;
}

QJsonObject GameManager::summary() const
{
    return {
        {"lobbyId",     m_lobbyId},
        {"host",        m_lobby.host().username},
        {"playerCount", m_lobby.playerCount()},
    };
}

QString GameManager::stateText() const
{
    if (m_state == State::WaitingForPlayers || !m_table)
        return "Waiting for players";
    if (m_state == State::Finished)
        return QString("Game over after %1 round(s), %2/%3 voted to play again")
            .arg(m_table->roundNumber()).arg(m_playAgainVotes.size()).arg(Lobby::kMaxPlayers);
    if (isPaused())
        return "Paused - waiting for " + disconnectedNames().join(", ");
    const QString phase = m_table->phase() == Table::Phase::Passing
                              ? "passing " + Table::directionName(m_table->passDirection())
                              : QString("trick %1").arg(m_table->gameState().trickNumber());
    return QString("In game - round %1, %2").arg(m_table->roundNumber()).arg(phase);
}
