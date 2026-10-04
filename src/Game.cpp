#include "Game.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GLFW/glfw3.h> // Ensure we have GL functions

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

// Helper macros and constants
const ImU32 COLOR_BG_LIGHT = IM_COL32(250, 250, 250, 255);
const ImU32 COLOR_RED = IM_COL32(220, 50, 50, 255);
const ImU32 COLOR_BLACK = IM_COL32(30, 30, 30, 255);

constexpr float PREVIEW_WIDTH = 300.0f;
constexpr float PREVIEW_HEIGHT = 250.0f;
constexpr float PREVIEW_PADDING = 30.0f;
constexpr float SHADOW_OFFSET_NORMAL = 2.0f;
constexpr float SHADOW_OFFSET_DRAGGED = 8.0f;
constexpr float ANIM_DECAY_RATE = 15.0f;
constexpr float ANIM_FLIP_SPEED = 10.0f;
constexpr float DRAG_THRESHOLD = 4.0f;

struct GamePreview {
    std::string path;
    std::string name;
    std::vector<Pile> piles;
    bool autoCenter = true;
    ImVec2 cardSize = ImVec2(Game::DEFAULT_CARD_WIDTH, Game::DEFAULT_CARD_HEIGHT);
    float cornerRadius = Game::DEFAULT_CORNER_RADIUS;
};
static std::vector<GamePreview> s_previews;
static bool s_previews_loaded = false;
static float s_deal_delay = 0.0f;

static float s_boardScale = 1.0f;
static ImVec2 s_boardBasePos(0.0f, 0.0f);

static const size_t MAX_LUA_MEMORY = 10 * 1024 * 1024; // 10 MB memory limit

// Missing or malformed overrides leave the existing color intact. Colors use
// byte channels in Lua, matching DrawBoardText and DrawBoardPanel.
static std::optional<ImVec4> ReadThemeColor(const sol::object& object) {
    if (object.get_type() != sol::type::table) return std::nullopt;
    sol::table table = object.as<sol::table>();
    float channels[4] = {0, 0, 0, 1};
    for (int i = 0; i < 4; ++i) {
        sol::object value = table.raw_get<sol::object>(i + 1);
        if (i == 3 && value.get_type() == sol::type::nil) continue;
        if (value.get_type() != sol::type::number) return std::nullopt;
        double channel = value.as<double>();
        if (!std::isfinite(channel)) return std::nullopt;
        channels[i] = static_cast<float>(std::clamp(channel, 0.0, 255.0)) / 255.0f;
    }
    return ImVec4(channels[0], channels[1], channels[2], channels[3]);
}

class ScopedThemeStyle {
public:
    ~ScopedThemeStyle() { Clear(); }
    void Apply(const std::array<std::optional<ImVec4>, ImGuiCol_COUNT>& colors,
               std::optional<float> rounding, float scale) {
        Clear();
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            if (colors[i]) {
                ImGui::PushStyleColor(i, *colors[i]);
                ++m_colorCount;
            }
        }
        if (rounding) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, *rounding * scale);
            m_hasRounding = true;
        }
    }
private:
    int m_colorCount = 0;
    bool m_hasRounding = false;
    void Clear() {
        if (m_hasRounding) ImGui::PopStyleVar();
        if (m_colorCount) ImGui::PopStyleColor(m_colorCount);
        m_hasRounding = false;
        m_colorCount = 0;
    }
};

const Game::WindowTheme& Game::ActiveTheme() const {
    static const WindowTheme defaults;
    return m_currentScriptPath.empty() ? defaults : m_theme;
}

ImVec4 Game::GetBackgroundColor() const {
    return ImGui::ColorConvertU32ToFloat4(ActiveTheme().background);
}

void Game::LoadTheme() {
    m_theme = WindowTheme{};
    sol::object object = m_lua["Theme"];
    if (object.get_type() != sol::type::table) return;
    sol::table theme = object.as<sol::table>();
    auto color = [&](const char* key, ImU32& target) {
        auto value = ReadThemeColor(theme.raw_get<sol::object>(key));
        if (value) target = ImGui::ColorConvertFloat4ToU32(*value);
    };
    color("Background", m_theme.background);
    if (ReadThemeColor(theme.raw_get<sol::object>("Background"))) {
        m_theme.backgroundBottom = m_theme.background;
        m_theme.toolbar = m_theme.background;
    }
    color("BackgroundBottom", m_theme.backgroundBottom);
    color("Toolbar", m_theme.toolbar);
    color("EmptyPile", m_theme.emptyPile);
    color("EmptyPileBorder", m_theme.emptyPileBorder);
    color("EmptyPileText", m_theme.emptyPileText);
    color("CardBack", m_theme.cardBack);
    color("CardBorder", m_theme.cardBorder);
    color("CardHover", m_theme.cardHover);
    sol::object colors = theme.raw_get<sol::object>("Colors");
    if (colors.get_type() == sol::type::table) {
        sol::table entries = colors.as<sol::table>();
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            m_theme.colors[i] = ReadThemeColor(entries.raw_get<sol::object>(ImGui::GetStyleColorName(i)));
        }
    }
    sol::object rounding = theme.raw_get<sol::object>("ButtonRounding");
    if (rounding.get_type() == sol::type::number && std::isfinite(rounding.as<double>())) {
        m_theme.buttonRounding = static_cast<float>(std::clamp(rounding.as<double>(), 0.0, 24.0));
    }
}

static void* LuaMemoryAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    size_t* total_allocated = static_cast<size_t*>(ud);

    if (nsize == 0) {
        if (ptr != nullptr) {
            if (*total_allocated >= osize) {
                *total_allocated -= osize;
            } else {
                *total_allocated = 0; // Prevent underflow if freeing pre-allocated memory
            }
            std::free(ptr);
        }
        return nullptr;
    }

    size_t new_total = *total_allocated;
    if (ptr != nullptr) {
        if (new_total >= osize) {
            new_total -= osize;
        } else {
            new_total = 0;
        }
    }
    new_total += nsize;

    if (new_total > MAX_LUA_MEMORY) {
        std::cerr << "Lua memory limit exceeded (" << MAX_LUA_MEMORY << " bytes)!" << std::endl;
        return nullptr; // Returning nullptr triggers a Lua memory error
    }

    void* new_ptr = std::realloc(ptr, nsize);
    if (new_ptr) {
        *total_allocated = new_total;
    }
    return new_ptr;
}

Game::Game(bool loadTextures) {
    SetupLuaBindings();
    if (loadTextures) LoadCardTextures();
    LoadAvailableGames();
}

Game::~Game() {
    for (int s = 0; s < 4; ++s) {
        for (int r = 0; r < 13; ++r) {
            if (m_cardTextures[s][r]) {
                GLuint tex = (GLuint)(intptr_t)m_cardTextures[s][r];
                glDeleteTextures(1, &tex);
            }
        }
    }
    if (m_cardBackTexture) {
        GLuint tex = (GLuint)(intptr_t)m_cardBackTexture;
        glDeleteTextures(1, &tex);
    }
}

