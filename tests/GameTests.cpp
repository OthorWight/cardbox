#include "Game.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct GameTestAccess {
    static void RunScript(Game& game, const std::string& script) {
        auto result = game.m_lua.safe_script(script, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error error = result;
            throw std::runtime_error(error.what());
        }
    }

    static void TestBindings() {
        Game game(false);
        std::vector<Card> deck;
        game.CreateDeck(deck);
        game.m_lua["deck"] = &deck;
        RunScript(game, R"(
            local cards = VectorCard.new()
            local piles = VectorPile.new()
            local defaultCard = Card.new()
            assert(defaultCard.rank == 1 and defaultCard.suit == 0,
                "invalid default card: " .. tostring(defaultCard.rank) .. ", " .. tostring(defaultCard.suit))
            local function fails(f)
                local ok = pcall(f)
                assert(not ok, "invalid operation should fail")
            end
            fails(function() cards:pop_back() end)
            fails(function() cards:back() end)
            fails(function() cards:front() end)
            fails(function() cards:get(0) end)
            fails(function() cards:get(-1) end)
            fails(function() piles:get(0) end)
            fails(function() piles:get(-1) end)
            fails(function() deck:get(deck:size()) end)
            fails(function() deck:get(4294967296) end)
            local c = deck:get(0)
            fails(function() c.rank = 0 end)
            fails(function() c.rank = 14 end)
            fails(function() c.suit = -1 end)
            fails(function() c.suit = 4 end)
            fails(function() c.rank = 4294967297 end)
            fails(function() c.suit = 4294967296 end)
            assert(c.rank == 1 and c.suit == 0)
            c.rank = 13
            c.suit = 3
            assert(deck:front().rank == 13 and deck:front().suit == 3)
            cards:push_back(c)
            assert(cards:back().rank == 13)
            cards:pop_back()
            assert(cards:empty())
        )");
        for (int count : {0, -1, 9, 1000000}) {
            bool rejected = false;
            try { game.CreateDeck(deck, count); }
            catch (const std::out_of_range&) { rejected = true; }
            Require(rejected, "unbounded deck count accepted");
        }
    }

    static void TestMovesAndHistory() {
        Game game(false);
        Pile source{};
        source.type = PileType::Tableau;
        source.cards.push_back(Card{Rank::Ace, Suit::Hearts, true});
        Pile target{};
        target.id = 1;
        target.type = PileType::Foundation;
        game.m_piles = {source, target};
        RunScript(game, R"(
            function CanPickup() return true end
            function CanDrop() return true end
            function AfterMove() AddScore(15) end
        )");
        for (const auto& move : {
                "{-1, 1, 0}", "{100, 1, 0}", "{0, -1, 0}", "{0, 100, 0}",
                "{0, 0, 0}", "{0, 1, -1}", "{0, 1, 1}", "{0}",
                "{'bad', 1, 0}", "{0, 1, 0.5}", "{4294967296, 1, 0}",
                "{0, 4294967297, 0}", "{0, 1, 4294967296}", "{}"}) {
            RunScript(game, std::string("function AutoSolve() return ") + move + " end");
            game.ProcessAutoSolve();
            Require(game.m_piles[0].cards.size() == 1 && game.m_piles[1].cards.empty(),
                "invalid auto-solve move changed board");
        }
        game.DoMove(0, 0, 0);
        game.DoMove(-1, 1, 0);
        game.DoMove(0, 10, 0);
        game.DoMove(0, 1, 10);
        Require(game.m_piles[0].cards.size() == 1, "invalid direct move changed board");

        RunScript(game, "function AutoSolve() return {0, 1, 0} end; function CanDrop() return false end");
        game.ProcessAutoSolve();
        Require(game.m_piles[0].cards.size() == 1, "auto-solve bypassed drop rules");
        RunScript(game, "function CanDrop() return true end; function CanPickup() return false end");
        game.ProcessAutoSolve();
        Require(game.m_piles[0].cards.size() == 1, "auto-solve bypassed pickup rules");

        RunScript(game, "function CanPickup() return true end");
        game.SaveStateForUndo();
        game.ProcessAutoSolve();
        Require(game.m_piles[0].cards.empty() && game.m_piles[1].cards.size() == 1 && game.m_score == 15,
            "valid auto-solve move failed");
        game.Undo();
        Require(game.m_piles[0].cards.size() == 1 && game.m_score == 0, "undo did not restore board and score");
        game.Redo();
        Require(game.m_piles[1].cards.size() == 1 && game.m_score == 15, "redo did not restore board and score");

        RunScript(game, "function HandleClick() AddScore(5) end");
        game.HandleClick(1);
        Require(game.m_score == 20, "score-only click failed");
        game.Undo();
        Require(game.m_score == 15, "score-only click was not undoable");
        game.Redo();
        Require(game.m_score == 20, "score-only click was not redoable");
        RunScript(game, "function HandleClick() end");
        auto historySize = game.m_undoStack.size();
        game.HandleClick(1);
        Require(game.m_undoStack.size() == historySize, "no-op click created undo history");
    }

    static void TestGames() {
        Game game(false);
        size_t gameCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator("src")) {
            if (entry.path().extension() != ".lua") continue;
            game.InitGame(entry.path().string());
            Require(!game.m_currentScriptPath.empty(), "bundled game failed to initialize");
            Require(lua_gethook(game.m_lua.lua_state()) == nullptr, "instruction hook left active");
            // Exercise callbacks using the initialized board, including scripts that
            // do their own work in AutoSolve and return an empty table.
            game.ProcessAutoSolve();
            ++gameCount;
        }
        Require(gameCount >= 10, "bundled game smoke tests did not run");

        RunScript(game, "function AutoSolve() while true do end end");
        game.ProcessAutoSolve();
        Require(lua_gethook(game.m_lua.lua_state()) == nullptr, "failed callback left instruction hook active");
        RunScript(game, "assert(2 + 2 == 4)");

        game.InitGame("src/does-not-exist.lua");
        Require(game.m_currentScriptPath.empty() && game.m_piles.empty(), "failed initialization left an active game");
    }

    static void TestAllocatorIsolation() {
        Game first(false);
        size_t firstMemory = first.m_luaAllocatedMemory;
        {
            Game second(false);
            void* firstTracker = nullptr;
            void* secondTracker = nullptr;
            lua_getallocf(first.m_lua.lua_state(), &firstTracker);
            lua_getallocf(second.m_lua.lua_state(), &secondTracker);
            Require(firstTracker != secondTracker, "games share Lua memory accounting");
            RunScript(second, "allocation = string.rep('x', 100000)");
            Require(first.m_luaAllocatedMemory == firstMemory, "second game changed first memory accounting");
            RunScript(second, R"(
                local ok = pcall(function() return string.rep('x', 20 * 1024 * 1024) end)
                assert(not ok, "Lua memory limit was bypassed")
                assert(string.rep('x', 3) == 'xxx')
            )");
        }
        RunScript(first, "assert(string.rep('x', 10) == 'xxxxxxxxxx')");
    }
};

int main() {
    try {
        GameTestAccess::TestBindings();
        GameTestAccess::TestMovesAndHistory();
        GameTestAccess::TestGames();
        GameTestAccess::TestAllocatorIsolation();
        std::cout << "Cardbox engine regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
