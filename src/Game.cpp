#include "Game.h"
#include "imgui.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper macros and constants
const ImVec2 CARD_SIZE(100.0f, 140.0f);
const float CORNER_RADIUS = 8.0f;
const ImU32 COLOR_BG_DARK = IM_COL32(40, 40, 40, 255);
const ImU32 COLOR_BG_LIGHT = IM_COL32(250, 250, 250, 255);
const ImU32 COLOR_RED = IM_COL32(220, 50, 50, 255);
const ImU32 COLOR_BLACK = IM_COL32(30, 30, 30, 255);
const ImU32 COLOR_BORDER = IM_COL32(100, 100, 100, 255);

Game::Game() {
    InitGame(GameType::Klondike);
}

void Game::CreateDeck(std::vector<Card>& deck, int numDecks) {
    deck.clear();
    for (int d = 0; d < numDecks; ++d) {
        for (int s = 0; s < 4; ++s) {
            for (int r = 1; r <= 13; ++r) {
                Card c;
                c.rank = static_cast<Rank>(r);
                c.suit = static_cast<Suit>(s);
                c.faceUp = false;
                c.hasInitializedPos = false;
                c.flipVisual = -1.0f;
                deck.push_back(c);
            }
        }
    }
}

void Game::ShuffleDeck(std::vector<Card>& deck) {
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);
}

void Game::InitGame(GameType type) {
    m_currentType = type;
    m_piles.clear();
    m_dragSourcePile = -1;
    m_dragCardIndex = -1;
    m_dragCards.clear();
    m_isWon = false;
    m_bouncingCards.clear();
    m_winAnimTimer = 0.0f;

    if (type == GameType::Klondike) {
        InitKlondike();
    } else if (type == GameType::FreeCell) {
        InitFreeCell();
    } else if (type == GameType::Spider) {
        InitSpider();
    }
}

void Game::InitKlondike() {
    std::vector<Card> deck;
    CreateDeck(deck, 1);
    ShuffleDeck(deck);

    // Layout config
    float startX = 20.0f;
    float startY = 40.0f;
    float padX = CARD_SIZE.x + 20.0f;
    
    // 0: Stock
    Pile stock;
    stock.id = 0; stock.type = PileType::Stock;
    stock.pos = ImVec2(startX, startY);
    stock.size = CARD_SIZE;
    stock.offset = ImVec2(0.5f, 0.5f); // tiny offset for visual depth
    
    // 1: Waste
    Pile waste;
    waste.id = 1; waste.type = PileType::Waste;
    waste.pos = ImVec2(startX + padX, startY);
    waste.size = CARD_SIZE;
    waste.offset = ImVec2(20.0f, 0.0f); // horizontal offset

    // 2-5: Foundations
    std::vector<Pile> foundations(4);
    for (int i = 0; i < 4; ++i) {
        foundations[i].id = 2 + i;
        foundations[i].type = PileType::Foundation;
        foundations[i].pos = ImVec2(startX + padX * (3 + i), startY);
        foundations[i].size = CARD_SIZE;
        foundations[i].offset = ImVec2(0, 0);
    }

    // 6-12: Tableaus
    std::vector<Pile> tableaus(7);
    for (int i = 0; i < 7; ++i) {
        tableaus[i].id = 6 + i;
        tableaus[i].type = PileType::Tableau;
        tableaus[i].pos = ImVec2(startX + padX * i, startY + CARD_SIZE.y + 30.0f);
        tableaus[i].size = CARD_SIZE;
        tableaus[i].offset = ImVec2(0, 25.0f);

        // Deal cards
        for (int j = 0; j <= i; ++j) {
            Card c = deck.back();
            deck.pop_back();
            c.faceUp = (j == i);
            tableaus[i].cards.push_back(c);
        }
    }

    stock.cards = deck;

    m_piles.push_back(stock);
    m_piles.push_back(waste);
    for (const auto& p : foundations) m_piles.push_back(p);
    for (const auto& p : tableaus) m_piles.push_back(p);
}