void Game::SetupLuaBindings() {
    // Initialize memory tracking based on what sol2 already allocated before we hijack the allocator
    int kb = lua_gc(m_lua.lua_state(), LUA_GCCOUNT, 0);
    int bytes = lua_gc(m_lua.lua_state(), LUA_GCCOUNTB, 0);
    m_luaAllocatedMemory = (kb * 1024) + bytes;
    
    lua_setallocf(m_lua.lua_state(), LuaMemoryAllocator, &m_luaAllocatedMemory);

    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

    // Secure the sandbox by removing dangerous base library functions
    const char* dangerous_globals[] = {
        "load", "loadstring", "loadfile", "dofile", // Fixes 7 & 12: Dynamic code eval & Bytecode
        "getfenv", "setfenv", "_G",                 // Fixes 8: Global Environment Access
        "getmetatable", "setmetatable"              // Fixes 9: Metatable Poisoning
    };
    for (const char* global : dangerous_globals) {
        m_lua[global] = sol::lua_nil;
    }

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
        "rank", sol::property([](const Card& c) { return (int)c.rank; }, [](Card& c, lua_Integer r) {
            if (r < 1 || r > 13) throw std::out_of_range("Card rank must be between 1 and 13");
            c.rank = (Rank)r;
        }),
        "suit", sol::property([](const Card& c) { return (int)c.suit; }, [](Card& c, lua_Integer s) {
            if (s < 0 || s > 3) throw std::out_of_range("Card suit must be between 0 and 3");
            c.suit = (Suit)s;
        }),
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
        "pop_back", [](std::vector<Card>& v) {
            if (v.empty()) throw std::out_of_range("Cannot pop an empty card vector");
            v.pop_back();
        },
        "take_back", [](std::vector<Card>& v) -> Card {
            if (v.empty()) throw std::out_of_range("Cannot take from an empty card vector");
            Card card = v.back();
            v.pop_back();
            return card;
        },
        "back", [](std::vector<Card>& v) -> Card& {
            if (v.empty()) throw std::out_of_range("Cannot read an empty card vector");
            return v.back();
        },
        "front", [](std::vector<Card>& v) -> Card& { return v.at(0); },
        "get", [](std::vector<Card>& v, lua_Integer i) -> Card& { return v.at(i); }
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
        "get", [](std::vector<Pile>& v, lua_Integer i) -> Pile& { return v.at(i); }
    );

    // Expose a text drawing function to Lua
    m_lua.set_function("DrawBoardText", [](float x, float y, const std::string& text,
            sol::optional<float> fontSize, sol::optional<float> wrapWidth,
            sol::optional<int> red, sol::optional<int> green, sol::optional<int> blue) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos(s_boardBasePos.x + x * s_boardScale, s_boardBasePos.y + y * s_boardScale);
        float pixels = std::clamp(fontSize.value_or(24.0f), 8.0f, 72.0f) * s_boardScale;
        float wrap = std::max(0.0f, wrapWidth.value_or(0.0f)) * s_boardScale;
        ImU32 color = IM_COL32(std::clamp(red.value_or(255), 0, 255),
            std::clamp(green.value_or(255), 0, 255), std::clamp(blue.value_or(255), 0, 255), 255);
        drawList->AddText(ImGui::GetFont(), pixels, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 150), text.c_str(), nullptr, wrap);
        drawList->AddText(ImGui::GetFont(), pixels, pos, color, text.c_str(), nullptr, wrap);
    });

    m_lua.set_function("DrawBoardPanel", [](float x, float y, float w, float h, int red, int green, int blue, sol::optional<int> alpha) {
        if (w <= 0 || h <= 0) return;
        ImVec2 min(s_boardBasePos.x + x * s_boardScale, s_boardBasePos.y + y * s_boardScale);
        ImVec2 max(min.x + w * s_boardScale, min.y + h * s_boardScale);
        ImU32 color = IM_COL32(std::clamp(red, 0, 255), std::clamp(green, 0, 255),
            std::clamp(blue, 0, 255), std::clamp(alpha.value_or(255), 0, 255));
        ImGui::GetWindowDrawList()->AddRectFilled(min, max, color, 10.0f * s_boardScale);
    });

    m_lua.set_function("DrawBoardTooltip", [](float x, float y, float w, float h, const std::string& text) {
        ImVec2 min(s_boardBasePos.x + x * s_boardScale, s_boardBasePos.y + y * s_boardScale);
        ImVec2 max(min.x + w * s_boardScale, min.y + h * s_boardScale);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    });

    m_lua.set_function("DrawBoardButton", [](float x, float y, float w, float h, const std::string& label, sol::optional<bool> enabled) {
        ImVec2 pos(s_boardBasePos.x + x * s_boardScale, s_boardBasePos.y + y * s_boardScale);
        ImGui::SetCursorScreenPos(pos);
        // Repeated labels belong to different controls. Scope their IDs by
        // logical position so identity also stays stable when the window resizes.
        const ImVec2 logicalPos(x, y);
        ImGui::PushID(static_cast<int>(ImHashData(&logicalPos, sizeof(logicalPos))));
        // FontScaleMain already includes board scaling. Fit longer labels to
        // their button instead of multiplying the font by that scale again.
        float fontSize = 22.0f;
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0], fontSize);
        const ImVec2 padding = ImGui::GetStyle().FramePadding;
        // Remeasure after resizing: hinted glyph advances and ImGui's rounded
        // pixel sizes do not scale exactly in proportion to the requested size.
        for (int attempt = 0; attempt < 4; ++attempt) {
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str(), nullptr, true);
            float fit = std::min(
                std::max(1.0f, w * s_boardScale - 2 * padding.x - 2 * s_boardScale) / std::max(1.0f, textSize.x),
                std::max(1.0f, h * s_boardScale - 2 * padding.y) / std::max(1.0f, textSize.y));
            if (fit >= 1.0f) break;
            fontSize *= std::min(fit, 0.95f);
            ImGui::PopFont();
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0], fontSize);
        }
        ImGui::BeginDisabled(!enabled.value_or(true));
        bool clicked = ImGui::Button(label.c_str(), ImVec2(w * s_boardScale, h * s_boardScale));
        ImGui::EndDisabled();
        ImGui::PopFont();
        ImGui::PopID();
        return clicked;
    });

    // Run button actions after Draw returns so snapshots never replace a board
    // while the script is still drawing it.
    m_lua.set_function("PerformAction", [this](const std::string& action) {
        if (m_pendingAction.empty() && !m_isWon) m_pendingAction = action;
    });
    m_lua.set_function("EmitBoardParticles", [this](float x, float y, float w, float h) {
        if (!ImGui::GetCurrentContext() || !ImGui::GetCurrentWindowRead()) return;
        ImVec2 center(s_boardBasePos.x + x * s_boardScale, s_boardBasePos.y + y * s_boardScale);
        SpawnActionParticles(center, ImVec2(w * s_boardScale, h * s_boardScale), s_boardScale);
    });

    m_lua.set_function("GetScore", [this]() { return m_score; });
    m_lua.set_function("AddScore", [this](int points) { m_score += points; });
    m_lua.set_function("SetScore", [this](int points) { m_score = points; });
    m_lua.set_function("GetTime", [this]() { return (double)m_gameTime; });
}

void Game::CreateDeck(std::vector<Card>& deck, lua_Integer numDecks) {
    if (numDecks < 1 || numDecks > 8) {
        throw std::out_of_range("NumDecks must be between 1 and 8");
    }
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
    std::sort(m_availableGames.begin(), m_availableGames.end(), [](const std::string& a, const std::string& b) {
        return std::filesystem::path(a).stem().string() < std::filesystem::path(b).stem().string();
    });
}

