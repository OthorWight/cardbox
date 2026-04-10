#include "Game.h"
#include "imgui.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GLFW/glfw3.h> // Ensure we have GL functions

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

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
    SetupLuaBindings();
    LoadCardTextures();
    LoadAvailableGames();
    if (!m_availableGames.empty()) {
        InitGame(m_availableGames[0]);
    }
}

void Game::SetupLuaBindings() {
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

    m_lua.new_usertype<ImVec2>("ImVec2",
        sol::constructors<ImVec2(), ImVec2(float, float)>(),
        "x", &ImVec2::x, "y", &ImVec2::y
    );

    m_lua.new_enum("Suit", "Hearts", Suit::Hearts, "Diamonds", Suit::Diamonds, "Clubs", Suit::Clubs, "Spades", Suit::Spades);
    m_lua.new_enum("Rank", "Ace", Rank::Ace, "Two", Rank::Two, "Three", Rank::Three, "Four", Rank::Four, 
                   "Five", Rank::Five, "Six", Rank::Six, "Seven", Rank::Seven, "Eight", Rank::Eight, 
                   "Nine", Rank::Nine, "Ten", Rank::Ten, "Jack", Rank::Jack, "Queen", Rank::Queen, "King", Rank::King);
    m_lua.new_enum("PileType", "Stock", PileType::Stock, "Waste", PileType::Waste, "Tableau", PileType::Tableau, 
                   "Foundation", PileType::Foundation, "FreeCellSlot", PileType::FreeCellSlot, "Invisible", PileType::Invisible);

    m_lua.new_usertype<Card>("Card",
        "rank", sol::property([](const Card& c) { return (int)c.rank; }, [](Card& c, int r) { c.rank = (Rank)r; }),
        "suit", sol::property([](const Card& c) { return (int)c.suit; }, [](Card& c, int s) { c.suit = (Suit)s; }),
        "faceUp", &Card::faceUp,
        "Color", &Card::Color,
        "IsRed", &Card::IsRed
    );

    m_lua.new_usertype<std::vector<Card>>("VectorCard",
        sol::constructors<std::vector<Card>()>(),
        "size", &std::vector<Card>::size,
        "empty", &std::vector<Card>::empty,
        "clear", &std::vector<Card>::clear,
        "push_back", [](std::vector<Card>& v, const Card& c) { v.push_back(c); },
        "pop_back", [](std::vector<Card>& v) { v.pop_back(); },
        "back", [](std::vector<Card>& v) -> Card& { return v.back(); },
        "front", [](std::vector<Card>& v) -> Card& { return v.front(); },
        "get", [](std::vector<Card>& v, int i) -> Card& { return v[i]; }
    );

    m_lua.new_usertype<Pile>("Pile",
        "cards", &Pile::cards,
        "pos", &Pile::pos,
        "size", &Pile::size,
        "offset", &Pile::offset,
        "type", &Pile::type,
        "id", &Pile::id
    );

    m_lua.new_usertype<std::vector<Pile>>("VectorPile",
        sol::constructors<std::vector<Pile>()>(),
        "size", &std::vector<Pile>::size,
        "empty", &std::vector<Pile>::empty,
        "clear", &std::vector<Pile>::clear,
        "push_back", [](std::vector<Pile>& v, const Pile& p) { v.push_back(p); },
        "get", [](std::vector<Pile>& v, int i) -> Pile& { return v[i]; }
    );
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

void Game::LoadAvailableGames() {
    m_availableGames.clear();

    std::vector<std::string> searchPaths = { "rules", "src" };
    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.path().extension() == ".lua") {
                    m_availableGames.push_back(entry.path().string());
                }
            }
        }
    }
}