void Game::InitFreeCell() {
    std::vector<Card> deck;
    CreateDeck(deck, 1);
    ShuffleDeck(deck);

    float startX = 20.0f;
    float startY = 40.0f;
    float padX = CARD_SIZE.x + 20.0f;

    // 0-3: FreeCells
    std::vector<Pile> freeCells(4);
    for (int i = 0; i < 4; ++i) {
        freeCells[i].id = i;
        freeCells[i].type = PileType::FreeCellSlot;
        freeCells[i].pos = ImVec2(startX + padX * i, startY);
        freeCells[i].size = CARD_SIZE;
        freeCells[i].offset = ImVec2(0, 0);
    }

    // 4-7: Foundations
    std::vector<Pile> foundations(4);
    for (int i = 0; i < 4; ++i) {
        foundations[i].id = 4 + i;
        foundations[i].type = PileType::Foundation;
        foundations[i].pos = ImVec2(startX + padX * (4 + i), startY);
        foundations[i].size = CARD_SIZE;
        foundations[i].offset = ImVec2(0, 0);
    }

    // 8-15: Tableaus
    std::vector<Pile> tableaus(8);
    for (int i = 0; i < 8; ++i) {
        tableaus[i].id = 8 + i;
        tableaus[i].type = PileType::Tableau;
        tableaus[i].pos = ImVec2(startX + padX * i, startY + CARD_SIZE.y + 30.0f);
        tableaus[i].size = CARD_SIZE;
        tableaus[i].offset = ImVec2(0, 25.0f);
        
        int count = (i < 4) ? 7 : 6;
        for (int j = 0; j < count; ++j) {
            Card c = deck.back();
            deck.pop_back();
            c.faceUp = true;
            tableaus[i].cards.push_back(c);
        }
    }

    for (const auto& p : freeCells) m_piles.push_back(p);
    for (const auto& p : foundations) m_piles.push_back(p);
    for (const auto& p : tableaus) m_piles.push_back(p);
}

void Game::InitSpider() {
    std::vector<Card> deck;
    CreateDeck(deck, 2); // 2 decks (104 cards)
    // For simplicity, make it a 1-suit spider game for now by forcing all to Spades
    for (auto& c : deck) c.suit = Suit::Spades;
    ShuffleDeck(deck);

    float startX = 20.0f;
    float startY = 40.0f;
    float padX = CARD_SIZE.x + 10.0f;

    // 0-9: Tableaus
    std::vector<Pile> tableaus(10);
    for (int i = 0; i < 10; ++i) {
        tableaus[i].id = i;
        tableaus[i].type = PileType::Tableau;
        tableaus[i].pos = ImVec2(startX + padX * i, startY + CARD_SIZE.y + 30.0f);
        tableaus[i].size = CARD_SIZE;
        tableaus[i].offset = ImVec2(0, 20.0f);
        
        int count = (i < 4) ? 6 : 5;
        for (int j = 0; j < count; ++j) {
            Card c = deck.back();
            deck.pop_back();
            c.faceUp = (j == count - 1);
            tableaus[i].cards.push_back(c);
        }
        m_piles.push_back(tableaus[i]);
    }

    // 10: Stock
    Pile stock;
    stock.id = 10;
    stock.type = PileType::Stock;
    stock.pos = ImVec2(startX, startY);
    stock.size = CARD_SIZE;
    stock.offset = ImVec2(10.0f, 0.0f);
    stock.cards = deck;
    m_piles.push_back(stock);
}

bool Game::CanPickup(int pileIdx, int cardIdx) {
    if (pileIdx < 0 || pileIdx >= m_piles.size()) return false;
    const Pile& p = m_piles[pileIdx];
    if (cardIdx < 0 || cardIdx >= p.cards.size()) return false;
    
    const Card& c = p.cards[cardIdx];
    if (!c.faceUp) return false;

    // Different games have different rules for picking up multiple cards
    if (m_currentType == GameType::Klondike) {
        // Must be alternating colors and decreasing rank
        for (int i = cardIdx; i < (int)p.cards.size() - 1; ++i) {
            const Card& c1 = p.cards[i];
            const Card& c2 = p.cards[i + 1];
            if (c1.Color() == c2.Color() || (int)c1.rank - 1 != (int)c2.rank) {
                return false;
            }
        }
        return true;
    } else if (m_currentType == GameType::FreeCell) {
        // Can only pick up alternating colors, decreasing rank
        // Strictly speaking, number of cards depends on empty freecells/tableaus, but we'll approximate first
        for (int i = cardIdx; i < (int)p.cards.size() - 1; ++i) {
            const Card& c1 = p.cards[i];
            const Card& c2 = p.cards[i + 1];
            if (c1.Color() == c2.Color() || (int)c1.rank - 1 != (int)c2.rank) {
                return false;
            }
        }
        return true;
    } else if (m_currentType == GameType::Spider) {
        // Must be same suit and decreasing rank
        for (int i = cardIdx; i < (int)p.cards.size() - 1; ++i) {
            const Card& c1 = p.cards[i];
            const Card& c2 = p.cards[i + 1];
            if (c1.suit != c2.suit || (int)c1.rank - 1 != (int)c2.rank) {
                return false;
            }
        }
        return true;
    }

    return true;
}