void Game::LoadCardTextures() {
    const char* suits[] = { "hearts", "diamonds", "clubs", "spades" };
    const char* ranks[] = { "ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen", "king" };

    // Since main.cpp sets the working directory to the executable's location,
    // we can reliably just use the local folders.
    std::string baseDir = "cards/";
    if (!std::filesystem::exists(baseDir) && std::filesystem::exists("../cards")) {
        baseDir = "../cards/"; // Optional fallback just in case
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
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); // Auto-generate mipmaps for high-res downscaling
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
    m_currentScriptPath = scriptPath;
    m_pendingAction.clear();
    m_piles.clear();
    m_dragSourcePile = -1;
    m_dragCardIndex = -1;
    m_dragCards.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_isWon = false;
    m_particleSystem.Clear();
    m_gameTime = 0.0f;
    m_score = 0;

    m_cardSize = ImVec2(DEFAULT_CARD_WIDTH, DEFAULT_CARD_HEIGHT);
    m_cornerRadius = DEFAULT_CORNER_RADIUS;
    m_theme = WindowTheme{};
    s_previews.clear();
    s_previews_loaded = false;

    try {
        // Clear Lua globals to prevent leaking state between games
        m_lua["GameName"] = sol::lua_nil;
        m_lua["HelpText"] = sol::lua_nil;
        m_lua["NumDecks"] = sol::lua_nil;
        m_lua["AutoCenter"] = sol::lua_nil;
        m_lua["Init"] = sol::lua_nil;
        m_lua["CardSize"] = sol::lua_nil;
        m_lua["CornerRadius"] = sol::lua_nil;
        m_lua["Theme"] = sol::lua_nil;
        m_lua["CanPickup"] = sol::lua_nil;
        m_lua["CanDrop"] = sol::lua_nil;
        m_lua["AfterMove"] = sol::lua_nil;
        m_lua["HandleClick"] = sol::lua_nil;
        m_lua["AutoSolve"] = sol::lua_nil;
        m_lua["IsWon"] = sol::lua_nil;
        m_lua["Draw"] = sol::lua_nil;
        m_lua["DrawBackground"] = sol::lua_nil;
        m_lua["HandleAction"] = sol::lua_nil;
        m_lua["SaveState"] = sol::lua_nil;
        m_lua["LoadState"] = sol::lua_nil;

        lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
        m_lua.script_file(m_currentScriptPath, sol::load_mode::text);
        m_currentGameName = m_lua["GameName"].get_or<std::string>("Unknown Game");
        m_currentHelpText = m_lua["HelpText"].get_or<std::string>("No help available.");
        sol::optional<ImVec2> cardSizeOpt = m_lua["CardSize"];
        m_cardSize = cardSizeOpt ? *cardSizeOpt : ImVec2(DEFAULT_CARD_WIDTH, DEFAULT_CARD_HEIGHT);
        m_cornerRadius = m_lua["CornerRadius"].get_or(DEFAULT_CORNER_RADIUS);

        std::vector<Card> deck;
        lua_Integer numDecks = m_lua["NumDecks"].get_or<lua_Integer>(1);
        CreateDeck(deck, numDecks);
        ShuffleDeck(deck);

        sol::protected_function initFunc = m_lua["Init"];
        if (initFunc.valid()) {
            sol::protected_function_result result = initFunc(m_piles, deck);
            if (!result.valid()) { sol::error err = result; throw err; }
        }
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        LoadTheme();
    } catch (const std::exception& e) {
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        std::cerr << "Lua Error during InitGame: " << e.what() << std::endl;
        m_piles.clear();
        m_currentScriptPath.clear();
        m_theme = WindowTheme{};
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
        try {
            lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
            sol::protected_function_result result = canPickup(m_piles, pileIdx, cardIdx);
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            if (result.valid() && result.get_type() == sol::type::boolean) {
                return result.get<bool>();
            } else if (!result.valid()) {
                sol::error err = result; throw err;
            }
        } catch (const sol::error& e) {
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            std::cerr << "Lua Error in CanPickup: " << e.what() << std::endl;
        }
    }
    return false;
}

bool Game::CanDrop(int sourcePileIdx, const std::vector<Card>& cards, int targetPileIdx) {
    if (targetPileIdx < 0 || targetPileIdx >= m_piles.size() || cards.empty()) return false;
    if (sourcePileIdx == targetPileIdx) return false;
    
    sol::protected_function canDrop = m_lua["CanDrop"];
    if (canDrop.valid()) {
        try {
            lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
            sol::protected_function_result result = canDrop(m_piles, sourcePileIdx, targetPileIdx, cards);
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            if (result.valid() && result.get_type() == sol::type::boolean) {
                return result.get<bool>();
            } else if (!result.valid()) {
                sol::error err = result; throw err;
            }
        } catch (const sol::error& e) {
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            std::cerr << "Lua Error in CanDrop: " << e.what() << std::endl;
        }
    }
    return false;
}

void Game::DoMove(int sourcePileIdx, int targetPileIdx, int cardIdx) {
    if (sourcePileIdx < 0 || sourcePileIdx >= (int)m_piles.size() ||
        targetPileIdx < 0 || targetPileIdx >= (int)m_piles.size() ||
        sourcePileIdx == targetPileIdx || cardIdx < 0 ||
        cardIdx >= (int)m_piles[sourcePileIdx].cards.size()) return;

    Pile& sp = m_piles[sourcePileIdx];
    Pile& tp = m_piles[targetPileIdx];
    
    // Move cards
    tp.cards.insert(tp.cards.end(), sp.cards.begin() + cardIdx, sp.cards.end());
    sp.cards.erase(sp.cards.begin() + cardIdx, sp.cards.end());

    sol::protected_function afterMove = m_lua["AfterMove"];
    if (afterMove.valid()) {
        try {
            lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
            sol::protected_function_result result = afterMove(m_piles, sourcePileIdx, targetPileIdx, cardIdx);
            if (!result.valid()) { sol::error err = result; throw err; }
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        } catch (const sol::error& e) {
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            std::cerr << "Lua Error in AfterMove: " << e.what() << std::endl;
        }
    }
}

void Game::HandleClick(int pileIdx) {
    if (pileIdx < 0 || pileIdx >= (int)m_piles.size()) return;
    HandleScriptAction("HandleClick", sol::make_object(m_lua, pileIdx));
}

void Game::HandleAction(const std::string& action) {
    HandleScriptAction("HandleAction", sol::make_object(m_lua, action));
}

void Game::HandleScriptAction(const char* callback, const sol::object& argument) {
    sol::protected_function function = m_lua[callback];
    if (!function.valid()) return;
    SavedState backup = CaptureState();
    try {
        lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug*) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
        sol::protected_function_result result = function(m_piles, argument);
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        if (!result.valid()) { sol::error error = result; throw error; }
    } catch (const sol::error& error) {
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        std::cerr << "Lua Error in " << callback << ": " << error.what() << std::endl;
        RestoreState(std::move(backup));
        return;
    }
    if (StateChanged(backup)) {
        m_undoStack.push_back(std::move(backup));
        m_redoStack.clear();
    }
}

void Game::DrawScriptLayer(const char* callback) {
    sol::protected_function function = m_lua[callback];
    if (!function.valid()) return;
    try {
        lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug*) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
        sol::protected_function_result result = function();
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        if (!result.valid()) { sol::error error = result; throw error; }
    } catch (const sol::error& error) {
        lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
        std::cerr << "Lua Error in " << callback << ": " << error.what() << std::endl;
    }
}

