// ---------------------------------------------------------------------------
// GameState.cpp - see GameState.h for the rules.
// ---------------------------------------------------------------------------

#include "GameState.h"

#include <algorithm>

void GameState::updateHands(const std::array<std::vector<Card>, kSeats>& hands)
{
    m_hands = hands;
    for (auto& taken : m_tricksTaken)
        taken.clear();
    m_playedCards.clear();
    m_activePlayer = -1;
    m_trickNumber = 1;
    m_tricksPlayed = 0;
    m_heartsBroken = false;
}

void GameState::beginPlay()
{
    for (int seat = 0; seat < kSeats; ++seat)
        if (std::find(m_hands[seat].begin(), m_hands[seat].end(), kTwoOfClubs) != m_hands[seat].end())
            m_activePlayer = seat;
}

bool GameState::hasSuit(int seat, Suit suit) const
{
    return std::any_of(m_hands[seat].begin(), m_hands[seat].end(),
                       [suit](const Card& c) { return c.suit == suit; });
}

bool GameState::onlyPointCards(int seat) const
{
    return std::all_of(m_hands[seat].begin(), m_hands[seat].end(),
                       [](const Card& c) { return cardPoints(c) > 0; });
}

bool GameState::onlyHearts(int seat) const
{
    return std::all_of(m_hands[seat].begin(), m_hands[seat].end(),
                       [](const Card& c) { return c.suit == Suit::Hearts; });
}

GameState::PlayResult GameState::validateCard(int seat, const Card& card) const
{
    // Rule 1: your turn, your card.
    if (seat != m_activePlayer)
        return PlayResult::NotYourTurn;
    const std::vector<Card>& hand = m_hands[seat];
    if (std::find(hand.begin(), hand.end(), card) == hand.end())
        return PlayResult::NotInHand;

    const bool firstTrick = (m_tricksPlayed == 0);

    if (isLeading()) {
        // Rule 2: the 2 of Clubs opens the round.
        if (firstTrick && !(card == kTwoOfClubs))
            return PlayResult::MustLeadTwoOfClubs;
        // Rule 5: no leading hearts until they're broken (or you have nothing else).
        if (card.suit == Suit::Hearts && !m_heartsBroken && !onlyHearts(seat))
            return PlayResult::HeartsNotBroken;
        return PlayResult::Ok;
    }

    // Rule 3: follow suit if you can.
    if (card.suit != trickSuit() && hasSuit(seat, trickSuit()))
        return PlayResult::MustFollowSuit;

    // Rule 4: no points on the first trick, unless you have nothing else.
    if (firstTrick && cardPoints(card) > 0 && !onlyPointCards(seat))
        return PlayResult::NoPointsFirstTrick;

    return PlayResult::Ok;
}

std::vector<Card> GameState::legalCards(int seat) const
{
    std::vector<Card> legal;
    for (const Card& c : m_hands[seat])
        if (validateCard(seat, c) == PlayResult::Ok)
            legal.push_back(c);
    return legal;
}

GameState::PlayResult GameState::placeCard(int seat, const Card& card)
{
    const PlayResult result = validateCard(seat, card);
    if (result != PlayResult::Ok)
        return result;

    std::vector<Card>& hand = m_hands[seat];
    hand.erase(std::find(hand.begin(), hand.end(), card));
    m_playedCards.push_back({seat, card});

    // Any heart on the table breaks hearts for the rest of the round.
    if (card.suit == Suit::Hearts)
        m_heartsBroken = true;

    // Next player clockwise, unless this was the 4th card (then the winner
    // goes next, which finishTrick() decides).
    if (!trickComplete())
        m_activePlayer = (seat + 1) % kSeats;
    return PlayResult::Ok;
}

GameState::TrickResult GameState::finishTrick()
{
    TrickResult result;
    const Suit led = trickSuit();

    // Highest card of the LED suit wins; other suits can never win.
    const PlayedCard* best = &m_playedCards.front();
    for (const PlayedCard& p : m_playedCards) {
        if (p.card.suit == led && p.card.rank > best->card.rank)
            best = &p;
        result.cards.push_back(p.card);
        result.points += cardPoints(p.card);
    }
    result.winnerSeat = best->seat;

    // The winner takes the cards and leads next.
    std::vector<Card>& taken = m_tricksTaken[result.winnerSeat];
    taken.insert(taken.end(), result.cards.begin(), result.cards.end());
    m_playedCards.clear();
    m_activePlayer = result.winnerSeat;
    ++m_tricksPlayed;
    if (m_tricksPlayed < kTricksPerRound)
        ++m_trickNumber;
    return result;
}

int GameState::pointsTaken(int seat) const
{
    int points = 0;
    for (const Card& c : m_tricksTaken[seat])
        points += cardPoints(c);
    return points;
}