bool Game::CanDrop(int sourcePileIdx, const std::vector<Card>& cards, int targetPileIdx) {
    if (targetPileIdx < 0 || targetPileIdx >= m_piles.size() || cards.empty()) return false;
    if (sourcePileIdx == targetPileIdx) return false;
    
    const Pile& tp = m_piles[targetPileIdx];
    const Card& dragBottom = cards.front();

    if (m_currentType == GameType::Klondike) {
        if (tp.type == PileType::Foundation) {
            if (cards.size() > 1) return false;
            if (tp.cards.empty()) {
                return dragBottom.rank == Rank::Ace;
            } else {
                const Card& top = tp.cards.back();
                return top.suit == dragBottom.suit && (int)top.rank + 1 == (int)dragBottom.rank;
            }
        } else if (tp.type == PileType::Tableau) {
            if (tp.cards.empty()) {
                return dragBottom.rank == Rank::King;
            } else {
                const Card& top = tp.cards.back();
                return top.Color() != dragBottom.Color() && (int)top.rank - 1 == (int)dragBottom.rank;
            }
        }
    } else if (m_currentType == GameType::FreeCell) {
        if (tp.type == PileType::Foundation) {
            if (cards.size() > 1) return false;
            if (tp.cards.empty()) {
                return dragBottom.rank == Rank::Ace;
            } else {
                const Card& top = tp.cards.back();
                return top.suit == dragBottom.suit && (int)top.rank + 1 == (int)dragBottom.rank;
            }
        } else if (tp.type == PileType::Tableau) {
            if (tp.cards.empty()) {
                return true; // Any card to empty space
            } else {
                const Card& top = tp.cards.back();
                return top.Color() != dragBottom.Color() && (int)top.rank - 1 == (int)dragBottom.rank;
            }
        } else if (tp.type == PileType::FreeCellSlot) {
            return cards.size() == 1 && tp.cards.empty();
        }
    } else if (m_currentType == GameType::Spider) {
        if (tp.type == PileType::Tableau) {
            if (tp.cards.empty()) {
                return true;
            } else {
                const Card& top = tp.cards.back();
                return (int)top.rank - 1 == (int)dragBottom.rank;
            }
        } else if (tp.type == PileType::Foundation) {
            return false; // In spider, cards auto-move to foundation when a full set is made
        }
    }

    return false;
}

void Game::DoMove(int sourcePileIdx, int targetPileIdx, int cardIdx) {
    Pile& sp = m_piles[sourcePileIdx];
    Pile& tp = m_piles[targetPileIdx];
    
    // Move cards
    tp.cards.insert(tp.cards.end(), sp.cards.begin() + cardIdx, sp.cards.end());
    sp.cards.erase(sp.cards.begin() + cardIdx, sp.cards.end());

    // Flip top card of source if it's a tableau
    if (!sp.cards.empty() && sp.type == PileType::Tableau && !sp.cards.back().faceUp) {
        sp.cards.back().faceUp = true;
    }

    // Special Spider check: If tableau has full descending suit (K to A), remove it to foundation
    if (m_currentType == GameType::Spider && tp.type == PileType::Tableau && tp.cards.size() >= 13) {
        bool fullSet = true;
        Suit s = tp.cards.back().suit;
        int checkStart = (int)tp.cards.size() - 13;
        for (int i = 0; i < 13; ++i) {
            const Card& c = tp.cards[checkStart + i];
            if (!c.faceUp || c.suit != s || (int)c.rank != 13 - i) {
                fullSet = false;
                break;
            }
        }
        if (fullSet) {
            tp.cards.erase(tp.cards.begin() + checkStart, tp.cards.end());
            if (!tp.cards.empty() && !tp.cards.back().faceUp) {
                tp.cards.back().faceUp = true;
            }
        }
    }
}

void Game::HandleClick(int pileIdx) {
    if (pileIdx < 0 || pileIdx >= m_piles.size()) return;
    Pile& p = m_piles[pileIdx];

    if (m_currentType == GameType::Klondike) {
        if (p.type == PileType::Stock) {
            // Find waste pile
            int wasteIdx = -1;
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if (m_piles[i].type == PileType::Waste) { wasteIdx = i; break; }
            }
            if (wasteIdx != -1) {
                Pile& waste = m_piles[wasteIdx];
                if (!p.cards.empty()) {
                    // Move 1 card to waste
                    Card c = p.cards.back();
                    p.cards.pop_back();
                    c.faceUp = true;
                    waste.cards.push_back(c);
                } else {
                    // Recycle waste to stock
                    while (!waste.cards.empty()) {
                        Card c = waste.cards.back();
                        waste.cards.pop_back();
                        c.faceUp = false;
                        p.cards.push_back(c);
                    }
                }
            }
        }
    } else if (m_currentType == GameType::Spider) {
        if (p.type == PileType::Stock && !p.cards.empty()) {
            // Check if any tableaus are empty, in strict rules you can't deal if empty
            // But we'll allow it or skip it for now.
            
            // Deal one card to each tableau
            std::vector<int> tabIndices;
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if (m_piles[i].type == PileType::Tableau) tabIndices.push_back(i);
            }
            for (int tIdx : tabIndices) {
                if (p.cards.empty()) break;
                Card c = p.cards.back();
                p.cards.pop_back();
                c.faceUp = true;
                m_piles[tIdx].cards.push_back(c);
            }
        }
    }
}