void Game::RenderMenuBar() {
    bool showHelp = false;
    bool doUndo = false;
    bool doRedo = false;
    bool hasGame = !m_currentScriptPath.empty();

    if (hasGame) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyCtrl) {
            if (ImGui::GetIO().KeyShift) doRedo = true;
            else doUndo = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && ImGui::GetIO().KeyCtrl) {
            doRedo = true;
        }
        if (ImGui::IsMouseClicked(3)) { // Mouse Backward Button
            doUndo = true;
        }
        if (ImGui::IsMouseClicked(4)) { // Mouse Forward Button
            doRedo = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            InitGame(m_currentScriptPath);
        }
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("Return to Start Screen", NULL, false, hasGame)) {
                m_currentScriptPath.clear();
            }
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, hasGame && !m_undoStack.empty())) doUndo = true;
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, hasGame && !m_redoStack.empty())) doRedo = true;
            if (ImGui::MenuItem("Restart Game", "F2", false, hasGame)) InitGame(m_currentScriptPath);
            ImGui::Separator();
            if (ImGui::MenuItem("Refresh Game List")) {
                LoadAvailableGames();
                s_previews.clear();
                s_previews_loaded = false;
            }
            ImGui::Separator();
            for (const auto& gamePath : m_availableGames) {
                std::string displayName = std::filesystem::path(gamePath).stem().string();
                if (ImGui::MenuItem(("Play " + displayName).c_str())) {
                    InitGame(gamePath);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem(hasGame ? ("How to play " + m_currentGameName).c_str() : "How to play", NULL, false, hasGame)) showHelp = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (showHelp) ImGui::OpenPopup("Help");

    // Process undo/redo
    if (doUndo) {
        Undo();
    }
    if (doRedo) {
        Redo();
    }

    if (ImGui::BeginPopupModal("Help", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", m_currentGameName.c_str());
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(m_currentHelpText.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

void Game::RenderStartScreen(ImDrawList* drawList, float scale) {
    if (!s_previews_loaded) {
        s_previews.clear(); // Ensure we don't duplicate previews when refreshing
        for (const auto& path : m_availableGames) {
            GamePreview p;
            p.path = path;
            try {
                // Prevent leaking state from previously evaluated scripts
                m_lua["GameName"] = sol::lua_nil;
                m_lua["AutoCenter"] = sol::lua_nil;
                m_lua["NumDecks"] = sol::lua_nil;
                m_lua["Init"] = sol::lua_nil;
                m_lua["CardSize"] = sol::lua_nil;
                m_lua["CornerRadius"] = sol::lua_nil;
                m_lua["Theme"] = sol::lua_nil;

                lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
                m_lua.script_file(path, sol::load_mode::text);
                p.name = m_lua["GameName"].get_or<std::string>("Unknown");
                p.autoCenter = m_lua["AutoCenter"].get_or(true);
                std::vector<Card> deck;
                CreateDeck(deck, m_lua["NumDecks"].get_or<lua_Integer>(1));
                ShuffleDeck(deck);
                sol::protected_function initFunc = m_lua["Init"];
                if (initFunc.valid()) {
                    sol::protected_function_result result = initFunc(p.piles, deck);
                    if (!result.valid()) { sol::error err = result; throw err; }
                }
            sol::optional<ImVec2> previewCardSizeOpt = m_lua["CardSize"];
        p.cardSize = previewCardSizeOpt ? *previewCardSizeOpt : ImVec2(DEFAULT_CARD_WIDTH, DEFAULT_CARD_HEIGHT);
            p.cornerRadius = m_lua["CornerRadius"].get_or(DEFAULT_CORNER_RADIUS);
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                s_previews.push_back(p);
            } catch (const std::exception& e) {
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                std::cerr << "Lua Error loading preview for " << path << ": " << e.what() << std::endl;
            } catch (...) {
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                continue;
            }
        }
        std::sort(s_previews.begin(), s_previews.end(), [](const GamePreview& a, const GamePreview& b) {
            return a.name < b.name;
        });
        s_previews_loaded = true;
        s_deal_delay = 0.5f; // Wait half a second before dealing
    }

    float window_width = ImGui::GetWindowWidth();

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::SetWindowFontScale(2.5f * scale);
    float text_width = ImGui::CalcTextSize("Select a Game").x;
    ImGui::SetCursorPos(ImVec2((window_width - text_width) * 0.5f, 0.0f * scale + ImGui::GetFrameHeight()));
    ImGui::TextColored(ImVec4(1, 1, 1, 1), "Select a Game");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();

    float preview_width = PREVIEW_WIDTH * scale;
    float preview_height = PREVIEW_HEIGHT * scale;
    float padding = PREVIEW_PADDING * scale;
    int columns = std::max(1, (int)((window_width - padding) / (preview_width + padding)));
    int actual_columns = std::min((int)s_previews.size(), columns);
    float grid_width = actual_columns * preview_width + std::max(0, actual_columns - 1) * padding;
    float start_x = std::max(0.0f, (window_width - grid_width) * 0.5f);

    float dt = ImGui::GetIO().DeltaTime;
    float decayRate = ANIM_DECAY_RATE;
    float expDecay = std::exp(-decayRate * dt);
    if (s_deal_delay > 0.0f) s_deal_delay -= dt;

    ImGui::SetCursorPos(ImVec2(start_x, 80.0f * scale + ImGui::GetFrameHeight()));
    int col = 0;
    for (auto& preview : s_previews) {
        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImVec2 screenPos = ImGui::GetCursorScreenPos();

        ImGui::PushID(preview.path.c_str());
        if (ImGui::InvisibleButton("##gamebtn", ImVec2(preview_width, preview_height))) {
            InitGame(preview.path);
            ImGui::PopID();
            break;
        }

        bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            drawList->AddRectFilled(screenPos, ImVec2(screenPos.x + preview_width, screenPos.y + preview_height), IM_COL32(255, 255, 255, 40), 12.0f);
            drawList->AddRect(screenPos, ImVec2(screenPos.x + preview_width, screenPos.y + preview_height), IM_COL32(255, 255, 255, 200), 12.0f, 0, 3.0f);
        } else {
            drawList->AddRectFilled(screenPos, ImVec2(screenPos.x + preview_width, screenPos.y + preview_height), IM_COL32(0, 0, 0, 80), 12.0f);
            drawList->AddRect(screenPos, ImVec2(screenPos.x + preview_width, screenPos.y + preview_height), IM_COL32(255, 255, 255, 100), 12.0f, 0, 1.0f);
        }

        float mini_scale = scale * 0.22f;
        float minLogX = 999999.0f, maxLogX = -999999.0f;
        float minLogY = 999999.0f, maxLogY = -999999.0f;
        for (const auto& p : preview.piles) {
            if (p.type == PileType::Invisible) continue;
            int maxDrawIndex = 0;
            if (!p.cards.empty()) {
                maxDrawIndex = (int)p.cards.size() - 1;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    maxDrawIndex = std::max(0, maxDrawIndex - (int)(p.cards.size() - 3));
                }
            }
            float leftEdge = p.pos.x;
            float rightEdge = p.pos.x + p.size.x;
            if (p.offset.x > 0) rightEdge += p.offset.x * maxDrawIndex;
            else if (p.offset.x < 0) leftEdge += p.offset.x * maxDrawIndex;
            
            float topEdge = p.pos.y;
            float bottomEdge = p.pos.y + p.size.y;
            if (p.offset.y > 0) bottomEdge += p.offset.y * maxDrawIndex;
            else if (p.offset.y < 0) topEdge += p.offset.y * maxDrawIndex;
            
            if (leftEdge < minLogX) minLogX = leftEdge;
            if (rightEdge > maxLogX) maxLogX = rightEdge;
            if (topEdge < minLogY) minLogY = topEdge;
            if (bottomEdge > maxLogY) maxLogY = bottomEdge;
        }
        if (minLogX > maxLogX) { minLogX = 0.0f; maxLogX = 800.0f; minLogY = 0.0f; maxLogY = 600.0f; }
        
        float contentW = (maxLogX - minLogX) * mini_scale;
        float contentH = (maxLogY - minLogY) * mini_scale;
        float bOffsetX = (preview_width - contentW) * 0.5f;
        float availH = preview_height - 40.0f * scale;
        float bOffsetY = 40.0f * scale + std::max(0.0f, (availH - contentH) * 0.5f);

        ImVec2 boardOffset;
        if (preview.autoCenter) {
            boardOffset = ImVec2(screenPos.x + bOffsetX - minLogX * mini_scale, screenPos.y + bOffsetY - minLogY * mini_scale);
        } else {
            boardOffset = ImVec2(screenPos.x, screenPos.y + 40.0f * scale);
        }

        drawList->PushClipRect(screenPos, ImVec2(screenPos.x + preview_width, screenPos.y + preview_height), true);

        int cardsInitializedThisFrame = 0;
        for (auto& p : preview.piles) {
            ImVec2 pPos = ImVec2(boardOffset.x + p.pos.x * mini_scale, boardOffset.y + p.pos.y * mini_scale);
            ImVec2 pSize = ImVec2(p.size.x * mini_scale, p.size.y * mini_scale);
            DrawEmptyPile(drawList, pPos, pSize, mini_scale, p.type, preview.cornerRadius);

            int cCount = 0;
            for (auto& c : p.cards) {
                int drawIndex = cCount;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    drawIndex = std::max(0, cCount - (int)(p.cards.size() - 3));
                }
                ImVec2 cPos = ImVec2(pPos.x + p.offset.x * mini_scale * drawIndex, pPos.y + p.offset.y * mini_scale * drawIndex);
                if (!c.hasInitializedPos) {
                    if (s_deal_delay <= 0.0f && cardsInitializedThisFrame < 2) {
                        ImVec2 startPos = ImVec2(screenPos.x + preview_width * 0.5f, screenPos.y + preview_height + 50.0f * scale);
                        for (const auto& sp : preview.piles) {
                            if (sp.type == PileType::Stock) {
                                int drawIdx = sp.cards.empty() ? 0 : (int)sp.cards.size() - 1;
                                startPos = ImVec2(boardOffset.x + (sp.pos.x + sp.offset.x * drawIdx) * mini_scale, boardOffset.y + (sp.pos.y + sp.offset.y * drawIdx) * mini_scale);
                                break;
                            }
                        }
                        c.animPos = ImVec2(startPos.x - screenPos.x, startPos.y - screenPos.y);
                        c.hasInitializedPos = true;
                        cardsInitializedThisFrame++;
                    } else {
                        continue;
                    }
                } else {
                    ImVec2 targetRel = ImVec2(cPos.x - screenPos.x, cPos.y - screenPos.y);
                c.animPos.x = targetRel.x + (c.animPos.x - targetRel.x) * expDecay;
                c.animPos.y = targetRel.y + (c.animPos.y - targetRel.y) * expDecay;
                }

                ImVec2 drawPos = ImVec2(screenPos.x + c.animPos.x, screenPos.y + c.animPos.y);
                if (c.faceUp) {
                DrawCard(drawList, drawPos, pSize, c, mini_scale, preview.cornerRadius, 1.0f, false, false);
                } else {
                DrawCardBack(drawList, drawPos, pSize, mini_scale, preview.cornerRadius, 1.0f, false);
                }
                cCount++;
            }
        }

        drawList->PopClipRect();

        // Draw title text on top with a black outline
        float outline = 1.5f * scale;
        float tSize = ImGui::GetFontSize() * 1.5f * scale;
        float tWrap = preview_width - 30.0f * scale;
        ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(tSize, FLT_MAX, tWrap, preview.name.c_str());
        ImVec2 tPos = ImVec2(screenPos.x + (preview_width - textSize.x) * 0.5f, screenPos.y + 15.0f * scale);
        ImU32 outlineCol = IM_COL32(0, 0, 0, 255);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x - outline, tPos.y - outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x + outline, tPos.y - outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x - outline, tPos.y + outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x + outline, tPos.y + outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x - outline, tPos.y), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x + outline, tPos.y), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x, tPos.y - outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, ImVec2(tPos.x, tPos.y + outline), outlineCol, preview.name.c_str(), NULL, tWrap);
        drawList->AddText(ImGui::GetFont(), tSize, tPos, IM_COL32_WHITE, preview.name.c_str(), NULL, tWrap);

        ImGui::PopID();

        col++;
        if (col < columns) {
            ImGui::SetCursorPos(ImVec2(cursorPos.x + preview_width + padding, cursorPos.y));
        } else {
            col = 0;
            ImGui::SetCursorPos(ImVec2(start_x, cursorPos.y + preview_height + padding));
        }
    }
    
    if (col != 0) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + preview_height);
    }
    ImGui::Dummy(ImVec2(0.0f, 50.0f * scale));
}