void Game::LoadCardTextures() {
    const char* suits[] = { "hearts", "diamonds", "clubs", "spades" };
    const char* ranks[] = { "ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen", "king" };

    // Robust path resolution since the executable might be run from the project root or the build/ folder
    std::string baseDir = "cards/";
    if (std::filesystem::exists("cards")) {
        baseDir = "cards/";
    } else if (std::filesystem::exists("build/cards")) {
        baseDir = "build/cards/";
    } else if (std::filesystem::exists("../cards")) {
        baseDir = "../cards/";
    }

    auto loadTexture = [](const std::string& path) -> ImTextureID {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Texture not found: " << path << std::endl;
            return 0;
        }
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data) {
            std::cerr << "Failed to load image data: " << path << std::endl;
            return 0;
        }
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        return (ImTextureID)(intptr_t)texture;
    };

    for (int s = 0; s < 4; ++s) {
        for (int r = 0; r < 13; ++r) {
            std::string path = baseDir + std::string(ranks[r]) + "_of_" + std::string(suits[s]) + ".png";
            m_cardTextures[s][r] = loadTexture(path);
        }
    }
    m_cardBackTexture = loadTexture(baseDir + "card back blue.png"); // Repo has no back by default, so procedural fallback activates
}

void Game::InitGame(const std::string& scriptPath) {
    m_piles.clear();
    m_dragSourcePile = -1;
    m_dragCardIndex = -1;
    m_dragCards.clear();
    m_undoStack.clear();
    m_isWon = false;
    m_bouncingCards.clear();
    m_winTrails.clear();
    m_particles.clear();
    m_winAnimTimer = 0.0f;
    m_particles.clear();

    try {
        m_currentScriptPath = scriptPath;
        m_lua.script_file(scriptPath);
        m_currentGameName = m_lua["GameName"].get_or<std::string>("Unknown Game");
        m_currentHelpText = m_lua["HelpText"].get_or<std::string>("No help available.");

        std::vector<Card> deck;
        int numDecks = m_lua["NumDecks"].get_or(1);
        CreateDeck(deck, numDecks);
        ShuffleDeck(deck);

        sol::protected_function initFunc = m_lua["Init"];
        if (initFunc.valid()) {
            initFunc(m_piles, deck);
        }
    } catch (const sol::error& e) {
        std::cerr << "Lua Error during InitGame: " << e.what() << std::endl;
    }
}

bool Game::CanPickup(int pileIdx, int cardIdx) {
    if (pileIdx < 0 || pileIdx >= m_piles.size()) return false;
    const Pile& p = m_piles[pileIdx];
    if (cardIdx < 0 || cardIdx >= p.cards.size()) return false;
    
    const Card& c = p.cards[cardIdx];
    if (!c.faceUp) return false;

    sol::protected_function canPickup = m_lua["CanPickup"];
    if (canPickup.valid()) {
        return canPickup(m_piles, pileIdx, cardIdx);
    }
    return false;
}

bool Game::CanDrop(int sourcePileIdx, const std::vector<Card>& cards, int targetPileIdx) {
    if (targetPileIdx < 0 || targetPileIdx >= m_piles.size() || cards.empty()) return false;
    if (sourcePileIdx == targetPileIdx) return false;
    
    sol::protected_function canDrop = m_lua["CanDrop"];
    if (canDrop.valid()) {
        return canDrop(m_piles, sourcePileIdx, targetPileIdx, cards);
    }
    return false;
}

void Game::DoMove(int sourcePileIdx, int targetPileIdx, int cardIdx) {
    Pile& sp = m_piles[sourcePileIdx];
    Pile& tp = m_piles[targetPileIdx];
    
    // Move cards
    tp.cards.insert(tp.cards.end(), sp.cards.begin() + cardIdx, sp.cards.end());
    sp.cards.erase(sp.cards.begin() + cardIdx, sp.cards.end());

    sol::protected_function afterMove = m_lua["AfterMove"];
    if (afterMove.valid()) {
        afterMove(m_piles, sourcePileIdx, targetPileIdx, cardIdx);
    }
}

void Game::HandleClick(int pileIdx) {
    if (pileIdx < 0 || pileIdx >= m_piles.size()) return;
    
    if (m_piles[pileIdx].type == PileType::Stock) {
        SaveStateForUndo();
    }
    
    sol::protected_function handleClick = m_lua["HandleClick"];
    if (handleClick.valid()) {
        handleClick(m_piles, pileIdx);
    }
}