void Game::UpdateAndDraw() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool doubleClicked = ImGui::IsMouseDoubleClicked(0);
    bool mouseReleased = ImGui::IsMouseReleased(0);

    // Calculate scale based on window width (1280 is our reference width)
    float scale = ImGui::GetWindowWidth() / 1280.0f;
    if (scale < 0.5f) scale = 0.5f;

    ImGui::GetIO().FontGlobalScale = scale;

    // Menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("New Klondike")) InitGame(GameType::Klondike);
            if (ImGui::MenuItem("New FreeCell")) InitGame(GameType::FreeCell);
            if (ImGui::MenuItem("New Spider")) InitGame(GameType::Spider);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) exit(0);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImVec2 winPos = ImGui::GetWindowPos();
    // Offset for the menu bar height if necessary, but since it's full screen window we should just start below menu
    winPos.y += ImGui::GetFrameHeight(); 

    // Check Win Condition
    if (!m_isWon && m_currentType == GameType::Klondike) {
        int foundationCount = 0;
        for (const auto& p : m_piles) {
            if (p.type == PileType::Foundation) foundationCount += p.cards.size();
        }
        if (foundationCount == 52) {
            bool animating = false;
            for (size_t i = 0; i < m_piles.size(); ++i) {
                const Pile& p = m_piles[i];
                ImVec2 basePos = ImVec2(winPos.x + p.pos.x * scale, winPos.y + p.pos.y * scale);
                ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);
                for (size_t c = 0; c < p.cards.size(); ++c) {
                    int drawIndex = (int)c;
                    if (p.type == PileType::Waste && p.cards.size() > 3) {
                        drawIndex = std::max(0, (int)c - (int)(p.cards.size() - 3));
                    }
                    ImVec2 targetPos = ImVec2(basePos.x + pOffset.x * drawIndex, basePos.y + pOffset.y * drawIndex);
                    float dx = p.cards[c].animPos.x - targetPos.x;
                    float dy = p.cards[c].animPos.y - targetPos.y;
                    if (dx * dx + dy * dy > 4.0f) {
                        animating = true;
                        break;
                    }
                }
                if (animating) break;
            }
            if (!animating) {
                m_isWon = true;
                m_winAnimTimer = 0.0f;
                m_bouncingCards.clear();
            }
        }
    }

    // Interaction vars
    int hoveredPile = -1;
    int hoveredCard = -1;

    if (!m_isWon) {
        // 1. Calculate Hover Logic BEFORE Drawing
        for (size_t i = 0; i < m_piles.size(); ++i) {
            Pile& p = m_piles[i];
            ImVec2 basePos = ImVec2(winPos.x + p.pos.x * scale, winPos.y + p.pos.y * scale);
            ImVec2 pSize = ImVec2(p.size.x * scale, p.size.y * scale);
            ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);

            if (p.cards.empty()) {
                if (mousePos.x >= basePos.x && mousePos.x <= basePos.x + pSize.x &&
                    mousePos.y >= basePos.y && mousePos.y <= basePos.y + pSize.y) {
                    hoveredPile = (int)i;
                    hoveredCard = -1;
                }
            } else {
                for (size_t c = 0; c < p.cards.size(); ++c) {
                    int drawIndex = (int)c;
                    if (p.type == PileType::Waste && p.cards.size() > 3) {
                        drawIndex = std::max(0, (int)c - (int)(p.cards.size() - 3));
                    }

                    ImVec2 cardPos = ImVec2(basePos.x + pOffset.x * drawIndex, basePos.y + pOffset.y * drawIndex);
                    if (mousePos.x >= cardPos.x && mousePos.x <= cardPos.x + pSize.x &&
                        mousePos.y >= cardPos.y && mousePos.y <= cardPos.y + pSize.y) {
                        hoveredPile = (int)i;
                        hoveredCard = (int)c;
                    }
                }
            }
        }

        // 2. Input Handling
        // Dropping Dragged Cards
        if (mouseReleased && m_dragSourcePile != -1) {
        ImVec2 dragBasePos = ImVec2(mousePos.x - m_dragOffset.x, mousePos.y - m_dragOffset.y);
        ImVec2 dragCenter = ImVec2(dragBasePos.x + CARD_SIZE.x * scale * 0.5f, dragBasePos.y + CARD_SIZE.y * scale * 0.5f);
        
        int bestDropPile = -1;
        float bestDistSq = 9999999.0f;
        
        for (size_t i = 0; i < m_piles.size(); ++i) {
            if ((int)i == m_dragSourcePile) continue;
            
            Pile& p = m_piles[i];
            int cCount = p.cards.empty() ? 0 : (int)p.cards.size() - 1;
            
            int targetDrawIndex = cCount;
            if (p.type == PileType::Waste && p.cards.size() > 3) {
                targetDrawIndex = std::max(0, cCount - (int)(p.cards.size() - 3));
            }
            
            ImVec2 targetPos = ImVec2(winPos.x + p.pos.x * scale + p.offset.x * scale * targetDrawIndex, 
                                      winPos.y + p.pos.y * scale + p.offset.y * scale * targetDrawIndex);
            ImVec2 targetCenter = ImVec2(targetPos.x + CARD_SIZE.x * scale * 0.5f, targetPos.y + CARD_SIZE.y * scale * 0.5f);
            
            float dx = dragCenter.x - targetCenter.x;
            float dy = dragCenter.y - targetCenter.y;
            float distSq = dx * dx + dy * dy;
            
            float maxDist = CARD_SIZE.x * scale * 1.5f; // Forgiving distance
            if (distSq < maxDist * maxDist && distSq < bestDistSq) {
                if (CanDrop(m_dragSourcePile, m_dragCards, i)) {
                    bestDropPile = (int)i;
                    bestDistSq = distSq;
                }
            }
        }
        
        if (bestDropPile != -1) {
            Pile& sp = m_piles[m_dragSourcePile];
            for (size_t i = 0; i < m_dragCards.size(); ++i) {
                sp.cards[m_dragCardIndex + i].animPos = m_dragCards[i].animPos;
            }
            DoMove(m_dragSourcePile, bestDropPile, m_dragCardIndex);
        } else {
            Pile& sp = m_piles[m_dragSourcePile];
            for (size_t i = 0; i < m_dragCards.size(); ++i) {
                sp.cards[m_dragCardIndex + i].animPos = m_dragCards[i].animPos;
            }
        }
        
        m_dragSourcePile = -1;
        m_dragCardIndex = -1;
        m_dragCards.clear();
    }

    // Double click auto-move
    if (doubleClicked && hoveredPile != -1 && hoveredCard != -1 && m_dragSourcePile == -1) {
        if (CanPickup(hoveredPile, hoveredCard)) {
            std::vector<Card> stack(m_piles[hoveredPile].cards.begin() + hoveredCard, m_piles[hoveredPile].cards.end());
            int bestDrop = -1;
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if (m_piles[i].type == PileType::Foundation && CanDrop(hoveredPile, stack, (int)i)) {
                    bestDrop = (int)i; break;
                }
            }
            if (bestDrop == -1) {
                for (size_t i = 0; i < m_piles.size(); ++i) {
                    if ((m_piles[i].type == PileType::Tableau || m_piles[i].type == PileType::FreeCellSlot) && CanDrop(hoveredPile, stack, (int)i)) {
                        bestDrop = (int)i; break;
                    }
                }
            }
            if (bestDrop != -1) {
                DoMove(hoveredPile, bestDrop, hoveredCard);
            }
        }
    }
    // Single Click / Start Drag
    else if (mouseClicked && hoveredPile != -1) {
        if (m_piles[hoveredPile].type == PileType::Stock) {
            HandleClick(hoveredPile);
        } else if (hoveredCard != -1 && CanPickup(hoveredPile, hoveredCard)) {
            m_dragSourcePile = hoveredPile;
            m_dragCardIndex = hoveredCard;
            Pile& p = m_piles[hoveredPile];
            m_dragCards.assign(p.cards.begin() + hoveredCard, p.cards.end());
            
            ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);
            int drawIndex = hoveredCard;
            if (p.type == PileType::Waste && p.cards.size() > 3) {
                drawIndex = std::max(0, (int)hoveredCard - (int)(p.cards.size() - 3));
            }
            ImVec2 cardPos = ImVec2(winPos.x + p.pos.x * scale + pOffset.x * drawIndex, 
                                    winPos.y + p.pos.y * scale + pOffset.y * drawIndex);
            m_dragOffset = ImVec2(mousePos.x - cardPos.x, mousePos.y - cardPos.y);
        }
    }

    // 3. Auto Solve (Klondike)
    if (m_currentType == GameType::Klondike && m_dragSourcePile == -1) {
        bool allFaceUp = true;
        bool hasCardsInPlay = false;

        for (const auto& p : m_piles) {
            if ((p.type == PileType::Stock || p.type == PileType::Waste) && !p.cards.empty()) {
                allFaceUp = false; break;
            }
        }

        if (allFaceUp) {
            for (const auto& p : m_piles) {
                if (p.type == PileType::Tableau) {
                    for (const auto& c : p.cards) {
                        if (!c.faceUp) { allFaceUp = false; break; }
                    }
                    if (!p.cards.empty()) hasCardsInPlay = true;
                }
                if (!allFaceUp) break;
            }
        }

        if (allFaceUp && hasCardsInPlay) {
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if (m_piles[i].type == PileType::Tableau && !m_piles[i].cards.empty()) {
                    int cardIdx = (int)m_piles[i].cards.size() - 1;
                    std::vector<Card> stack = { m_piles[i].cards.back() };
                    bool moved = false;
                    for (size_t f = 0; f < m_piles.size(); ++f) {
                        if (m_piles[f].type == PileType::Foundation && CanDrop((int)i, stack, (int)f)) {
                            DoMove((int)i, (int)f, cardIdx);
                            moved = true;
                            break;
                        }
                    }
                    if (moved) break;
                }
            }
        }
    }
    } // End of if (!m_isWon)

    // 4. Draw piles
    if (!m_isWon) {
        // Draw in correct order, from bottom to top
        float dt = ImGui::GetIO().DeltaTime;
        float animSpeed = 15.0f * dt;
        if (animSpeed > 1.0f) animSpeed = 1.0f;
        float flipSpeed = 10.0f * dt;

        for (size_t i = 0; i < m_piles.size(); ++i) {
            Pile& p = m_piles[i];
            ImVec2 basePos = ImVec2(winPos.x + p.pos.x * scale, winPos.y + p.pos.y * scale);
            ImVec2 pSize = ImVec2(p.size.x * scale, p.size.y * scale);
            ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);

            if (p.cards.empty()) {
                DrawEmptyPile(drawList, basePos, pSize, scale);
            } else {
                // Draw cards
                for (size_t c = 0; c < p.cards.size(); ++c) {
                    // Skip drawing dragged cards in their original pile
                    if (m_dragSourcePile == (int)i && (int)c >= m_dragCardIndex) continue;

                    int drawIndex = (int)c;
                    if (p.type == PileType::Waste && p.cards.size() > 3) {
                        drawIndex = std::max(0, (int)c - (int)(p.cards.size() - 3));
                    }

                    ImVec2 cardPos = ImVec2(basePos.x + pOffset.x * drawIndex, basePos.y + pOffset.y * drawIndex);
                    Card& cardRef = p.cards[c];

                    if (!cardRef.hasInitializedPos) {
                        cardRef.animPos = cardPos;
                        cardRef.hasInitializedPos = true;
                        cardRef.flipVisual = cardRef.faceUp ? 1.0f : -1.0f;
                    } else {
                        cardRef.animPos.x += (cardPos.x - cardRef.animPos.x) * animSpeed;
                        cardRef.animPos.y += (cardPos.y - cardRef.animPos.y) * animSpeed;
                    }

                    float targetFlip = cardRef.faceUp ? 1.0f : -1.0f;
                    if (cardRef.flipVisual < targetFlip) {
                        cardRef.flipVisual += flipSpeed;
                        if (cardRef.flipVisual > targetFlip) cardRef.flipVisual = targetFlip;
                    } else if (cardRef.flipVisual > targetFlip) {
                        cardRef.flipVisual -= flipSpeed;
                        if (cardRef.flipVisual < targetFlip) cardRef.flipVisual = targetFlip;
                    }

                    if (cardRef.flipVisual > 0.0f) {
                        DrawCard(drawList, cardRef.animPos, pSize, cardRef, scale, cardRef.flipVisual);
                    } else {
                        DrawCardBack(drawList, cardRef.animPos, pSize, scale, -cardRef.flipVisual);
                    }
                }
            }
        }

        // 5. Draw Dragged Cards
        if (m_dragSourcePile != -1 && !m_dragCards.empty()) {
            ImVec2 dragBasePos = ImVec2(mousePos.x - m_dragOffset.x, mousePos.y - m_dragOffset.y);
            ImVec2 pSize = ImVec2(CARD_SIZE.x * scale, CARD_SIZE.y * scale);
            ImVec2 pOffset = ImVec2(m_piles[m_dragSourcePile].offset.x * scale, m_piles[m_dragSourcePile].offset.y * scale);
            for (size_t c = 0; c < m_dragCards.size(); ++c) {
                ImVec2 cardPos = ImVec2(dragBasePos.x + pOffset.x * c, dragBasePos.y + pOffset.y * c);
                m_dragCards[c].animPos = cardPos; // Snap to mouse instantly
                DrawCard(drawList, cardPos, pSize, m_dragCards[c], scale, 1.0f, true);
            }
        }
    }

    if (m_isWon) {
        UpdateWinAnimation(drawList, scale);
    }
}