void Game::RenderInGameMenu(float scale) {
    ImGui::SetCursorPos(ImVec2(10.0f * scale, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ActiveTheme().colors[ImGuiCol_HeaderHovered]
        .value_or(ImVec4(1, 1, 1, 50.0f / 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveTheme().colors[ImGuiCol_HeaderActive]
        .value_or(ImVec4(1, 1, 1, 100.0f / 255)));
    
    if (ImGui::ArrowButton("##BackToStart", ImGuiDir_Left)) {
        m_currentScriptPath.clear();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Return to Start Screen");

    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        InitGame(m_currentScriptPath);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restart Game");

    ImGui::PopStyleColor(3);
}

void Game::ProcessInput(float scale, const ImVec2& boardBasePos, int& outHoveredPile, int& outHoveredCard) {
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool mouseReleased = ImGui::IsMouseReleased(0);
    bool rightClicked = ImGui::IsMouseClicked(1);

    outHoveredPile = -1;
    outHoveredCard = -1;

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
                        outHoveredPile = (int)i;
                        outHoveredCard = -1;
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
                        outHoveredPile = (int)i;
                        outHoveredCard = (int)c;
                    }
                }
            }
        }
    }

    // Dropping Dragged Cards
    if (mouseReleased && m_dragSourcePile != -1) {
        ImVec2 dragBasePos = ImVec2(mousePos.x - m_dragOffset.x, mousePos.y - m_dragOffset.y);
        ImVec2 dragCenter = ImVec2(dragBasePos.x + m_cardSize.x * scale * 0.5f, dragBasePos.y + m_cardSize.y * scale * 0.5f);
        
        int bestDropPile = -1;
        float bestDistSq = 9999999.0f;
        bool isClick = !ImGui::IsMouseDragPastThreshold(0, DRAG_THRESHOLD * scale);
        
        if (!isClick && ImGui::IsWindowHovered()) {
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
                ImVec2 targetCenter = ImVec2(targetPos.x + m_cardSize.x * scale * 0.5f, targetPos.y + m_cardSize.y * scale * 0.5f);
                
                float dx = dragCenter.x - targetCenter.x;
                float dy = dragCenter.y - targetCenter.y;
                float distSq = dx * dx + dy * dy;
                
                float maxDist = m_cardSize.x * scale * 1.5f; // Forgiving distance
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
            
            Pile& tp = m_piles[bestDropPile];
            int tCount = tp.cards.empty() ? 0 : (int)tp.cards.size() - 1;
            int tDrawIndex = tCount;
            if (tp.type == PileType::Waste && tp.cards.size() > 3) {
                tDrawIndex = std::max(0, tCount - (int)(tp.cards.size() - 3));
            }
            ImVec2 targetPos = ImVec2(boardBasePos.x + tp.pos.x * scale + tp.offset.x * scale * tDrawIndex, 
                                      boardBasePos.y + tp.pos.y * scale + tp.offset.y * scale * tDrawIndex);
            ImVec2 targetSize = ImVec2(m_cardSize.x * scale, m_cardSize.y * scale);
            ImVec2 targetCenter = ImVec2(targetPos.x + targetSize.x * 0.5f, targetPos.y + targetSize.y * 0.5f);
            SpawnActionParticles(targetCenter, targetSize, scale);
        } else {
            Pile& sp = m_piles[m_dragSourcePile];
            for (size_t i = 0; i < m_dragCards.size(); ++i) {
                sp.cards[m_dragCardIndex + i].animPos = m_dragCards[i].animPos;
            }
            
            if (isClick) {
                HandleClick(m_dragSourcePile);
            }
        }
        
        m_dragSourcePile = -1;
        m_dragCardIndex = -1;
        m_dragCards.clear();
    }

    // Right-click auto-move
    if (rightClicked && outHoveredPile != -1 && outHoveredCard != -1 && m_dragSourcePile == -1) {
        if (CanPickup(outHoveredPile, outHoveredCard)) {
            std::vector<Card> stack(m_piles[outHoveredPile].cards.begin() + outHoveredCard, m_piles[outHoveredPile].cards.end());
            int bestDrop = -1;
            for (size_t i = 0; i < m_piles.size(); ++i) {
                if (m_piles[i].type == PileType::Foundation && CanDrop(outHoveredPile, stack, (int)i)) {
                    bestDrop = (int)i; break;
                }
            }
            if (bestDrop == -1) {
                for (size_t i = 0; i < m_piles.size(); ++i) {
                    if ((m_piles[i].type == PileType::Tableau || m_piles[i].type == PileType::FreeCellSlot) && CanDrop(outHoveredPile, stack, (int)i)) {
                        bestDrop = (int)i; break;
                    }
                }
            }
            if (bestDrop != -1) {
                SaveStateForUndo();
                DoMove(outHoveredPile, bestDrop, outHoveredCard);
                
                Pile& tp = m_piles[bestDrop];
                int tCount = tp.cards.empty() ? 0 : (int)tp.cards.size() - 1;
                int tDrawIndex = tCount;
                if (tp.type == PileType::Waste && tp.cards.size() > 3) {
                    tDrawIndex = std::max(0, tCount - (int)(tp.cards.size() - 3));
                }
                ImVec2 targetPos = ImVec2(boardBasePos.x + tp.pos.x * scale + tp.offset.x * scale * tDrawIndex, 
                                          boardBasePos.y + tp.pos.y * scale + tp.offset.y * scale * tDrawIndex);
                ImVec2 targetSize = ImVec2(m_cardSize.x * scale, m_cardSize.y * scale);
                ImVec2 targetCenter = ImVec2(targetPos.x + targetSize.x * 0.5f, targetPos.y + targetSize.y * 0.5f);
                SpawnActionParticles(targetCenter, targetSize, scale);
            }
        }
    }
    // Single Click / Start Drag
    else if (mouseClicked && outHoveredPile != -1) {
        if (m_piles[outHoveredPile].type == PileType::Stock) {
            HandleClick(outHoveredPile);
        } else if (outHoveredCard != -1) {
            if (CanPickup(outHoveredPile, outHoveredCard)) {
                m_dragSourcePile = outHoveredPile;
                m_dragCardIndex = outHoveredCard;
                Pile& p = m_piles[outHoveredPile];
                m_dragCards.assign(p.cards.begin() + outHoveredCard, p.cards.end());
                
                ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);
                int drawIndex = outHoveredCard;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    drawIndex = std::max(0, (int)outHoveredCard - (int)(p.cards.size() - 3));
                }
                ImVec2 cardPos = ImVec2(boardBasePos.x + p.pos.x * scale + pOffset.x * drawIndex, 
                                        boardBasePos.y + p.pos.y * scale + pOffset.y * drawIndex);
                m_dragOffset = ImVec2(mousePos.x - cardPos.x, mousePos.y - cardPos.y);
            } else {
                HandleClick(outHoveredPile);
            }
        } else if (m_piles[outHoveredPile].cards.empty()) {
            HandleClick(outHoveredPile);
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
}

