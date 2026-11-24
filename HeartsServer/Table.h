#pragma once
// ---------------------------------------------------------------------------
// Table.h - the game room for 4 players ("Table" in the class diagram).
// Team notes: "Table = game room with 4 players actively playing."
//
// Issue #71 (use case 16, Setup Table): the constructor seats the four
//   players (in lobby order: seat 0 = host = playerNum 1) and sets every
//   score to 0.
// Issue #70 (use case 17, Setup a Round): startNewRound() shuffles and deals
//   13 cards each, then the 3-card pass happens in this round's direction,
//   then the holder of the 2 of Clubs leads the first trick.
//
// PASS DIRECTION rotates every round (standard Hearts, "left right across
// hold - 4 cycles" in the team notes):
//   round 1: Left    -> you pass to the next seat      (seat + 1)
//   round 2: Right   -> you pass to the previous seat  (seat + 3)
//   round 3: Across  -> you pass to the opposite seat  (seat + 2)
//   round 4: Hold    -> no passing; play starts right away
//   round 5: Left again, and so on.
// (Seats go around the table in order 0 -> 1 -> 2 -> 3 -> 0, so "left" is
//  the next seat number.)
//
// Like GameManager, the Table never sends network messages. The Server asks
// it what changed and tells the players.
// ---------------------------------------------------------------------------

#include "Card.h"
#include "Lobby.h"

#include <QString>
#include <array>
#include <vector>

class Table
{
public:
    static constexpr int kSeats = 4;
    static constexpr int kCardsToPass = 3;

    enum class PassDirection { Left, Right, Across, Hold };
    enum class Phase { Passing, Playing };

    // Why submitPass() refused. Each maps to a protocol error code.
    enum class PassResult { Ok, NotPassingPhase, AlreadyPassed, WrongCount, NotInHand, Duplicate };

    // One seat at the table. Copied from the lobby when the game starts, so
    // seat numbers never shift during a game even if someone leaves the lobby.
    struct Seat
    {
        int     playerId = -1;
        QString username;
    };

    // Use case 16: seat the players in lobby order and set all scores to 0.
    explicit Table(const std::vector<LobbyPlayer>& players);

    // ---- seats and scores (#71) ----
    const Seat& seat(int seatIndex) const { return m_seats[seatIndex]; }
    int seatOf(int playerId) const;                    // -1 if not at this table
    const std::array<int, kSeats>& scores() const { return m_scores; }

    // ---- rounds and passing (#70) ----

    // Use case 17: next round number, shuffle, deal 13 each, pick the pass
    // direction. Phase becomes Passing (or Playing if this round is Hold).
    void startNewRound();

    int           roundNumber() const { return m_round; }
    PassDirection passDirection() const { return m_direction; }
    Phase         phase() const { return m_phase; }
    const std::vector<Card>& hand(int seatIndex) const { return m_hands[seatIndex]; }

    // Which seat `seatIndex` passes to / receives from this round.
    int passTarget(int seatIndex) const;
    int passSource(int seatIndex) const;

    // A player picks their 3 cards. They aren't moved until everyone has
    // picked (so nobody sees cards early); then call applyPasses().
    PassResult submitPass(int seatIndex, const std::vector<Card>& cards);
    bool hasPassed(int seatIndex) const { return !m_chosenPass[seatIndex].empty(); }
    bool allPassed() const;

    // Moves every player's 3 chosen cards to their target, re-sorts the hands,
    // and switches to Playing. receivedCards() then says what each seat got.
    void applyPasses();
    const std::vector<Card>& receivedCards(int seatIndex) const { return m_received[seatIndex]; }

    // Seat holding the 2 of Clubs: they lead the first trick (use case 17 step 3).
    int leadSeat() const;

    // "left" / "right" / "across" / "hold", as sent to clients.
    static QString directionName(PassDirection direction);

private:
    std::array<Seat, kSeats>              m_seats;
    std::array<int, kSeats>               m_scores{};   // all 0 (use case 16)
    std::array<std::vector<Card>, kSeats> m_hands;
    std::array<std::vector<Card>, kSeats> m_chosenPass; // cards picked this round, not moved yet
    std::array<std::vector<Card>, kSeats> m_received;   // cards each seat got in the pass
    int           m_round = 0;
    PassDirection m_direction = PassDirection::Left;
    Phase         m_phase = Phase::Passing;
};