// Rendering Helpers

void Game::DrawEmptyPile(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale) {
    float r = CORNER_RADIUS * scale;
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 60, 30, 100), r);
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(50, 100, 50, 150), r, 0, 2.0f * scale);
}

void Game::DrawCardBack(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, float widthScale, bool isDragged) {
    float r = CORNER_RADIUS * scale;
    float s = isDragged ? 8.0f * scale : 2.0f * scale; // shadow offset
    float cx = pos.x + size.x * 0.5f;
    float w = size.x * widthScale;
    
    ImVec2 pMin(cx - w * 0.5f, pos.y);
    ImVec2 pMax(cx + w * 0.5f, pos.y + size.y);
    
    // Shadow
    ImU32 shadowColor = isDragged ? IM_COL32(0, 0, 0, 80) : IM_COL32(0, 0, 0, 50);
    drawList->AddRectFilled(ImVec2(pMin.x + s, pMin.y + s), ImVec2(pMax.x + s, pMax.y + s), shadowColor, r);
    
    // Background
    drawList->AddRectFilled(pMin, pMax, IM_COL32(20, 60, 120, 255), r);
    
    if (widthScale > 0.05f) {
        drawList->PushClipRect(pMin, pMax, true);
        // Pattern (simple grid)
        float step = 10.0f * scale;
        for (float x = step; x < size.x; x += step) {
            drawList->AddLine(ImVec2(pos.x + x, pos.y), ImVec2(pos.x + x, pos.y + size.y), IM_COL32(30, 80, 140, 255), 1.0f * scale);
        }
        for (float y = step; y < size.y; y += step) {
            drawList->AddLine(ImVec2(pos.x, pos.y + y), ImVec2(pos.x + size.x, pos.y + y), IM_COL32(30, 80, 140, 255), 1.0f * scale);
        }
        drawList->PopClipRect();
    }

    // Border
    drawList->AddRect(pMin, pMax, COLOR_BORDER, r, 0, 1.0f * scale);
}