void Game::ProcessAutoSolve() {
    if (m_dragSourcePile == -1) {
        sol::protected_function autoSolve = m_lua["AutoSolve"];
        if (autoSolve.valid()) {
            try {
                lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
                sol::protected_function_result result = autoSolve(m_piles);
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                
                if (result.valid() && result.get_type() == sol::type::table) {
                    sol::table move = result;
                    if (!move.empty()) {
                        // Lua rules may return incomplete or invalid moves.
                        sol::object srcValue = move[1];
                        sol::object dstValue = move[2];
                        sol::object idxValue = move[3];
                        if (!srcValue.is<lua_Integer>() || !dstValue.is<lua_Integer>() || !idxValue.is<lua_Integer>()) return;
                        lua_Integer src = srcValue.as<lua_Integer>();
                        lua_Integer dst = dstValue.as<lua_Integer>();
                        lua_Integer idx = idxValue.as<lua_Integer>();
                        if (src < 0 || src >= (lua_Integer)m_piles.size() ||
                            dst < 0 || dst >= (lua_Integer)m_piles.size() || src == dst ||
                            idx < 0 || idx >= (lua_Integer)m_piles[src].cards.size()) return;
                        if (!CanPickup(src, idx)) return;
                        std::vector<Card> cards(m_piles[src].cards.begin() + idx, m_piles[src].cards.end());
                        if (!CanDrop(src, cards, dst)) return;
                        // Do not SaveStateForUndo() here to avoid flooding the undo stack with single auto-moves
                        DoMove(src, dst, idx);
                        m_redoStack.clear();
                    }
                } else if (!result.valid()) {
                    sol::error err = result; throw err;
                }
            } catch (const sol::error& e) {
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                std::cerr << "Lua Error in AutoSolve: " << e.what() << std::endl;
            }
        }
    }
}

