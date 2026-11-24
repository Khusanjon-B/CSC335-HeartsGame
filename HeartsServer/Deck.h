#pragma once
// ---------------------------------------------------------------------------
// Deck.h - a standard 52-card deck ("Deck" in the class diagram:
// newDeck(), shuffle(), dealCards()).
//
// The SERVER shuffles, not the client. The team notes debated who owns the
// random seed; the server is authoritative (notes 10/6), and a server-side
// shuffle can't be tampered with by a player.
// ---------------------------------------------------------------------------

#include "Card.h"

#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <vector>

class Deck
{
public:
    // A fresh, ordered deck: 2C..AC, 2D..AD, 2S..AS, 2H..AH.
    Deck() { newDeck(); }

    void newDeck()
    {
        m_cards.clear();
        for (Suit s : {Suit::Clubs, Suit::Diamonds, Suit::Spades, Suit::Hearts})
            for (int rank = 2; rank <= 14; ++rank)
                m_cards.push_back({s, rank});
    }

    // Random order, using the operating system's secure random source.
    void shuffle()
    {
        std::shuffle(m_cards.begin(), m_cards.end(), *QRandomGenerator::system());
    }

    // Deals the whole deck evenly: 13 cards to each of 4 hands, one at a
    // time around the table like a real dealer. Each hand comes back sorted.
    std::array<std::vector<Card>, 4> dealCards() const
    {
        std::array<std::vector<Card>, 4> hands;
        for (std::size_t i = 0; i < m_cards.size(); ++i)
            hands[i % 4].push_back(m_cards[i]);
        for (auto& hand : hands)
            std::sort(hand.begin(), hand.end());
        return hands;
    }

    int cardsLeft() const { return static_cast<int>(m_cards.size()); }

private:
    std::vector<Card> m_cards;
};
