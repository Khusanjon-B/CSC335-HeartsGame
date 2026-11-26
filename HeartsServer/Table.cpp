// ---------------------------------------------------------------------------
// Table.cpp - see Table.h.
// ---------------------------------------------------------------------------

#include "Table.h"
#include "Deck.h"

#include <algorithm>

Table::Table(const std::vector<LobbyPlayer>& players)
{
    // Seat i = lobby position i (seat 0 is the host). The lobby is full
    // (4 players) before a game can start, so all 4 seats get filled.
    for (int i = 0; i < kSeats && i < static_cast<int>(players.size()); ++i)
        m_seats[i] = {players[i].playerId, players[i].username};
    // m_scores is already {0, 0, 0, 0} from its {} initializer.
}

int Table::seatOf(int playerId) const
{
    for (int i = 0; i < kSeats; ++i)
        if (m_seats[i].playerId == playerId)
            return i;
    return -1;
}

void Table::startNewRound()
{
    ++m_round;

    // Rotate left, right, across, hold, left, ...
    static constexpr PassDirection kCycle[] = {PassDirection::Left, PassDirection::Right,
                                               PassDirection::Across, PassDirection::Hold};
    m_direction = kCycle[(m_round - 1) % 4];

    Deck deck;          // newDeck()
    deck.shuffle();
    m_gameState.updateHands(deck.dealCards());   // also clears last round's tricks

    for (int i = 0; i < kSeats; ++i) {
        m_chosenPass[i].clear();
        m_received[i].clear();
    }

    // Hold rounds skip passing entirely, so play starts now.
    if (m_direction == PassDirection::Hold) {
        m_phase = Phase::Playing;
        m_gameState.beginPlay();
    } else {
        m_phase = Phase::Passing;
    }
}

int Table::passTarget(int seatIndex) const
{
    switch (m_direction) {
    case PassDirection::Left:   return (seatIndex + 1) % kSeats;
    case PassDirection::Right:  return (seatIndex + 3) % kSeats;
    case PassDirection::Across: return (seatIndex + 2) % kSeats;
    case PassDirection::Hold:   break;
    }
    return seatIndex;   // hold: nobody passes
}

int Table::passSource(int seatIndex) const
{
    // Whoever's target is me.
    for (int i = 0; i < kSeats; ++i)
        if (passTarget(i) == seatIndex && i != seatIndex)
            return i;
    return seatIndex;
}

Table::PassResult Table::submitPass(int seatIndex, const std::vector<Card>& cards)
{
    if (m_phase != Phase::Passing)
        return PassResult::NotPassingPhase;
    if (hasPassed(seatIndex))
        return PassResult::AlreadyPassed;
    if (static_cast<int>(cards.size()) != kCardsToPass)
        return PassResult::WrongCount;

    const std::vector<Card>& hand = m_gameState.hand(seatIndex);
    for (std::size_t i = 0; i < cards.size(); ++i) {
        if (std::find(hand.begin(), hand.end(), cards[i]) == hand.end())
            return PassResult::NotInHand;
        // The same card twice, e.g. ["QS","QS","2C"].
        if (std::find(cards.begin() + i + 1, cards.end(), cards[i]) != cards.end())
            return PassResult::Duplicate;
    }

    m_chosenPass[seatIndex] = cards;
    return PassResult::Ok;
}

bool Table::allPassed() const
{
    for (int i = 0; i < kSeats; ++i)
        if (!hasPassed(i))
            return false;
    return true;
}

void Table::applyPasses()
{
    std::array<std::vector<Card>, kSeats>& hands = m_gameState.playerHands();

    // 1. Take every chosen card out of its owner's hand...
    for (int i = 0; i < kSeats; ++i)
        for (const Card& c : m_chosenPass[i])
            hands[i].erase(std::find(hands[i].begin(), hands[i].end(), c));

    // 2. ...then give each set to its target, so nobody gets back what they
    //    just passed even in odd orderings.
    for (int i = 0; i < kSeats; ++i) {
        const int to = passTarget(i);
        m_received[to] = m_chosenPass[i];
        hands[to].insert(hands[to].end(), m_chosenPass[i].begin(), m_chosenPass[i].end());
    }

    for (auto& hand : hands)
        std::sort(hand.begin(), hand.end());
    m_phase = Phase::Playing;
    m_gameState.beginPlay();   // the 2 of Clubs holder is now the active player
}

int Table::leadSeat() const
{
    for (int i = 0; i < kSeats; ++i) {
        const std::vector<Card>& hand = m_gameState.hand(i);
        if (std::find(hand.begin(), hand.end(), kTwoOfClubs) != hand.end())
            return i;
    }
    return 0;   // can't happen with a full deck
}

GameState::PlayResult Table::playCard(int seatIndex, const Card& card)
{
    if (m_phase != Phase::Playing || m_gameState.roundOver())
        return GameState::PlayResult::NotYourTurn;
    return m_gameState.placeCard(seatIndex, card);
}

GameState::TrickResult Table::determineTrickWinner()
{
    return m_gameState.finishTrick();
}

QString Table::directionName(PassDirection direction)
{
    switch (direction) {
    case PassDirection::Left:   return "left";
    case PassDirection::Right:  return "right";
    case PassDirection::Across: return "across";
    case PassDirection::Hold:   return "hold";
    }
    return "hold";
}
