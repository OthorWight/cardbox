#pragma once

#include <vector>
#include <string>
#include "imgui.h"
#include <sol/sol.hpp>

enum class Suit { Hearts, Diamonds, Clubs, Spades };
enum class Rank { Ace = 1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King };
enum class PileType { Stock, Waste, Tableau, Foundation, FreeCellSlot };

struct Card {
    Rank rank;
    Suit suit;
    bool faceUp = false;

    // Animation state
    ImVec2 animPos = ImVec2(0, 0);
    bool hasInitializedPos = false;
    float flipVisual = -1.0f; // -1.0 is face down, 1.0 is face up

    // Helper functions
    bool IsRed() const { return suit == Suit::Hearts || suit == Suit::Diamonds; }
    int Color() const { return IsRed() ? 1 : 0; }
};

struct Pile {
    std::vector<Card> cards;
    ImVec2 pos;
    ImVec2 size;
    ImVec2 offset; // Offset between consecutive cards in the pile
    PileType type;
    int id; // For identification
};

struct BouncingCard {
    Card card;
    ImVec2 pos;
    ImVec2 velocity;
};

class Game {
public:
    Game();
    void InitGame(const std::string& scriptPath);
    void UpdateAndDraw();
    bool IsWon() const;

private:
    sol::state m_lua;
    std::vector<Pile> m_piles;
    std::vector<std::string> m_availableGames;
    std::string m_currentGameName;
    std::string m_currentHelpText;
    std::string m_currentScriptPath;
    
    // Win state
    bool m_isWon = false;
    std::vector<BouncingCard> m_bouncingCards;
    float m_winAnimTimer = 0.0f;
    void UpdateWinAnimation(ImDrawList* drawList, float scale);

    // Dragging state
    int m_dragSourcePile = -1;
    int m_dragCardIndex = -1;
    std::vector<Card> m_dragCards;
    ImVec2 m_dragOffset;

    // Undo stack
    std::vector<std::vector<Pile>> m_undoStack;
    void SaveStateForUndo();
    void Undo();

    // Core game methods
    void SetupLuaBindings();
    void LoadAvailableGames();
    void CreateDeck(std::vector<Card>& deck, int numDecks = 1);
    void ShuffleDeck(std::vector<Card>& deck);
    
    bool CanPickup(int pileIdx, int cardIdx);
    bool CanDrop(int sourcePileIdx, const std::vector<Card>& cards, int targetPileIdx);
    void DoMove(int sourcePileIdx, int targetPileIdx, int cardIdx);
    void HandleClick(int pileIdx); // For things like stock pile clicking

    // Rendering
    void DrawCard(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, const Card& card, float scale, float widthScale = 1.0f, bool isDragged = false);
    void DrawCardBack(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, float widthScale = 1.0f, bool isDragged = false);
    void DrawEmptyPile(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, PileType type);
    void DrawSuit(ImDrawList* drawList, const ImVec2& center, float size, Suit suit, ImU32 color);
};