void Game::UpdateAndDraw() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool mouseReleased = ImGui::IsMouseReleased(0);
    bool rightClicked = ImGui::IsMouseClicked(1);

    // Calculate scale based on window width (1280 is our reference width)
    float scale = ImGui::GetWindowWidth() / 1280.0f;
    if (scale < 0.5f) scale = 0.5f;

    ImGui::GetIO().FontGlobalScale = scale;

    // Menu bar
    bool showHelp = false;
    bool doUndo = false;

    if (ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyCtrl) {
        doUndo = true;
    }
    if (ImGui::IsMouseClicked(3)) { // Mouse Backward Button
        doUndo = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        InitGame(m_currentScriptPath);
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) doUndo = true;
            if (ImGui::MenuItem("Restart Game", "F2")) InitGame(m_currentScriptPath);
            ImGui::Separator();
            for (const auto& gamePath : m_availableGames) {
                std::string displayName = std::filesystem::path(gamePath).stem().string();
                if (ImGui::MenuItem(("New " + displayName).c_str())) {
                    InitGame(gamePath);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) exit(0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem(("How to play " + m_currentGameName).c_str())) showHelp = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (showHelp) ImGui::OpenPopup("Help");

    // Process undo
    if (doUndo) {
        Undo();
    }

    if (ImGui::BeginPopupModal("Help", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", m_currentGameName.c_str());
        ImGui::Separator();
        ImGui::Text("%s", m_currentHelpText.c_str());
        if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImVec2 winPos = ImGui::GetWindowPos();
    // Offset for the menu bar height if necessary, but since it's full screen window we should just start below menu
    winPos.y += ImGui::GetFrameHeight(); 

    // 1. Draw a nice gradient background for a "Lit Felt Table" look
    drawList->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + ImGui::GetWindowWidth(), winPos.y + ImGui::GetWindowHeight()), 
                                      IM_COL32(30, 90, 30, 255), IM_COL32(30, 90, 30, 255), IM_COL32(12, 35, 12, 255), IM_COL32(12, 35, 12, 255));

    // Calculate logical bounding box to center the board
    float minLogicalX = 999999.0f;
    float maxLogicalX = -999999.0f;
    for (const auto& p : m_piles) {
        if (p.type == PileType::Invisible) continue;
        float leftEdge = p.pos.x;
        float rightEdge = p.pos.x + p.size.x;
        if (p.offset.x > 0) rightEdge += 2 * p.offset.x;
        else if (p.offset.x < 0) leftEdge += 2 * p.offset.x;
        
        if (leftEdge < minLogicalX) minLogicalX = leftEdge;
        if (rightEdge > maxLogicalX) maxLogicalX = rightEdge;
    }
    if (minLogicalX > maxLogicalX) {
        minLogicalX = 0.0f;
        maxLogicalX = 1280.0f;
    }

    float logicalWidth = maxLogicalX - minLogicalX;
    float boardPixelWidth = logicalWidth * scale;
    float boardOffsetX = (ImGui::GetWindowWidth() - boardPixelWidth) * 0.5f;
    if (boardOffsetX < 0.0f) boardOffsetX = 0.0f;
    
    ImVec2 boardBasePos = winPos;
    boardBasePos.x += boardOffsetX - (minLogicalX * scale);

    // Interaction vars
    int hoveredPile = -1;
    int hoveredCard = -1;

    if (!m_isWon) {
        // 2. Calculate Hover Logic BEFORE Drawing
        if (ImGui::IsWindowHovered()) {
            for (size_t i = 0; i < m_piles.size(); ++i) {
                Pile& p = m_piles[i];
                ImVec2 basePos = ImVec2(boardBasePos.x + p.pos.x * scale, boardBasePos.y + p.pos.y * scale);
                ImVec2 pSize = ImVec2(p.size.x * scale, p.size.y * scale);
                ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);
    
                if (p.cards.empty()) {
                    if (p.type != PileType::Invisible) {
                        if (mousePos.x >= basePos.x && mousePos.x <= basePos.x + pSize.x &&
                            mousePos.y >= basePos.y && mousePos.y <= basePos.y + pSize.y) {
                            hoveredPile = (int)i;
                            hoveredCard = -1;
                        }
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
        }

        // 3. Input Handling
        // Dropping Dragged Cards
        if (mouseReleased && m_dragSourcePile != -1) {
        ImVec2 dragBasePos = ImVec2(mousePos.x - m_dragOffset.x, mousePos.y - m_dragOffset.y);
        ImVec2 dragCenter = ImVec2(dragBasePos.x + CARD_SIZE.x * scale * 0.5f, dragBasePos.y + CARD_SIZE.y * scale * 0.5f);
        
        int bestDropPile = -1;
        float bestDistSq = 9999999.0f;
        
        if (ImGui::IsWindowHovered()) {
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if ((int)i == m_dragSourcePile) continue;
                
                Pile& p = m_piles[i];
                int cCount = p.cards.empty() ? 0 : (int)p.cards.size() - 1;
                
                int targetDrawIndex = cCount;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    targetDrawIndex = std::max(0, cCount - (int)(p.cards.size() - 3));
                }
                
                ImVec2 targetPos = ImVec2(boardBasePos.x + p.pos.x * scale + p.offset.x * scale * targetDrawIndex, 
                                          boardBasePos.y + p.pos.y * scale + p.offset.y * scale * targetDrawIndex);
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
        }
        
        if (bestDropPile != -1) {
            SaveStateForUndo();
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

    // Right-click auto-move
    if (rightClicked && hoveredPile != -1 && hoveredCard != -1 && m_dragSourcePile == -1) {
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
                            SaveStateForUndo();
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
            ImVec2 cardPos = ImVec2(boardBasePos.x + p.pos.x * scale + pOffset.x * drawIndex, 
                                    boardBasePos.y + p.pos.y * scale + pOffset.y * drawIndex);
            m_dragOffset = ImVec2(mousePos.x - cardPos.x, mousePos.y - cardPos.y);
        }
    }

    // Cancel drag with Escape or Right Click
    if (m_dragSourcePile != -1 && (ImGui::IsKeyPressed(ImGuiKey_Escape) || rightClicked)) {
        Pile& sp = m_piles[m_dragSourcePile];
        for (size_t i = 0; i < m_dragCards.size(); ++i) {
            sp.cards[m_dragCardIndex + i].animPos = m_dragCards[i].animPos;
        }
        m_dragSourcePile = -1;
        m_dragCardIndex = -1;
        m_dragCards.clear();
    }

    // 4. Auto Solve
    if (m_dragSourcePile == -1) {
        sol::protected_function autoSolve = m_lua["AutoSolve"];
        if (autoSolve.valid()) {
            sol::protected_function_result result = autoSolve(m_piles);
            if (result.valid() && result.get_type() == sol::type::table) {
                sol::table move = result;
                if (!move.empty()) {
                    int src = move[1];
                    int dst = move[2];
                    int idx = move[3];
                    // Do not SaveStateForUndo() here to avoid flooding the undo stack with single auto-moves
                    DoMove(src, dst, idx);
                }
            }
        }
    }
    } // End of if (!m_isWon)

    // 4. Draw piles
    bool cardsAnimating = false;
    {
        // Draw in correct order, from bottom to top
        float dt = ImGui::GetIO().DeltaTime;
        float animSpeed = 15.0f * dt;
        if (animSpeed > 1.0f) animSpeed = 1.0f;
        float flipSpeed = 10.0f * dt;

        for (size_t i = 0; i < m_piles.size(); ++i) {
            Pile& p = m_piles[i];
            ImVec2 basePos = ImVec2(boardBasePos.x + p.pos.x * scale, boardBasePos.y + p.pos.y * scale);
            ImVec2 pSize = ImVec2(p.size.x * scale, p.size.y * scale);
            ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);

            if (p.cards.empty()) {
                DrawEmptyPile(drawList, basePos, pSize, scale, p.type);
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

                        if (std::abs(cardRef.animPos.x - cardPos.x) > 1.0f || std::abs(cardRef.animPos.y - cardPos.y) > 1.0f) {
                            cardsAnimating = true;
                        }
                    }

                    float targetFlip = cardRef.faceUp ? 1.0f : -1.0f;
                    if (cardRef.flipVisual < targetFlip) {
                        cardRef.flipVisual += flipSpeed;
                        if (cardRef.flipVisual > targetFlip) cardRef.flipVisual = targetFlip;
                        cardsAnimating = true;
                    } else if (cardRef.flipVisual > targetFlip) {
                        cardRef.flipVisual -= flipSpeed;
                        if (cardRef.flipVisual < targetFlip) cardRef.flipVisual = targetFlip;
                        cardsAnimating = true;
                    }

                    if (cardRef.flipVisual > 0.0f) {
                        DrawCard(drawList, cardRef.animPos, pSize, cardRef, scale, cardRef.flipVisual, false, hoveredPile == (int)i && hoveredCard == (int)c);
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

            // Calculate Magnetic Force
            ImVec2 pullOffset(0, 0);
            ImVec2 dragCenter = ImVec2(dragBasePos.x + pSize.x * 0.5f, dragBasePos.y + pSize.y * 0.5f);
            float maxDist = pSize.x * 1.8f; // Range of magnetic effect

            for (size_t i = 0; i < m_piles.size(); ++i) {
                if ((int)i == m_dragSourcePile) continue;
                
                Pile& p = m_piles[i];
                int cCount = p.cards.empty() ? 0 : (int)p.cards.size() - 1;
                int targetDrawIndex = cCount;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    targetDrawIndex = std::max(0, cCount - (int)(p.cards.size() - 3));
                }
                
                ImVec2 targetPos = ImVec2(boardBasePos.x + p.pos.x * scale + p.offset.x * scale * targetDrawIndex, 
                                          boardBasePos.y + p.pos.y * scale + p.offset.y * scale * targetDrawIndex);
                ImVec2 targetCenter = ImVec2(targetPos.x + pSize.x * 0.5f, targetPos.y + pSize.y * 0.5f);
                
                float dx = targetCenter.x - dragCenter.x;
                float dy = targetCenter.y - dragCenter.y;
                float distSq = dx * dx + dy * dy;
                
                if (distSq > 0.0001f && distSq < maxDist * maxDist) {
                    float dist = std::sqrt(distSq);
                    bool canDrop = CanDrop(m_dragSourcePile, m_dragCards, (int)i);
                    float strength = std::pow((maxDist - dist) / maxDist, 2.0f); // Ease-in falloff
                    
                    if (canDrop) {
                        // Magnetic Pull (Snap target)
                        pullOffset.x += dx * strength * 0.6f;
                        pullOffset.y += dy * strength * 0.6f;
                    } else {
                        // Magnetic Push (Repel invalid target)
                        pullOffset.x -= (dx / dist) * strength * pSize.x * 0.05f;
                        pullOffset.y -= (dy / dist) * strength * pSize.x * 0.05f;
                    }
                }
            }
            
            dragBasePos.x += pullOffset.x;
            dragBasePos.y += pullOffset.y;

            for (size_t c = 0; c < m_dragCards.size(); ++c) {
                // Dynamic sway: bottom cards in a dragged stack lag behind or fan out 
                // in the direction of the magnetic force, faking a 3D turning/fanning effect.
                ImVec2 swayOffset(pullOffset.x * 0.08f * c, pullOffset.y * 0.08f * c);
                ImVec2 cardPos = ImVec2(dragBasePos.x + pOffset.x * c + swayOffset.x, 
                                        dragBasePos.y + pOffset.y * c + swayOffset.y);
                
                m_dragCards[c].animPos = cardPos;
                DrawCard(drawList, cardPos, pSize, m_dragCards[c], scale, 1.0f, true);
            }
        }
    }

    // Check Win Condition (Only trigger once cards finish flying)
    if (!m_isWon && !cardsAnimating) {
        sol::protected_function isWonFunc = m_lua["IsWon"];
        if (isWonFunc.valid() && isWonFunc(m_piles)) {
            m_isWon = true;
            m_winAnimTimer = 0.0f;
            m_bouncingCards.clear();
            m_winTrails.clear();
            
            // Spawn explosion particles!
            for (int i = 0; i < 500; ++i) {
                Particle p;
                p.pos = mousePos;
                float angle = (rand() % 360) * M_PI / 180.0f;
                float speed = 100.0f + (rand() % 600);
                p.velocity = ImVec2(cos(angle) * speed, sin(angle) * speed - 300.0f);
                p.life = 1.0f + (rand() % 200) / 100.0f;
                p.size = 2.0f + (rand() % 6);
                p.color = IM_COL32(100 + rand() % 155, 100 + rand() % 155, 100 + rand() % 155, 255);
                m_particles.push_back(p);
            }
        }
    }

    if (m_isWon && !cardsAnimating) {
        UpdateWinAnimation(drawList, scale);
    }
}

// Rendering Helpers

void Game::DrawEmptyPile(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, PileType type) {
    if (type == PileType::Invisible) return;

    float r = CORNER_RADIUS * scale;
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 60, 30, 100), r);
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(50, 100, 50, 150), r, 0, 2.0f * scale);

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    float fontSize = ImGui::GetFontSize() * 2.0f;
    ImU32 textColor = IM_COL32(50, 100, 50, 200);

    if (type == PileType::Foundation) {
        ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "A");
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x * 0.5f - tsize.x * 0.5f, pos.y + size.y * 0.5f - tsize.y * 0.5f), textColor, "A");
    } else if (type == PileType::FreeCellSlot) {
        fontSize = ImGui::GetFontSize() * 1.5f;
        ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "Free");
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x * 0.5f - tsize.x * 0.5f, pos.y + size.y * 0.5f - tsize.y * 0.5f), textColor, "Free");
    }

    ImGui::PopFont();
}

