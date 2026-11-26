// ---------------------------------------------------------------------------
// GameManager.cpp - see GameManager.h.
// ---------------------------------------------------------------------------

#include "GameManager.h"

GameManager::GameManager(int lobbyId, const LobbyPlayer& host)
    : m_lobbyId(lobbyId)
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
    m_table = std::make_unique<Table>(m_lobby.players());
    // Use case 17: shuffle and deal round 1.
    m_table->startNewRound();
    return StartResult::Ok;
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
    const QString phase = m_table->phase() == Table::Phase::Passing
                              ? "passing " + Table::directionName(m_table->passDirection())
                              : QString("trick %1").arg(m_table->gameState().trickNumber());
    return QString("In game - round %1, %2").arg(m_table->roundNumber()).arg(phase);
}
