#pragma once
// ---------------------------------------------------------------------------
// Card.h - one playing card ("Card" with the Suit and Value enumerations in
// the class diagram), plus how cards are written in JSON.
//
// CARD CODES (what clients send and receive):
//   rank + suit letter
//   ranks: 2 3 4 5 6 7 8 9 10 J Q K A
//   suits: C = Clubs, D = Diamonds, S = Spades, H = Hearts
//   e.g.  "2C" (two of clubs), "10H", "QS" (queen of spades), "AH"
//
// Rank is stored as a number 2..14 (J=11, Q=12, K=13, A=14), so comparing
// two cards of the same suit is just comparing numbers.
// ---------------------------------------------------------------------------

#include <QString>
#include <optional>

enum class Suit { Clubs, Diamonds, Spades, Hearts };   // also the sort order of a hand

struct Card
{
    Suit suit = Suit::Clubs;
    int  rank = 2;   // 2..10, J=11, Q=12, K=13, A=14

    bool operator==(const Card& other) const { return suit == other.suit && rank == other.rank; }

    // Sort order for a hand: by suit (C, D, S, H), then low to high.
    bool operator<(const Card& other) const
    {
        if (suit != other.suit)
            return static_cast<int>(suit) < static_cast<int>(other.suit);
        return rank < other.rank;
    }

    // "QS", "10H", ...
    QString code() const
    {
        static const char* rankNames[] = {"", "", "2", "3", "4", "5", "6", "7", "8",
                                          "9", "10", "J", "Q", "K", "A"};
        static const char suitLetters[] = {'C', 'D', 'S', 'H'};
        return QString(rankNames[rank]) + QChar(suitLetters[static_cast<int>(suit)]);
    }

    // Parses a card code. std::nullopt if it isn't a real card ("1X", "", ...).
    static std::optional<Card> fromCode(const QString& text)
    {
        const QString code = text.trimmed().toUpper();
        if (code.size() < 2 || code.size() > 3)
            return std::nullopt;

        Card c;
        switch (code.back().toLatin1()) {
        case 'C': c.suit = Suit::Clubs;    break;
        case 'D': c.suit = Suit::Diamonds; break;
        case 'S': c.suit = Suit::Spades;   break;
        case 'H': c.suit = Suit::Hearts;   break;
        default:  return std::nullopt;
        }

        const QString rank = code.left(code.size() - 1);
        if      (rank == "J") c.rank = 11;
        else if (rank == "Q") c.rank = 12;
        else if (rank == "K") c.rank = 13;
        else if (rank == "A") c.rank = 14;
        else {
            bool ok = false;
            c.rank = rank.toInt(&ok);
            if (!ok || c.rank < 2 || c.rank > 10)
                return std::nullopt;
        }
        return c;
    }
};

// The card that leads the first trick of every round (use case 17).
inline const Card kTwoOfClubs{Suit::Clubs, 2};

// The Queen of Spades: worth 13 points.
inline const Card kQueenOfSpades{Suit::Spades, 12};

// Penalty points for one card: every heart is 1, the Queen of Spades is 13,
// everything else 0. A round has 26 points in total.
inline int cardPoints(const Card& card)
{
    if (card.suit == Suit::Hearts)
        return 1;
    if (card == kQueenOfSpades)
        return 13;
    return 0;
}