void Game::DrawCardBack(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, float widthScale, bool isDragged) {
    float r = CORNER_RADIUS * scale;
    float s = isDragged ? 8.0f * scale : 2.0f * scale; // shadow offset
    float cx = pos.x + size.x * 0.5f;
    float w = size.x * widthScale;
    
    ImVec2 pMin(cx - w * 0.5f, pos.y);
    ImVec2 pMax(cx + w * 0.5f, pos.y + size.y);
    
    // Shadow
    for (int i = 0; i < 3; ++i) {
        float off = s + (i * scale);
        ImU32 shadowColor = isDragged ? IM_COL32(0, 0, 0, 35) : IM_COL32(0, 0, 0, 15);
        drawList->AddRectFilled(ImVec2(pMin.x + off, pMin.y + off), ImVec2(pMax.x + off, pMax.y + off), shadowColor, r);
    }
    
    if (m_cardBackTexture && widthScale > 0.05f) {
        drawList->AddRectFilled(pMin, pMax, COLOR_BG_LIGHT, r); // Solid background
        float uvX0 = (1.0f - widthScale) * 0.5f;
        float uvX1 = 1.0f - uvX0;
        drawList->AddImageRounded(m_cardBackTexture, pMin, pMax, ImVec2(uvX0, 0), ImVec2(uvX1, 1), IM_COL32_WHITE, r);
    } else {
        // Nicer Fallback Background (Casino Blue with inset border)
        drawList->AddRectFilled(pMin, pMax, IM_COL32(35, 75, 145, 255), r);
        
        if (widthScale > 0.05f) {
            drawList->AddRect(ImVec2(pMin.x + 6.0f * scale, pMin.y + 6.0f * scale), ImVec2(pMax.x - 6.0f * scale, pMax.y - 6.0f * scale), IM_COL32(255, 255, 255, 100), r * 0.5f, 0, 1.5f * scale);
        }
    }

    // Border
    drawList->AddRect(pMin, pMax, COLOR_BORDER, r, 0, 1.0f * scale);
}

