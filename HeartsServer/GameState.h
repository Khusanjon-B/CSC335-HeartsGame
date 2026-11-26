#pragma once
// ---------------------------------------------------------------------------
// GameState.h - the cards in play during one round ("GameState" in the class
// diagram: playerHands[][], playerTricksTaken[][], activePlayer, trickTurn,
// trickSuit, playedCards[], placeCard(), getPlayedCards(), ...).
//
// Use case 18 (Adjudicate a Trick, issue #66). The Table owns one GameState
// and resets it every round.
//
// HEARTS RULES CHECKED HERE (validateCard):
//   1. Only the active player can play, and only a card in their hand.
//   2. The first trick of a round must be led with the 2 of Clubs.
//   3. Follow suit: if you have a card of the led suit, you must play one.
//   4. First trick: no points. If you can't follow suit you still may not
//      play a heart or the Queen of Spades, unless that's all you have.
//   5. Hearts can't be LED until hearts are "broken" (someone has played a
//      heart in an earlier trick), unless your hand is only hearts.
//
// A trick is over when 4 cards are down. finishTrick() then gives the cards
// to whoever played the highest card of the led suit, and that player leads
// the next trick. After 13 tricks the round is over.
//
// Seats are 0..3, the same as Table seats.
// ---------------------------------------------------------------------------

#include "Card.h"

#include <array>
#include <vector>

class GameState
{
public:
    static constexpr int kSeats = 4;
    static constexpr int kTricksPerRound = 13;

    // Why validateCard()/placeCard() refused a card. Each maps to an error code.
    enum class PlayResult {
        Ok,
        NotYourTurn,          // someone else is the active player
        NotInHand,            // you don't have that card
        MustLeadTwoOfClubs,   // rule 2
        MustFollowSuit,       // rule 3
        NoPointsFirstTrick,   // rule 4
        HeartsNotBroken       // rule 5
    };

    // One card on the table in the current trick.
    struct PlayedCard
    {
        int  seat = 0;
        Card card;
    };

    // What finishTrick() returns.
    struct TrickResult
    {
        int               winnerSeat = 0;
        std::vector<Card> cards;        // the 4 cards, in play order
        int               points = 0;   // hearts + 13 for the Queen of Spades
    };

    // New round: give everyone their dealt hand and clear the tricks
    // ("updateHands()" in the class diagram). Nobody is active until beginPlay().
    void updateHands(const std::array<std::vector<Card>, kSeats>& hands);

    // Hands can still change before play starts (the 3-card pass).
    std::array<std::vector<Card>, kSeats>& playerHands() { return m_hands; }
    const std::vector<Card>& hand(int seat) const { return m_hands[seat]; }

    // Passing is done: the 2 of Clubs holder becomes the active player and
    // trick 1 begins.
    void beginPlay();

    // ---- turns ----
    int  getActivePlayer() const { return m_activePlayer; }
    void setActivePlayer(int seat) { m_activePlayer = seat; }
    int  trickNumber() const { return m_trickNumber; }          // 1..13
    bool heartsBroken() const { return m_heartsBroken; }
    bool isLeading() const { return m_playedCards.empty(); }   // first card of the trick?

    // ---- playing ----
    // Checks the rules above without changing anything.
    PlayResult validateCard(int seat, const Card& card) const;

    // Every card `seat` could legally play right now (empty if not their turn).
    std::vector<Card> legalCards(int seat) const;

    // validateCard(), and if Ok: takes the card out of the hand, puts it on
    // the table, breaks hearts if it's a heart, and moves the turn to the
    // next seat (unless the trick is now complete).
    PlayResult placeCard(int seat, const Card& card);

    const std::vector<PlayedCard>& getPlayedCards() const { return m_playedCards; }
    bool trickComplete() const { return static_cast<int>(m_playedCards.size()) == kSeats; }

    // Call when trickComplete(). Highest card of the led suit wins; the
    // winner takes the cards, becomes the active player, and the trick
    // number goes up.
    TrickResult finishTrick();

    // All 13 tricks have been played.
    bool roundOver() const { return m_tricksPlayed == kTricksPerRound; }

    // Cards each seat has won this round ("playerTricksTaken[][]").
    const std::vector<Card>& tricksTaken(int seat) const { return m_tricksTaken[seat]; }

    // Points each seat took this round (before any Shoot the Moon adjustment).
    int pointsTaken(int seat) const;

private:
    // The suit that was led in the current trick ("trickSuit").
    Suit trickSuit() const { return m_playedCards.front().card.suit; }
    bool hasSuit(int seat, Suit suit) const;
    bool onlyPointCards(int seat) const;   // hand is all hearts / Queen of Spades
    bool onlyHearts(int seat) const;

    std::array<std::vector<Card>, kSeats> m_hands;         // "playerHands[][]"
    std::array<std::vector<Card>, kSeats> m_tricksTaken;   // "playerTricksTaken[][]"
    std::vector<PlayedCard> m_playedCards;                 // current trick, in play order
    int  m_activePlayer = -1;   // whose turn; -1 = nobody (passing)
    int  m_trickNumber  = 1;    // which trick is being played
    int  m_tricksPlayed = 0;    // finished tricks this round
    bool m_heartsBroken = false;
};