void Game::DrawCard(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, const Card& card, float scale, float widthScale, bool isDragged) {
    float r = CORNER_RADIUS * scale;
    float s = isDragged ? 8.0f * scale : 2.0f * scale; // shadow offset
    float cx = pos.x + size.x * 0.5f;
    float w = size.x * widthScale;

    ImVec2 pMin(cx - w * 0.5f, pos.y);
    ImVec2 pMax(cx + w * 0.5f, pos.y + size.y);

    // Shadow
    ImU32 shadowColor = isDragged ? IM_COL32(0, 0, 0, 80) : IM_COL32(0, 0, 0, 50);
    drawList->AddRectFilled(ImVec2(pMin.x + s, pMin.y + s), ImVec2(pMax.x + s, pMax.y + s), shadowColor, r);
    
    // Background
    drawList->AddRectFilled(pMin, pMax, COLOR_BG_LIGHT, r);
    
    if (widthScale > 0.05f) {
        drawList->PushClipRect(pMin, pMax, true);
        
        ImU32 color = card.IsRed() ? COLOR_RED : COLOR_BLACK;

        // Rank text
        const char* ranks[] = { "?", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
        const char* rankStr = ranks[(int)card.rank];
        
        float pad = 5.0f * scale;
        float fontSize = ImGui::GetFontSize() * 1.5f;

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + pad, pos.y + pad), color, rankStr);

        // Mini Suit below rank
        DrawSuit(drawList, ImVec2(pos.x + 10.0f * scale, pos.y + fontSize + 15.0f * scale), 8.0f * scale, card.suit, color);

        // Bottom right inverted text and suit
        // ImGui lacks standard text rotation, so we just draw normal text but at bottom right
        ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, rankStr);
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x - tsize.x - pad, pos.y + size.y - tsize.y - 25.0f * scale), color, rankStr);
        DrawSuit(drawList, ImVec2(pos.x + size.x - 10.0f * scale, pos.y + size.y - 15.0f * scale), 8.0f * scale, card.suit, color);
        ImGui::PopFont();
        // Center big suit
        DrawSuit(drawList, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), 24.0f * scale, card.suit, color);
        
        drawList->PopClipRect();
    }

    // Border
    drawList->AddRect(pMin, pMax, COLOR_BORDER, r, 0, 1.0f * scale);
}