void Game::DrawCard(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, const Card& card, float scale, float widthScale, bool isDragged, bool isHovered) {
    float r = CORNER_RADIUS * scale;
    float s = isDragged ? 8.0f * scale : 2.0f * scale; // shadow offset
    float cx = pos.x + size.x * 0.5f;
    float w = size.x * widthScale;

    ImVec2 pMin(cx - w * 0.5f, pos.y);
    ImVec2 pMax(cx + w * 0.5f, pos.y + size.y);

    // Shadow
    for (int i = 0; i < 3; ++i) {
        float off = s + (i * scale);
        ImU32 shadowColor = isDragged ? IM_COL32(0, 0, 0, 35) : IM_COL32(0, 0, 0, 15);
        drawList->AddRectFilled(ImVec2(pMin.x + off, pMin.y + off), ImVec2(pMax.x + off, pMax.y + off), shadowColor, r);
    }
    
    ImTextureID tex = m_cardTextures[(int)card.suit][(int)card.rank - 1];
    if (tex && widthScale > 0.05f) {
        drawList->AddRectFilled(pMin, pMax, COLOR_BG_LIGHT, r); // Solid background for transparent PNGs
        float uvX0 = (1.0f - widthScale) * 0.5f;
        float uvX1 = 1.0f - uvX0;
        
        float padX = 6.0f * scale * widthScale;
        float padY = 6.0f * scale;
        ImVec2 imgMin(pMin.x + padX, pMin.y + padY);
        ImVec2 imgMax(pMax.x - padX, pMax.y - padY);
        drawList->AddImageRounded(tex, imgMin, imgMax, ImVec2(uvX0, 0), ImVec2(uvX1, 1), IM_COL32_WHITE, std::max(0.0f, r - padY));
    } else {
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
            ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, rankStr);
            drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x - tsize.x - pad, pos.y + size.y - tsize.y - 25.0f * scale), color, rankStr);
            DrawSuit(drawList, ImVec2(pos.x + size.x - 10.0f * scale, pos.y + size.y - 15.0f * scale), 8.0f * scale, card.suit, color);
            ImGui::PopFont();

            DrawSuit(drawList, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), 24.0f * scale, card.suit, color);
            
            drawList->PopClipRect();
        }
    }

    // Border
    drawList->AddRect(pMin, pMax, COLOR_BORDER, r, 0, 1.0f * scale);
    
    // Hover Glow
    if (isHovered && !isDragged) {
        drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 150, 200), r, 0, 3.0f * scale);
    }
}

