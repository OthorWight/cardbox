#pragma once

#include <vector>
#include <string>
#include "imgui.h"

#if defined(__GNUC__) && __GNUC__ >= 15
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtemplate-body"
#endif
#include <sol/sol.hpp>
#if defined(__GNUC__) && __GNUC__ >= 15
#pragma GCC diagnostic pop
#endif

enum class Suit { Hearts, Diamonds, Clubs, Spades };
enum class Rank { Ace = 1, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King };
enum class PileType { Stock, Waste, Tableau, Foundation, FreeCellSlot, Invisible };

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

    // Comparison operators for sol2 bindings
    bool operator==(const Card& other) const {
        return rank == other.rank && suit == other.suit && faceUp == other.faceUp;
    }
    bool operator<(const Card& other) const {
        if (suit != other.suit) return suit < other.suit;
        return rank < other.rank;
    }
};

struct Pile {
    std::vector<Card> cards;
    ImVec2 pos;
    ImVec2 size;
    ImVec2 offset; // Offset between consecutive cards in the pile
    PileType type;
    int id; // For identification

    // Comparison operators for sol2 bindings
    bool operator==(const Pile& other) const {
        return id == other.id;
    }
    bool operator<(const Pile& other) const {
        return id < other.id;
    }
};

struct Particle {
    ImVec2 pos;
    ImVec2 velocity;
    ImU32 color;
    float life;
    float size;
};

class Game {
public:
    Game();
    ~Game();
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
    float m_winAnimTimer = 0.0f;
    std::vector<Particle> m_particles;
    void UpdateWinAnimation(ImDrawList* drawList, float scale);

    // Dragging state
    int m_dragSourcePile = -1;
    int m_dragCardIndex = -1;
    std::vector<Card> m_dragCards;
    ImVec2 m_dragOffset;

    // Undo stack
    std::vector<std::vector<Pile>> m_undoStack;
    std::vector<std::vector<Pile>> m_redoStack;
    void SaveStateForUndo();
    void Undo();
    void Redo();

    // Core game methods
    void SetupLuaBindings();
    void LoadAvailableGames();
    void CreateDeck(std::vector<Card>& deck, int numDecks = 1);
    void ShuffleDeck(std::vector<Card>& deck);
    
    bool CanPickup(int pileIdx, int cardIdx);
    bool CanDrop(int sourcePileIdx, const std::vector<Card>& cards, int targetPileIdx);
    void DoMove(int sourcePileIdx, int targetPileIdx, int cardIdx);
    void HandleClick(int pileIdx); // For things like stock pile clicking
    
    // Refactored UpdateAndDraw helpers
    void RenderMenuBar();
    void RenderStartScreen(ImDrawList* drawList, float scale);
    void RenderInGameMenu(float scale);
    void ProcessInput(float scale, const ImVec2& boardBasePos, int& outHoveredPile, int& outHoveredCard);
    void ProcessAutoSolve();
    bool RenderBoard(ImDrawList* drawList, float scale, const ImVec2& boardBasePos, int hoveredPile, int hoveredCard);
    void CheckWinCondition(float scale, bool cardsAnimating);

    // Assets
    ImTextureID m_cardTextures[4][13] = { 0 };
    ImTextureID m_cardBackTexture = 0;
    void LoadCardTextures();

    // Rendering
    void DrawCard(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, const Card& card, float scale, float widthScale = 1.0f, bool isDragged = false, bool isHovered = false);
    void DrawCardBack(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, float widthScale = 1.0f, bool isDragged = false);
    void DrawEmptyPile(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, PileType type);
    void DrawSuit(ImDrawList* drawList, const ImVec2& center, float size, Suit suit, ImU32 color);
};