void Game::DrawSuit(ImDrawList* drawList, const ImVec2& center, float size, Suit suit, ImU32 color) {
    if (suit == Suit::Hearts) {
        // Procedural heart
        float r = size * 0.5f;
        drawList->AddCircleFilled(ImVec2(center.x - r, center.y - r), r, color, 12);
        drawList->AddCircleFilled(ImVec2(center.x + r, center.y - r), r, color, 12);
        
        ImVec2 p1(center.x - r * 2.0f + 0.5f, center.y - r + 1.0f);
        ImVec2 p2(center.x + r * 2.0f - 0.5f, center.y - r + 1.0f);
        ImVec2 p3(center.x, center.y + size * 1.2f);
        drawList->AddTriangleFilled(p1, p2, p3, color);
    } 
    else if (suit == Suit::Diamonds) {
        ImVec2 p1(center.x, center.y - size);
        ImVec2 p2(center.x + size * 0.8f, center.y);
        ImVec2 p3(center.x, center.y + size);
        ImVec2 p4(center.x - size * 0.8f, center.y);
        drawList->AddQuadFilled(p1, p2, p3, p4, color);
    } 
    else if (suit == Suit::Spades) {
        float r = size * 0.5f;
        drawList->AddCircleFilled(ImVec2(center.x - r, center.y + r), r, color, 12);
        drawList->AddCircleFilled(ImVec2(center.x + r, center.y + r), r, color, 12);
        
        ImVec2 p1(center.x - r * 2.0f + 0.5f, center.y + r - 1.0f);
        ImVec2 p2(center.x + r * 2.0f - 0.5f, center.y + r - 1.0f);
        ImVec2 p3(center.x, center.y - size * 1.2f);
        drawList->AddTriangleFilled(p1, p2, p3, color);
        
        // Base
        drawList->AddTriangleFilled(ImVec2(center.x, center.y + r), 
                                    ImVec2(center.x - r, center.y + size * 1.5f),
                                    ImVec2(center.x + r, center.y + size * 1.5f), color);
    } 
    else if (suit == Suit::Clubs) {
        float r = size * 0.4f;
        drawList->AddCircleFilled(ImVec2(center.x, center.y - r * 1.1f), r, color, 12);
        drawList->AddCircleFilled(ImVec2(center.x - r * 1.1f, center.y + r * 0.5f), r, color, 12);
        drawList->AddCircleFilled(ImVec2(center.x + r * 1.1f, center.y + r * 0.5f), r, color, 12);
        
        // Fill center gap seamlessly
        drawList->AddCircleFilled(center, r * 0.8f, color, 12);
        
        // Base (stem)
        drawList->AddTriangleFilled(ImVec2(center.x, center.y + r * 0.2f), 
                                    ImVec2(center.x - r * 0.8f, center.y + size * 1.2f),
                                    ImVec2(center.x + r * 0.8f, center.y + size * 1.2f), color);
    }
}