bool Game::RenderBoard(ImDrawList* drawList, float scale, const ImVec2& boardBasePos, int hoveredPile, int hoveredCard) {
    bool cardsAnimating = false;
    ImVec2 mousePos = ImGui::GetMousePos();
    int cardsInitializedThisFrame = 0;
    
    // Draw in correct order, from bottom to top
    float dt = ImGui::GetIO().DeltaTime;
    float decayRate = ANIM_DECAY_RATE;
    float expDecay = std::exp(-decayRate * dt);
    float flipSpeed = ANIM_FLIP_SPEED * dt;

    struct AnimCard {
        Card* card;
        ImVec2 size;
        bool isHovered;
    };
    std::vector<AnimCard> deferredCards;

    for (size_t i = 0; i < m_piles.size(); ++i) {
        Pile& p = m_piles[i];
        ImVec2 basePos = ImVec2(boardBasePos.x + p.pos.x * scale, boardBasePos.y + p.pos.y * scale);
        ImVec2 pSize = ImVec2(p.size.x * scale, p.size.y * scale);
        ImVec2 pOffset = ImVec2(p.offset.x * scale, p.offset.y * scale);

        if (p.cards.empty()) {
            DrawEmptyPile(drawList, basePos, pSize, scale, p.type, m_cornerRadius);
        } else {
            bool deferRemaining = false;
            for (size_t c = 0; c < p.cards.size(); ++c) {
                if (m_dragSourcePile == (int)i && (int)c >= m_dragCardIndex) continue;

                int drawIndex = (int)c;
                if (p.type == PileType::Waste && p.cards.size() > 3) {
                    drawIndex = std::max(0, (int)c - (int)(p.cards.size() - 3));
                }

                ImVec2 cardPos = ImVec2(basePos.x + pOffset.x * drawIndex, basePos.y + pOffset.y * drawIndex);
                Card& cardRef = p.cards[c];

                if (!cardRef.hasInitializedPos) {
                    if (cardsInitializedThisFrame < 2) {
                        ImVec2 startPos = boardBasePos;
                        for (const auto& sp : m_piles) {
                            if (sp.type == PileType::Stock) {
                                int drawIdx = sp.cards.empty() ? 0 : (int)sp.cards.size() - 1;
                                startPos = ImVec2(boardBasePos.x + (sp.pos.x + sp.offset.x * drawIdx) * scale, boardBasePos.y + (sp.pos.y + sp.offset.y * drawIdx) * scale);
                                break;
                            }
                        }
                        cardRef.animPos = startPos;
                        cardRef.hasInitializedPos = true;
                        cardRef.flipVisual = -1.0f;
                        cardsInitializedThisFrame++;
                    } else {
                        continue;
                    }
                } else {
                    cardRef.animPos.x = cardPos.x + (cardRef.animPos.x - cardPos.x) * expDecay;
                    cardRef.animPos.y = cardPos.y + (cardRef.animPos.y - cardPos.y) * expDecay;

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

                bool isMoving = (std::abs(cardRef.animPos.x - cardPos.x) > 1.0f || std::abs(cardRef.animPos.y - cardPos.y) > 1.0f);
                if (isMoving) {
                    deferRemaining = true;
                }
                
                if (deferRemaining) {
                    deferredCards.push_back({&cardRef, pSize, hoveredPile == (int)i && hoveredCard == (int)c});
                } else {
                    if (cardRef.flipVisual > 0.0f) {
                        DrawCard(drawList, cardRef.animPos, pSize, cardRef, scale, m_cornerRadius, cardRef.flipVisual, false, hoveredPile == (int)i && hoveredCard == (int)c);
                    } else {
                        DrawCardBack(drawList, cardRef.animPos, pSize, scale, m_cornerRadius, -cardRef.flipVisual, false);
                    }
                }
            }
        }
    }

    for (const auto& dc : deferredCards) {
        if (dc.card->flipVisual > 0.0f) {
            DrawCard(drawList, dc.card->animPos, dc.size, *dc.card, scale, m_cornerRadius, dc.card->flipVisual, false, dc.isHovered);
        } else {
            DrawCardBack(drawList, dc.card->animPos, dc.size, scale, m_cornerRadius, -dc.card->flipVisual, false);
        }
    }

    if (m_dragSourcePile != -1 && !m_dragCards.empty()) {
        ImVec2 dragBasePos = ImVec2(mousePos.x - m_dragOffset.x, mousePos.y - m_dragOffset.y);
        ImVec2 pSize = ImVec2(m_cardSize.x * scale, m_cardSize.y * scale);
        ImVec2 pOffset = ImVec2(m_piles[m_dragSourcePile].offset.x * scale, m_piles[m_dragSourcePile].offset.y * scale);

        ImVec2 pullOffset(0, 0);
        ImVec2 dragCenter = ImVec2(dragBasePos.x + pSize.x * 0.5f, dragBasePos.y + pSize.y * 0.5f);
        float maxDist = pSize.x * 1.8f;

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
                float strength = std::pow((maxDist - dist) / maxDist, 2.0f);
                
                if (canDrop) {
                    pullOffset.x += dx * strength * 0.6f;
                    pullOffset.y += dy * strength * 0.6f;
                } else {
                    pullOffset.x -= (dx / dist) * strength * pSize.x * 0.05f;
                    pullOffset.y -= (dy / dist) * strength * pSize.x * 0.05f;
                }
            }
        }
        
        dragBasePos.x += pullOffset.x;
        dragBasePos.y += pullOffset.y;

        float minX = dragBasePos.x;
        float maxX = dragBasePos.x + pSize.x;
        float minY = dragBasePos.y;
        float maxY = dragBasePos.y + pSize.y;

        for (size_t c = 1; c < m_dragCards.size(); ++c) {
            ImVec2 swayOffset(pullOffset.x * 0.08f * c, pullOffset.y * 0.08f * c);
            float cx = dragBasePos.x + pOffset.x * c + swayOffset.x;
            float cy = dragBasePos.y + pOffset.y * c + swayOffset.y;
            if (cx < minX) minX = cx;
            if (cx + pSize.x > maxX) maxX = cx + pSize.x;
            if (cy < minY) minY = cy;
            if (cy + pSize.y > maxY) maxY = cy + pSize.y;
        }

        ImVec2 stackSize = ImVec2(maxX - minX, maxY - minY);
        ImVec2 stackCenter = ImVec2(minX + stackSize.x * 0.5f, minY + stackSize.y * 0.5f);
        if (ImGui::IsMouseDragPastThreshold(0, DRAG_THRESHOLD * scale)) {
            m_particleSystem.EmitTrail(stackCenter, stackSize,
                {ImGui::GetWindowPos(), ImGui::GetWindowSize(), scale}, dt);
        }

        for (size_t c = 0; c < m_dragCards.size(); ++c) {
            ImVec2 swayOffset(pullOffset.x * 0.08f * c, pullOffset.y * 0.08f * c);
            ImVec2 cardPos = ImVec2(dragBasePos.x + pOffset.x * c + swayOffset.x, 
                                    dragBasePos.y + pOffset.y * c + swayOffset.y);
            
            m_dragCards[c].animPos = cardPos;
            DrawCard(drawList, cardPos, pSize, m_dragCards[c], scale, m_cornerRadius, 1.0f, true, false);
        }
    }
    
    return cardsAnimating;
}

void Game::CheckWinCondition(float scale, bool cardsAnimating) {
    if (!m_isWon && !cardsAnimating) {
        sol::protected_function isWonFunc = m_lua["IsWon"];
        if (isWonFunc.valid()) {
            try {
                lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug* ar) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
                sol::protected_function_result result = isWonFunc(m_piles);
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                
                if (result.valid() && result.get_type() == sol::type::boolean && result.get<bool>()) {
                    m_isWon = true;
                    m_particleSystem.StartVictory({ImGui::GetWindowPos(), ImGui::GetWindowSize(), scale});
                } else if (!result.valid()) {
                    sol::error err = result; throw err;
                }
            } catch (const sol::error& e) {
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                std::cerr << "Lua Error in IsWon: " << e.what() << std::endl;
            }
        }
    }
}

