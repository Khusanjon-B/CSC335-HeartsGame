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
// Issue #66 (use case 18, Adjudicate a Trick): playCard() and
//   determineTrickWinner(). The cards in play live in the Table's GameState
//   (class diagram: "Table 1 -- 1 GameState"); the rules are in GameState.h.
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
#include "GameState.h"
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
    // `gameOverScore` is the 100-point limit (gameOverScore in server.ini).
    Table(const std::vector<LobbyPlayer>& players, int gameOverScore);

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
    const std::vector<Card>& hand(int seatIndex) const { return m_gameState.hand(seatIndex); }

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
    // ("getLeadPlayer()" in the class diagram.)
    int leadSeat() const;

    // ---- playing tricks (#66) ----

    // The cards in play: hands, the current trick, whose turn, tricks taken.
    const GameState& gameState() const { return m_gameState; }

    // Plays one card for `seatIndex` if the rules allow it (see GameState.h).
    // Only during the Playing phase; otherwise NotYourTurn.
    GameState::PlayResult playCard(int seatIndex, const Card& card);

    // After the 4th card of a trick: who won it, which cards, how many
    // points. The winner leads next.
    GameState::TrickResult determineTrickWinner();

    // All 13 tricks of this round are done.
    bool roundOver() const { return m_phase == Phase::Playing && m_gameState.roundOver(); }

    // ---- scoring (#69, use case 19) ----

    static constexpr int kMoonPoints = 26;   // all 13 hearts + the Queen of Spades

    // What updateScore() did this round.
    struct RoundScore
    {
        std::array<int, kSeats> points{};   // added to each seat's score this round
        int moonShooter = -1;               // seat that shot the moon, or -1
    };

    // Call when roundOver(). Adds each seat's points for the round to its
    // score ("updateScore()" in the class diagram):
    //   - every heart taken = 1, the Queen of Spades = 13
    //   - SHOOT THE MOON: a player who took all 26 points scores 0 and
    //     everyone else gets 26 instead.
    RoundScore updateScore();

    // Someone has reached the game-over score (use case 19 step 3).
    bool gameOver() const;
    int  gameOverScore() const { return m_gameOverScore; }

    // Final places, by seat: lowest score = place 1. Tied scores share a
    // place (e.g. 1, 1, 3, 4), so a tie for lowest means two winners.
    std::array<int, kSeats> places() const;

    // "left" / "right" / "across" / "hold", as sent to clients.
    static QString directionName(PassDirection direction);

private:
    std::array<Seat, kSeats>              m_seats;
    std::array<int, kSeats>               m_scores{};   // all 0 (use case 16)
    GameState                             m_gameState;  // hands + tricks for this round
    std::array<std::vector<Card>, kSeats> m_chosenPass; // cards picked this round, not moved yet
    std::array<std::vector<Card>, kSeats> m_received;   // cards each seat got in the pass
    int           m_gameOverScore = 100;
    int           m_round = 0;
    PassDirection m_direction = PassDirection::Left;
    Phase         m_phase = Phase::Passing;
};