bool Game::IsWon() const {
    return m_isWon;
}

void Game::UpdateWinAnimation(ImDrawList* drawList, float scale) {
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 winPos = ImGui::GetWindowPos();
    float dt = ImGui::GetIO().DeltaTime;
    
    m_winAnimTimer -= dt;
    
    // Spawn a new bouncing card periodically from random piles
    if (m_winAnimTimer <= 0.0f) {
        // Find all piles with cards
        std::vector<int> validPiles;
        for (size_t i = 0; i < m_piles.size(); ++i) {
            if (!m_piles[i].cards.empty()) {
                validPiles.push_back((int)i);
            }
        }
        
        if (!validPiles.empty()) {
            int foundPileIdx = validPiles[rand() % validPiles.size()];
            Pile& p = m_piles[foundPileIdx];
            Card c = p.cards.back();
            p.cards.pop_back();
            
            BouncingCard bc;
            bc.card = c;
            ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);
            int drawIndex = p.cards.size(); // The position it was at
            bc.pos = ImVec2(winPos.x + p.pos.x * scale + pOffset.x * drawIndex,
                            winPos.y + p.pos.y * scale + pOffset.y * drawIndex);
            
            // Initial velocity: random x, up y
            float vx = (rand() % 100 > 50 ? 1 : -1) * (200.0f + (rand() % 200)) * scale;
            bc.velocity = ImVec2(vx, (-200.0f - (rand() % 400)) * scale);
            
            m_bouncingCards.push_back(bc);
            m_winAnimTimer = 0.5f; // Wait before next card
        }
    }
    
    // Update and draw bouncing cards
    float gravity = 900.0f * scale;
    float bounceLoss = 0.8f;
    ImVec2 pSize = ImVec2(CARD_SIZE.x * scale, CARD_SIZE.y * scale);
    
    for (auto& bc : m_bouncingCards) {
        bc.velocity.y += gravity * dt;
        bc.pos.x += bc.velocity.x * dt;
        bc.pos.y += bc.velocity.y * dt;
        
        // Bounce off bottom
        if (bc.pos.y + pSize.y > winPos.y + winSize.y) {
            bc.pos.y = winPos.y + winSize.y - pSize.y;
            bc.velocity.y = -bc.velocity.y * bounceLoss;
        }
        // Bounce off sides
        if (bc.pos.x < winPos.x) {
            bc.pos.x = winPos.x;
            bc.velocity.x = -bc.velocity.x;
        } else if (bc.pos.x + pSize.x > winPos.x + winSize.x) {
            bc.pos.x = winPos.x + winSize.x - pSize.x;
            bc.velocity.x = -bc.velocity.x;
        }
        
        DrawCard(drawList, bc.pos, pSize, bc.card, scale, 1.0f, false);
    }
}