void Game::UpdateAndDraw() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float scaleX = ImGui::GetWindowWidth() / REFERENCE_WINDOW_WIDTH;
    float scaleY = ImGui::GetWindowHeight() / REFERENCE_WINDOW_HEIGHT;
    float scale = std::max(0.5f, std::min(scaleX, scaleY));

    // Factor out the OS scaling to let ImGui native dynamic DPI handle crisp rendering of sizes
    ImGui::GetStyle().FontScaleMain = scale / ImGui::GetMainViewport()->DpiScale;

    ScopedThemeStyle themeStyle;
    themeStyle.Apply(ActiveTheme().colors, ActiveTheme().buttonRounding, scale);
    RenderMenuBar();
    // Menu actions can switch games. Reapply the palette before drawing the board.
    themeStyle.Apply(ActiveTheme().colors, ActiveTheme().buttonRounding, scale);

    ImVec2 winPos = ImGui::GetWindowPos();
    winPos.y += ImGui::GetFrameHeight();

    // Cover the whole client area, including the menu margin and toolbar;
    // the board window itself is transparent and may begin below the menu.
    const auto& theme = ActiveTheme();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewportEnd(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImGui::GetBackgroundDrawList(viewport)->AddRectFilledMultiColor(viewport->Pos, viewportEnd,
        theme.background, theme.background, theme.backgroundBottom, theme.backgroundBottom);

    bool hasGame = !m_currentScriptPath.empty();
    if (!hasGame) {
        m_particleSystem.Clear();
        RenderStartScreen(drawList, scale);
        return;
    }

    drawList->AddRectFilled(ImGui::GetWindowPos(),
        ImVec2(winPos.x + ImGui::GetWindowWidth(), winPos.y), theme.toolbar);
    RenderInGameMenu(scale);
    themeStyle.Apply(ActiveTheme().colors, ActiveTheme().buttonRounding, scale);
    if (m_currentScriptPath.empty()) {
        m_particleSystem.Clear();
        return;
    }

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
        maxLogicalX = REFERENCE_WINDOW_WIDTH;
    }

    float logicalWidth = maxLogicalX - minLogicalX;
    float boardPixelWidth = logicalWidth * scale;
    float boardOffsetX = std::max(0.0f, (ImGui::GetWindowWidth() - boardPixelWidth) * 0.5f);
    
    ImVec2 boardBasePos = winPos;
    bool autoCenter = m_lua["AutoCenter"].get_or(true);
    if (autoCenter) {
        boardBasePos.x += boardOffsetX - (minLogicalX * scale);
    }

    s_boardScale = scale;
    s_boardBasePos = boardBasePos;

    bool isDealing = false;
    for (const auto& p : m_piles) {
        for (const auto& c : p.cards) {
            if (!c.hasInitializedPos) isDealing = true;
        }
    }

    int hoveredPile = -1;
    int hoveredCard = -1;

    if (!isDealing && !m_isWon) {
        m_gameTime += ImGui::GetIO().DeltaTime;
        ProcessInput(scale, boardBasePos, hoveredPile, hoveredCard);
        ProcessAutoSolve();
    }

    DrawScriptLayer("DrawBackground");
    bool cardsAnimating = RenderBoard(drawList, scale, boardBasePos, hoveredPile, hoveredCard);
    DrawScriptLayer("Draw");
    if (!m_pendingAction.empty()) {
        std::string action = std::move(m_pendingAction);
        m_pendingAction.clear();
        if (!isDealing && !m_isWon) HandleAction(action);
    }

    CheckWinCondition(scale, cardsAnimating);

    UpdateAndDrawParticles(drawList, scale);
}

void Game::DrawEmptyPile(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, PileType type, float cornerRadius) {
    if (type == PileType::Invisible) return;

    float r = cornerRadius * scale;
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ActiveTheme().emptyPile, r);
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ActiveTheme().emptyPileBorder, r, 0, 2.0f * scale);

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        float fontSize = 44.0f * scale; // Base 22.0f * 2.0f
    ImU32 textColor = ActiveTheme().emptyPileText;

    if (type == PileType::Foundation) {
        ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "A");
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x * 0.5f - tsize.x * 0.5f, pos.y + size.y * 0.5f - tsize.y * 0.5f), textColor, "A");
    } else if (type == PileType::FreeCellSlot) {
            fontSize = 32.0f * scale;
        ImVec2 tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "Free");
            if (tsize.x > size.x - 4.0f * scale) {
                fontSize *= (size.x - 4.0f * scale) / tsize.x;
                tsize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "Free");
            }
        drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + size.x * 0.5f - tsize.x * 0.5f, pos.y + size.y * 0.5f - tsize.y * 0.5f), textColor, "Free");
    }

    ImGui::PopFont();
}

void Game::DrawCardBack(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, float scale, float cornerRadius, float widthScale, bool isDragged) {
    float r = cornerRadius * scale;
    float s = (isDragged ? SHADOW_OFFSET_DRAGGED : SHADOW_OFFSET_NORMAL) * scale;
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
        drawList->AddRectFilled(pMin, pMax, ActiveTheme().cardBack, r);
        
        if (widthScale > 0.05f) {
            drawList->AddRect(ImVec2(pMin.x + 6.0f * scale, pMin.y + 6.0f * scale), ImVec2(pMax.x - 6.0f * scale, pMax.y - 6.0f * scale), IM_COL32(255, 255, 255, 100), r * 0.5f, 0, 1.5f * scale);
        }
    }

    // Border
    drawList->AddRect(pMin, pMax, ActiveTheme().cardBorder, r, 0, 1.0f * scale);
}

void Game::DrawCard(ImDrawList* drawList, const ImVec2& pos, const ImVec2& size, const Card& card, float scale, float cornerRadius, float widthScale, bool isDragged, bool isHovered) {
    float r = cornerRadius * scale;
    float s = (isDragged ? SHADOW_OFFSET_DRAGGED : SHADOW_OFFSET_NORMAL) * scale;
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
            float fontSize = 33.0f * scale; // Base 22.0f * 1.5f

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
    drawList->AddRect(pMin, pMax, ActiveTheme().cardBorder, r, 0, 1.0f * scale);
    
    // Hover Glow
    if (isHovered && !isDragged) {
        drawList->AddRect(pMin, pMax, ActiveTheme().cardHover, r, 0, 3.0f * scale);
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

void Game::SpawnActionParticles(ImVec2 center, ImVec2 size, float scale) {
    m_particleSystem.EmitMove(center, size, {ImGui::GetWindowPos(), ImGui::GetWindowSize(), scale});
}

void Game::UpdateAndDrawParticles(ImDrawList* drawList, float scale) {
    if (m_dragSourcePile == -1) m_particleSystem.StopTrail();
    ParticleSystem::View view{ImGui::GetWindowPos(), ImGui::GetWindowSize(), scale};
    m_particleSystem.Update(ImGui::GetIO().DeltaTime, view);
    m_particleSystem.Draw(drawList, view);
}

Game::SavedState Game::CaptureState() {
    SavedState state{m_piles, m_score, std::nullopt};
    sol::protected_function save = m_lua["SaveState"];
    sol::protected_function load = m_lua["LoadState"];
    if (save.valid() && load.valid()) {
        try {
            lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug*) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
            sol::protected_function_result result = save();
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            if (!result.valid()) { sol::error error = result; throw error; }
            if (result.get_type() == sol::type::string) state.scriptState = result.get<std::string>();
        } catch (const sol::error& error) {
            lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
            std::cerr << "Lua Error in SaveState: " << error.what() << std::endl;
        }
    }
    return state;
}

bool Game::StateChanged(const SavedState& before) {
    if (m_score != before.score || m_piles.size() != before.piles.size()) return true;
    for (size_t i = 0; i < m_piles.size(); ++i) {
        if (m_piles[i].cards != before.piles[i].cards) return true;
    }
    return before.scriptState != CaptureState().scriptState;
}

void Game::RestoreState(SavedState state) {
    m_piles = std::move(state.piles);
    m_score = state.score;
    if (state.scriptState) {
        sol::protected_function load = m_lua["LoadState"];
        if (load.valid()) {
            try {
                lua_sethook(m_lua.lua_state(), [](lua_State* L, lua_Debug*) { luaL_error(L, "Script execution limit exceeded!"); }, LUA_MASKCOUNT, 500000);
                sol::protected_function_result result = load(m_piles, *state.scriptState);
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                if (!result.valid()) { sol::error error = result; throw error; }
            } catch (const sol::error& error) {
                lua_sethook(m_lua.lua_state(), nullptr, 0, 0);
                std::cerr << "Lua Error in LoadState: " << error.what() << std::endl;
            }
        }
    }
    m_pendingAction.clear();
    m_dragSourcePile = -1;
    m_dragCardIndex = -1;
    m_dragCards.clear();
    m_isWon = false;
    m_particleSystem.Clear();
}

void Game::SaveStateForUndo() {
    m_undoStack.push_back(CaptureState());
    m_redoStack.clear();
}

void Game::Undo() {
    if (m_undoStack.empty()) return;
    m_redoStack.push_back(CaptureState());
    SavedState state = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    RestoreState(std::move(state));
}

void Game::Redo() {
    if (m_redoStack.empty()) return;
    m_undoStack.push_back(CaptureState());
    SavedState state = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    RestoreState(std::move(state));
}
