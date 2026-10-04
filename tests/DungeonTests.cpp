#include "Game.h"
#include "imgui_internal.h"
#include <array>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct DungeonTestAccess {
    static int Stat(Game& game, const char* name) {
        sol::table dungeon = game.m_lua["Dungeon"];
        return dungeon[name].get<int>();
    }
    static void Script(Game& game, const std::string& code) {
        auto result = game.m_lua.safe_script(code, sol::script_pass_on_error);
        if (!result.valid()) { sol::error error = result; throw error; }
    }
    static void Init(Game& game) {
        game.InitGame("src/dungeon.lua");
        Require(!game.m_currentScriptPath.empty(), "dungeon failed to load");
    }
    static void Fixture(Game& game, int hp = 20) {
        Init(game);
        for (auto& pile : game.m_piles) pile.cards.clear();
        Script(game, "Dungeon.hp = " + std::to_string(hp) + "; Dungeon.dur = 0; SetScore(0)");
        // A stock card keeps combat cases from immediately ending the run.
        CardAt(game, 5, 2, Suit::Diamonds, false);
    }
    static void CardAt(Game& game, int pile, int rank, Suit suit, bool faceUp = true) {
        game.m_piles[pile].cards.push_back(Card{static_cast<Rank>(rank), suit, faceUp});
    }
    static int CardCount(Game& game) {
        int count = 0;
        for (const auto& pile : game.m_piles) count += static_cast<int>(pile.cards.size());
        return count;
    }

    static void TestStartingDeck() {
        Game game(false);
        Init(game);
        Require(game.m_piles.size() == 7 && CardCount(game) == 52 && game.m_piles[5].cards.size() == 47,
            "initial deal lost cards");
        for (int i = 0; i < 4; ++i) Require(game.m_piles[i].cards.size() == 1, "initial room was not filled");
        Require(game.m_piles[4].cards.back().rank == Rank::Six && Stat(game, "dur") == 3,
            "starter weapon missing");
        std::array<int, 52> seen{};
        for (const auto& pile : game.m_piles) {
            for (const auto& card : pile.cards) {
                ++seen[static_cast<int>(card.suit) * 13 + static_cast<int>(card.rank) - 1];
                Require(card.faceUp == (pile.id != 5), "initial card visibility incorrect");
            }
        }
        for (int count : seen) Require(count == 1, "deal duplicated or omitted a card");
    }

    static void TestCombatAndUndo() {
        Game game(false);
        Fixture(game);
        CardAt(game, 4, 6, Suit::Clubs);
        Script(game, "Dungeon.dur = 3");
        CardAt(game, 0, 8, Suit::Spades);
        CardAt(game, 1, 5, Suit::Spades);
        CardAt(game, 2, 10, Suit::Spades);
        auto count = CardCount(game);
        game.HandleClick(0);
        Require(Stat(game, "hp") == 18 && Stat(game, "dur") == 2 && Stat(game, "kills") == 1,
            "armed combat calculation wrong");
        game.HandleClick(1);
        Require(Stat(game, "hp") == 18 && Stat(game, "dur") == 1, "blocked attack should use a strike");
        game.HandleClick(2);
        Require(Stat(game, "hp") == 14 && Stat(game, "dur") == 0 && game.m_piles[4].cards.empty(),
            "weapon did not break on third strike");
        Require(CardCount(game) == count && game.m_score == 15, "combat lost cards or miscounted score");
        game.Undo();
        Require(Stat(game, "hp") == 18 && Stat(game, "dur") == 1 && Stat(game, "kills") == 2 &&
            game.m_piles[2].cards.size() == 1 && game.m_piles[4].cards.size() == 1 && game.m_score == 10,
            "undo did not restore combat state");
        game.Redo();
        Require(Stat(game, "hp") == 14 && Stat(game, "dur") == 0 && game.m_piles[4].cards.empty(),
            "redo did not restore broken weapon");

        Fixture(game);
        CardAt(game, 4, 10, Suit::Clubs);
        Script(game, "Dungeon.dur = 3");
        CardAt(game, 0, 8, Suit::Spades);
        game.HandleAction("bare:0");
        Require(Stat(game, "hp") == 12 && Stat(game, "dur") == 3, "bare-handed fight consumed weapon durability");
        CardAt(game, 1, 2, Suit::Clubs);
        game.HandleAction("leave:1");
        Require(game.m_piles[4].cards.back().rank == Rank::Ten, "leaving a weapon replaced the current one");
        CardAt(game, 2, 1, Suit::Clubs);
        game.HandleClick(2);
        Require(game.m_piles[4].cards.back().rank == Rank::Ace && Stat(game, "dur") == 3,
            "weapon replacement did not reset strikes");
    }

    static void TestPotionsAndCarry() {
        Game game(false);
        Fixture(game, 10);
        CardAt(game, 0, 3, Suit::Hearts);
        CardAt(game, 1, 7, Suit::Hearts);
        game.HandleClick(0);
        Require(Stat(game, "hp") == 13 && Stat(game, "potions") == 1, "potion healing incorrect");
        auto history = game.m_undoStack.size();
        game.HandleClick(1);
        Require(Stat(game, "hp") == 13 && game.m_piles[1].cards.size() == 1 && game.m_undoStack.size() == history,
            "second potion was consumed or created history");
        game.HandleAction("advance");
        Require(Stat(game, "room") == 2 && Stat(game, "potions") == 0 && game.m_piles[1].cards.size() == 1,
            "room advance lost carried potion or did not reset allowance");
        game.HandleClick(1);
        Require(Stat(game, "hp") == 20, "carried potion unusable in next room");
        game.Undo();
        Require(Stat(game, "hp") == 13 && Stat(game, "potions") == 0, "undo did not restore potion allowance");
        game.Undo();
        Require(Stat(game, "room") == 1 && Stat(game, "potions") == 1, "undo did not restore previous room");

        Fixture(game);
        CardAt(game, 0, 5, Suit::Hearts);
        game.HandleClick(0);
        Require(!game.m_piles[0].cards.empty(), "full-health click wasted a potion");
        game.m_piles[5].cards.clear();
        game.HandleAction("leave:0");
        Require(Stat(game, "finished") == 1 && game.m_piles[0].cards.empty(), "unused final potion blocked victory");
    }

    static void TestCampAndRetreat() {
        Game game(false);
        Fixture(game, 10);
        Script(game, "Dungeon.gold = 16; SetScore(16)");
        game.HandleAction("camp");
        Require(Stat(game, "hp") == 14 && Stat(game, "gold") == 8 && Stat(game, "rested") == 1,
            "camp costs or healing incorrect");
        game.HandleAction("camp");
        Require(Stat(game, "hp") == 14 && Stat(game, "gold") == 8, "camp repeated in one room");
        game.Undo();
        Require(Stat(game, "hp") == 10 && Stat(game, "gold") == 16 && Stat(game, "rested") == 0 && game.m_score == 16,
            "state-only camp action was not undoable");
        game.Redo();
        Require(Stat(game, "rested") == 1 && game.m_score == 8, "camp redo failed");

        Fixture(game);
        game.m_piles[5].cards.clear();
        for (int rank = 2; rank <= 7; ++rank) CardAt(game, 5, rank, Suit::Diamonds, false);
        for (int i = 0; i < 4; ++i) CardAt(game, i, i + 8, Suit::Spades);
        auto before = game.CaptureState();
        int count = CardCount(game);
        game.HandleAction("advance");
        Require(Stat(game, "room") == 1, "advanced past live monsters");
        game.HandleAction("camp");
        Require(Stat(game, "hp") == 20, "camp allowed in unsafe room");
        game.HandleAction("retreat");
        Require(Stat(game, "hp") == 18 && Stat(game, "canRetreat") == 0 && Stat(game, "retreats") == 1,
            "retreat cost or cooldown incorrect");
        Require(CardCount(game) == count && game.m_piles[5].cards[0].suit == Suit::Spades &&
            game.m_piles[0].cards.back().rank == Rank::Seven,
            "retreat lost encounters or changed unexplored deck order");
        game.HandleAction("retreat");
        Require(Stat(game, "hp") == 18 && Stat(game, "retreats") == 1, "consecutive retreats allowed");
        game.Undo();
        Require(game.CaptureState().scriptState == before.scriptState && game.m_piles[0].cards.back().suit == Suit::Spades,
            "undo did not restore retreated room");
        Script(game, "Dungeon.hp = 2");
        game.HandleAction("retreat");
        Require(Stat(game, "hp") == 2 && Stat(game, "retreats") == 0, "retreat killed the player");
    }

    static void TestEndStatesAndHookIsolation() {
        Game game(false);
        Fixture(game, 5);
        game.m_piles[5].cards.clear();
        CardAt(game, 0, 1, Suit::Spades);
        game.HandleClick(0);
        Require(Stat(game, "hp") == 0 && Stat(game, "finished") == 0, "lethal ace counted as victory");
        auto deadState = game.CaptureState().scriptState;
        game.HandleAction("camp");
        Require(game.CaptureState().scriptState == deadState, "action changed dead run");
        game.Undo();
        Require(Stat(game, "hp") == 5 && !game.m_piles[0].cards.empty(), "undo could not recover from death");

        Fixture(game);
        game.m_piles[5].cards.clear();
        CardAt(game, 0, 10, Suit::Diamonds);
        game.HandleAction("retreat");
        Require(Stat(game, "retreats") == 0, "retreated from final room");
        game.HandleClick(0);
        Require(Stat(game, "finished") == 1 && Stat(game, "gold") == 10 && game.m_score == 50,
            "victory score incorrect");
        game.Undo();
        Require(Stat(game, "finished") == 0 && game.m_score == 0, "undo kept victory bonus");
        game.Redo();
        Require(Stat(game, "finished") == 1 && game.m_score == 50, "redo lost victory bonus");
        game.InitGame("src/klondike.lua");
        Require(!game.m_lua["SaveState"].valid() && !game.m_lua["HandleAction"].valid() &&
            !game.m_lua["DrawBackground"].valid(), "dungeon hooks leaked into another game");

        Fixture(game);
        CardAt(game, 0, 8, Suit::Spades);
        auto before = game.CaptureState();
        Script(game, "function HandleAction(piles) Dungeon.hp = 1; SetScore(99); piles:get(0).cards:clear(); error('failed action') end");
        game.HandleAction("broken");
        Require(game.CaptureState().scriptState == before.scriptState && game.m_piles[0].cards.size() == 1 && game.m_score == 0,
            "failed action left partial run changes");
    }

    static void TestDrawingAndQueuedActions(const char* previewPath) {
        Game game(false);
        Fixture(game, 16);
        CardAt(game, 0, 9, Suit::Spades);
        CardAt(game, 1, 5, Suit::Hearts);
        CardAt(game, 2, 10, Suit::Diamonds);
        CardAt(game, 3, 7, Suit::Clubs);
        CardAt(game, 4, 6, Suit::Clubs);
        game.m_lua.set_function("CheckTheme", [&]() {
            Require(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)) == IM_COL32(43, 57, 76, 255),
                "Lua HUD buttons did not inherit game theme");
            Require(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_PopupBg)) == IM_COL32(25, 32, 44, 255),
                "Lua tooltips did not inherit game theme");
        });
        Script(game, "Dungeon.dur = 3; Dungeon.gold = 12; SetScore(12); "
            "drawnFrames = 0; originalDraw = Draw; Draw = function() CheckTheme(); originalDraw(); drawnFrames = drawnFrames + 1 end");

        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1280, 720);
        io.DeltaTime = 1.0f / 60;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetStyle().FontSizeBase = 22;
        io.Fonts->AddFontDefaultVector();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(1);
        int frames = 0;
        ImVec2 fightButton;
        std::unordered_set<ImGuiID> buttonIds;
        std::vector<ImVec2> keepCurrentButtons;
        game.m_lua.set_function("CheckButtonIdentity", [&](const std::string& label) {
            const auto& item = ImGui::GetCurrentContext()->LastItemData;
            Require(item.ID != 0 && buttonIds.insert(item.ID).second, "board buttons have conflicting IDs");
            if (label == "Keep current") keepCurrentButtons.push_back(item.Rect.GetCenter());
        });
        Script(game, "originalButton = DrawBoardButton; DrawBoardButton = function(x, y, w, h, label, enabled) "
            "local clicked = originalButton(x, y, w, h, label, enabled); CheckButtonIdentity(label); return clicked end");
        auto drawFrame = [&]() {
            ImGui::NewFrame();
            auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("Dungeon test board", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar();
            float scale = std::max(0.5f, std::min(ImGui::GetWindowWidth() / 1280, ImGui::GetWindowHeight() / 720));
            ImVec2 origin = ImGui::GetWindowPos();
            origin.y += ImGui::GetFrameHeight();
            for (auto& pile : game.m_piles) {
                for (auto& card : pile.cards) {
                    card.hasInitializedPos = true;
                    card.animPos = ImVec2(origin.x + pile.pos.x * scale, origin.y + pile.pos.y * scale);
                    card.flipVisual = card.faceUp ? 1.0f : -1.0f;
                }
            }
            const ImGuiStyle originalStyle = ImGui::GetStyle();
            buttonIds.clear();
            keepCurrentButtons.clear();
            game.UpdateAndDraw();
            Require(ImGui::GetCurrentContext()->HoveredIdPreviousFrameItemCount <= 1,
                "hovering a board button triggered an ImGui ID conflict");
            const auto& background = ImGui::GetBackgroundDrawList(viewport)->VtxBuffer;
            Require(background.Size >= 4 && background[0].pos.x == viewport->Pos.x &&
                background[0].pos.y == viewport->Pos.y && background[0].col == game.ActiveTheme().background &&
                background[2].pos.x == viewport->Pos.x + viewport->Size.x &&
                background[2].pos.y == viewport->Pos.y + viewport->Size.y &&
                background[2].col == game.ActiveTheme().backgroundBottom, "theme did not cover full client area");
            for (int i = 0; i < ImGuiCol_COUNT; ++i) {
                const auto& original = originalStyle.Colors[i];
                const auto& restored = ImGui::GetStyle().Colors[i];
                Require(original.x == restored.x && original.y == restored.y &&
                    original.z == restored.z && original.w == restored.w, "game theme leaked into caller style");
            }
            Require(originalStyle.FrameRounding == ImGui::GetStyle().FrameRounding, "theme rounding leaked into caller style");
            fightButton = ImVec2(ImGui::GetWindowPos().x + 375 * scale,
                ImGui::GetWindowPos().y + ImGui::GetFrameHeight() + (499 + 18) * scale);
            ImGui::End();
            ImGui::Render();
            // Simulate the renderer's texture lifecycle while retaining CPU
            // pixels for preview capture. This exercises the app's scalable fonts.
            for (auto* texture : ImGui::GetPlatformIO().Textures) {
                if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates) {
                    texture->SetTexID(static_cast<ImTextureID>(texture->UniqueID));
                    texture->SetStatus(ImTextureStatus_OK);
                } else if (texture->Status == ImTextureStatus_WantDestroy) {
                    texture->SetTexID(ImTextureID_Invalid);
                    texture->SetStatus(ImTextureStatus_Destroyed);
                }
            }
            ++frames;
            Require(game.m_lua["drawnFrames"].get<int>() == frames, "dungeon HUD failed to draw");
        };
        // Warm up the font atlas before capturing the actual ImGui geometry.
        for (int i = 0; i < 5; ++i) drawFrame();
        if (previewPath) {
            Require(io.Fonts->TexData->Format == ImTextureFormat_RGBA32, "unexpected preview atlas format");
            pixels = io.Fonts->TexData->Pixels;
            width = io.Fonts->TexData->Width;
            height = io.Fonts->TexData->Height;
            std::ofstream atlas(std::string(previewPath) + ".atlas", std::ios::binary);
            atlas.write(reinterpret_cast<const char*>(&width), sizeof(width));
            atlas.write(reinterpret_cast<const char*>(&height), sizeof(height));
            atlas.write(reinterpret_cast<const char*>(pixels), width * height * 4);
            std::ofstream output(previewPath);
            output << "[";
            bool first = true;
            auto* data = ImGui::GetDrawData();
            for (const auto* list : data->CmdLists) {
                for (const auto& command : list->CmdBuffer) {
                    if (command.UserCallback) continue;
                    for (unsigned i = 0; i < command.ElemCount; i += 3) {
                        if (!first) output << ',';
                        first = false;
                        output << "{\"clip\":[" << command.ClipRect.x << ',' << command.ClipRect.y << ','
                            << command.ClipRect.z << ',' << command.ClipRect.w << "],\"vertices\":[";
                        for (unsigned j = 0; j < 3; ++j) {
                            if (j) output << ',';
                            const auto& vertex = list->VtxBuffer[list->IdxBuffer[command.IdxOffset + i + j] + command.VtxOffset];
                            output << '[' << vertex.pos.x << ',' << vertex.pos.y << ',' << vertex.uv.x << ','
                                << vertex.uv.y << ',' << vertex.col << ']';
                        }
                        output << "]}";
                    }
                }
            }
            output << ']';
        }

        // Click the real Fight button. It queues a history-aware action after
        // Draw rather than mutating the board while the HUD holds references.
        io.AddMousePosEvent(fightButton.x, fightButton.y);
        drawFrame();
        io.AddMouseButtonEvent(0, true);
        drawFrame();
        io.AddMouseButtonEvent(0, false);
        drawFrame();
        Require(Stat(game, "hp") == 13 && Stat(game, "dur") == 2 && game.m_pendingAction.empty(),
            "Fight button did not apply queued action");
        game.Undo();
        Require(Stat(game, "hp") == 16 && Stat(game, "dur") == 3, "HUD action bypassed undo");

        // Two weapons produce duplicate Equip and Keep current labels. Each
        // alternative must discard only its own encounter and remain undoable.
        game.m_piles[2].cards.clear();
        CardAt(game, 2, 9, Suit::Clubs);
        drawFrame();
        Require(keepCurrentButtons.size() == 2, "duplicate-label fixture did not draw both alternatives");
        for (int buttonIndex : {1, 0}) {
            const ImVec2 button = keepCurrentButtons[buttonIndex];
            io.AddMousePosEvent(button.x, button.y);
            drawFrame();
            drawFrame();
            io.AddMouseButtonEvent(0, true);
            drawFrame();
            io.AddMouseButtonEvent(0, false);
            drawFrame();
            const int pile = 2 + buttonIndex;
            const int otherPile = 3 - buttonIndex;
            Require(game.m_piles[pile].cards.empty() && game.m_piles[otherPile].cards.size() == 1 &&
                game.m_piles[4].cards.back().rank == Rank::Six && Stat(game, "dur") == 3,
                "duplicate-label button acted on the wrong encounter");
            game.Undo();
            drawFrame();
            Require(keepCurrentButtons.size() == 2 && game.m_piles[2].cards.size() == 1 && game.m_piles[3].cards.size() == 1,
                "duplicate-label button bypassed undo");
        }

        const auto stableButtonIds = buttonIds;
        io.DisplaySize = ImVec2(960, 540);
        drawFrame();
        Require(buttonIds == stableButtonIds, "resizing changed board button identity");
        io.DisplaySize = ImVec2(2560, 1440);
        ImGui::GetStyle().ScaleAllSizes(2);
        drawFrame();
        Require(buttonIds == stableButtonIds, "DPI scaling changed board button identity");
        ImGui::GetStyle().ScaleAllSizes(0.5f);
        // Legacy three-argument text calls must still work in other rule scripts.
        Init(game);
        game.InitGame("src/example.lua");
        Script(game, "drawnFrames = 0; originalDraw = Draw; Draw = function() originalDraw(); drawnFrames = drawnFrames + 1 end");
        frames = 0;
        drawFrame();
        ImGui::DestroyContext();
    }
};

int main(int argc, char** argv) {
    try {
        DungeonTestAccess::TestStartingDeck();
        DungeonTestAccess::TestCombatAndUndo();
        DungeonTestAccess::TestPotionsAndCarry();
        DungeonTestAccess::TestCampAndRetreat();
        DungeonTestAccess::TestEndStatesAndHookIsolation();
        DungeonTestAccess::TestDrawingAndQueuedActions(argc == 3 && std::string(argv[1]) == "--preview" ? argv[2] : nullptr);
        std::cout << "Dungeon regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