void Game::DrawSuit(ImDrawList* drawList, const ImVec2& center, float size, Suit suit, ImU32 color) {
    if (suit == Suit::Hearts) {
        // Procedural heart
        float r = size * 0.5f;
        drawList->AddCircleFilled(ImVec2(center.x - r, center.y - r), r, color, 12);
        drawList->AddCircleFilled(ImVec2(center.x + r, center.y - r), r, color, 12);

        float d = std::sqrt(r * r + (size * 1.2f + r) * (size * 1.2f + r));
        float angleRight = std::atan2(size * 1.2f + r, -r) - std::acos(r / d);
        float angleLeft = std::atan2(size * 1.2f + r, r) + std::acos(r / d);

        ImVec2 p1(center.x - r + r * std::cos(angleLeft), center.y - r + r * std::sin(angleLeft));
        ImVec2 p2(center.x + r + r * std::cos(angleRight), center.y - r + r * std::sin(angleRight));
        ImVec2 p3(center.x, center.y + size * 1.2f);
        ImVec2 p4(center.x, center.y - r); // Fill gap between circles

        drawList->AddQuadFilled(p1, p4, p2, p3, color);
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

        float d = std::sqrt(r * r + (-size * 1.2f - r) * (-size * 1.2f - r));
        float angleRight = std::atan2(-size * 1.2f - r, -r) + std::acos(r / d);
        float angleLeft = std::atan2(-size * 1.2f - r, r) - std::acos(r / d);

        ImVec2 p1(center.x - r + r * std::cos(angleLeft), center.y + r + r * std::sin(angleLeft));
        ImVec2 p2(center.x + r + r * std::cos(angleRight), center.y + r + r * std::sin(angleRight));
        ImVec2 p3(center.x, center.y - size * 1.2f);
        ImVec2 p4(center.x, center.y + r); // Fill gap between circles

        drawList->AddQuadFilled(p1, p4, p2, p3, color);
        
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
    
    ImVec2 boardWinPos = winPos;
    boardWinPos.y += ImGui::GetFrameHeight();
    
    float minLogicalX = 999999.0f;
    float maxLogicalX = -999999.0f;
    for (const auto& p : m_piles) {
        if (p.type == PileType::Invisible) continue;
        float leftEdge = p.pos.x;
        float rightEdge = p.pos.x + p.size.x;
        if (p.offset.x > 0) rightEdge += 2 * p.offset.x;
        else if (p.offset.x < 0) leftEdge += 2 * p.offset.x;
        
        if (leftEdge < minLogicalX) minLogicalX = leftEdge;
        if (rightEdge > maxLogicalX) maxLogicalX = rightEdge;
    }
    if (minLogicalX > maxLogicalX) {
        minLogicalX = 0.0f;
        maxLogicalX = 1280.0f;
    }
    float logicalWidth = maxLogicalX - minLogicalX;
    float boardPixelWidth = logicalWidth * scale;
    float boardOffsetX = (ImGui::GetWindowWidth() - boardPixelWidth) * 0.5f;
    if (boardOffsetX < 0.0f) boardOffsetX = 0.0f;
    
    ImVec2 boardBasePos = boardWinPos;
    boardBasePos.x += boardOffsetX - (minLogicalX * scale);

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
            bc.pos = ImVec2(boardBasePos.x + p.pos.x * scale + pOffset.x * drawIndex,
                            boardBasePos.y + p.pos.y * scale + pOffset.y * drawIndex);
            
            // Initial velocity: random x, up y
            float vx = (rand() % 100 > 50 ? 1 : -1) * (200.0f + (rand() % 200)) * scale;
            bc.velocity = ImVec2(vx, (-200.0f - (rand() % 400)) * scale);
            
            m_bouncingCards.push_back(bc);
            m_winAnimTimer = 0.5f; // Wait before next card
        }
    }
    
    // Draw all historic trails first so they appear behind the moving cards
    ImVec2 pSize = ImVec2(CARD_SIZE.x * scale, CARD_SIZE.y * scale);
    for (const auto& trail : m_winTrails) {
        DrawCard(drawList, trail.pos, pSize, trail.card, scale, 1.0f, false);
    }

    // Update and draw bouncing cards
    float gravity = 900.0f * scale;
    float bounceLoss = 0.8f;
    
    for (auto it = m_bouncingCards.begin(); it != m_bouncingCards.end(); ) {
        BouncingCard& bc = *it;
        
        // Save trail
        m_winTrails.push_back({bc.card, bc.pos});

        bc.velocity.y += gravity * dt;
        bc.pos.x += bc.velocity.x * dt;
        bc.pos.y += bc.velocity.y * dt;
        
        // Bounce off bottom
        if (bc.pos.y + pSize.y > winPos.y + winSize.y) {
            bc.pos.y = winPos.y + winSize.y - pSize.y;
            bc.velocity.y = -bc.velocity.y * bounceLoss;
        }
        
        // If it goes completely off the sides, remove it to prevent infinite trails
        if (bc.pos.x + pSize.x < winPos.x || bc.pos.x > winPos.x + winSize.x) {
            it = m_bouncingCards.erase(it);
            continue;
        }
        
        DrawCard(drawList, bc.pos, pSize, bc.card, scale, 1.0f, false);
        ++it;
    }

    // Update and draw particles
    for (auto it = m_particles.begin(); it != m_particles.end(); ) {
        it->velocity.y += gravity * 0.5f * dt;
        it->pos.x += it->velocity.x * dt;
        it->pos.y += it->velocity.y * dt;
        it->life -= dt;
        
        if (it->pos.y > winPos.y + winSize.y) {
            it->pos.y = winPos.y + winSize.y;
            it->velocity.y *= -0.6f;
        }
        
        if (it->life <= 0.0f || it->pos.x < winPos.x || it->pos.x > winPos.x + winSize.x) {
            it = m_particles.erase(it);
            continue;
        }
        
        ImU32 col = it->color;
        int alpha = (int)(255.0f * std::min(1.0f, it->life));
        col = (col & 0x00FFFFFF) | (alpha << 24);
        drawList->AddRectFilled(it->pos, ImVec2(it->pos.x + it->size * scale, it->pos.y + it->size * scale), col);
        ++it;
    }
}

void Game::SaveStateForUndo() {
    m_undoStack.push_back(m_piles);
}

void Game::Undo() {
    if (m_undoStack.empty()) return;
    m_piles = m_undoStack.back();
    m_undoStack.pop_back();
    
    m_dragSourcePile = -1;
    m_dragCardIndex = -1;
    m_dragCards.clear();
    m_isWon = false;
    m_bouncingCards.clear();
    m_winTrails.clear();
}
