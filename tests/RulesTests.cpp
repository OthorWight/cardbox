#include "Game.h"
#include <array>
#include <iostream>
#include <stdexcept>

struct RulesTestAccess {
    static void Script(Game& game, const std::string& script) {
        auto result = game.m_lua.safe_script(script, sol::script_pass_on_error);
        if (!result.valid()) { sol::error error = result; throw std::runtime_error(game.m_currentGameName + ": " + error.what()); }
    }
    static void Init(Game& game, const char* path) {
        game.InitGame(std::string("src/") + path + ".lua");
        if (game.m_currentScriptPath.empty()) throw std::runtime_error("Game failed to initialize");
        game.m_lua["p"] = &game.m_piles;
        game.m_lua.set_function("Click", [&](int pile, int index) { game.HandleClick(pile, index); });
        game.m_lua.set_function("Action", [&](const std::string& action) { game.HandleAction(action); });
        game.m_lua.set_function("UndoTest", [&]() { game.Undo(); });
        game.m_lua.set_function("RedoTest", [&]() { game.Redo(); });
        Script(game, R"(
            function clear() for i=0,p:size()-1 do p:get(i).cards:clear() end end
            function add(i,r,s,up)
                local c=Card.new(); c.rank,c.suit,c.faceUp=r,s or Suit.Spades,up~=false
                p:get(i).cards:push_back(c)
            end
            function stack(i,index)
                local cards=VectorCard.new()
                for j=index or 0,p:get(i).cards:size()-1 do cards:push_back(p:get(i).cards:get(j)) end
                return cards
            end
            function count()
                local n=0; for i=0,p:size()-1 do n=n+p:get(i).cards:size() end; return n
            end
            function conservation()
                assert(count()==52,'card count')
                local seen={}
                for i=0,p:size()-1 do for j=0,p:get(i).cards:size()-1 do
                    local c=p:get(i).cards:get(j); local k=c.suit*13+c.rank
                    assert(not seen[k],'duplicate card'); seen[k]=true
                end end
            end
        )");
    }
    static void SolitaireRules() {
        Game game(false);
        Init(game,"klondike");
        Script(game,R"(
            conservation()
            for i=6,12 do assert(p:get(i).cards:size()==i-5); assert(p:get(i).cards:back().faceUp) end
            local before=SaveState(); Click(0,-1); assert(p:get(1).cards:size()==1)
            UndoTest(); assert(SaveState()==before and p:get(1).cards:empty())
            RedoTest(); assert(p:get(1).cards:size()==1)
            for i=1,23 do Click(0,-1) end
            assert(p:get(0).cards:empty() and p:get(1).cards:size()==24)
            local first=p:get(1).cards:front().rank; Click(0,-1)
            assert(p:get(0).cards:back().rank==first and SaveState():match(',1$'))
            conservation(); clear()
            add(6,7,Suit.Spades,false); add(6,6,Suit.Hearts); add(6,5,Suit.Clubs)
            assert(not CanPickup(p,6,0) and CanPickup(p,6,1))
            add(7,7,Suit.Clubs); assert(CanDrop(p,6,7,stack(6,1)))
            add(8,8,Suit.Hearts); assert(not CanDrop(p,6,8,stack(6,1)))
            assert(not CanDrop(p,6,2,stack(6,1)))
            p:get(6).cards:clear(); add(6,1,Suit.Hearts)
            assert(CanDrop(p,6,2,stack(6)))
            Action('safe'); assert(p:get(2).cards:size()==1 and p:get(6).cards:empty())
            UndoTest(); assert(p:get(6).cards:size()==1 and p:get(2).cards:empty())
        )");
        Init(game,"freecell");
        Script(game,R"(
            conservation(); clear()
            for i=0,3 do add(i,13) end
            for i=10,15 do add(i,13) end
            add(8,7,Suit.Spades); add(8,6,Suit.Hearts)
            assert(CanPickup(p,8,0))
            assert(MoveCapacity(p,8,9)==1 and not CanDrop(p,8,9,stack(8)), 'empty destination counted as spare')
            p:get(10).cards:clear(); add(9,8,Suit.Hearts)
            assert(MoveCapacity(p,8,9)==2 and CanDrop(p,8,9,stack(8)))
            assert(not CanDrop(p,8,0,stack(8)))
            p:get(0).cards:clear(); assert(CanDrop(p,8,0,stack(8,1)))
            add(4,1,Suit.Spades); add(4,2,Suit.Spades)
            assert(not CanPickup(p,4,0) and CanPickup(p,4,1))
            add(11,3,Suit.Hearts); assert(CanDrop(p,4,11,stack(4,1)))
        )");
        Init(game,"yukon");
        Script(game,R"(
            conservation(); clear()
            add(4,9,Suit.Spades,false); add(4,8,Suit.Hearts); add(4,3,Suit.Hearts); add(4,11,Suit.Clubs)
            add(5,9,Suit.Clubs)
            assert(not CanPickup(p,4,0) and CanPickup(p,4,1))
            assert(CanDrop(p,4,5,stack(4,1)) and not CanDrop(p,4,0,stack(4,1)))
            p:get(4).cards:pop_back(); p:get(4).cards:pop_back(); p:get(4).cards:pop_back()
            AfterMove(p,4,5); assert(p:get(4).cards:back().faceUp)
        )");
        Init(game,"spider");
        Script(game,R"(
            assert(count()==104 and p:get(8).cards:size()==50)
            local ranks={}; for i=0,p:size()-1 do for j=0,p:get(i).cards:size()-1 do
                local c=p:get(i).cards:get(j); assert(c.suit==Suit.Spades); ranks[c.rank]=(ranks[c.rank] or 0)+1
            end end
            for r=1,13 do assert(ranks[r]==8) end
            Click(8,-1); assert(p:get(8).cards:size()==40)
            UndoTest(); assert(p:get(8).cards:size()==50)
            p:get(9).cards:clear(); local before=SaveState(); Click(8,-1)
            assert(p:get(8).cards:size()==50 and SaveState()==before)
            clear(); add(9,4,Suit.Hearts,false)
            for run=1,2 do for r=13,1,-1 do add(9,r) end end
            AfterMove(p,10,9)
            assert(p:get(0).cards:size()==13 and p:get(1).cards:size()==13 and p:get(9).cards:back().faceUp)
            clear(); for r=13,1,-1 do add(9,r,r==7 and Suit.Hearts or Suit.Spades) end
            AfterMove(p,10,9); assert(p:get(0).cards:empty() and not CanPickup(p,9,0))
        )");
        Init(game,"golf");
        Script(game,R"(
            conservation(); clear(); add(0,9); add(0,13); add(8,1)
            assert(CanDrop(p,0,8,stack(0,1)) and not CanPickup(p,0,0))
            Click(0,0); assert(p:get(0).cards:size()==2,'buried click played top card')
            Click(0,1); assert(p:get(0).cards:size()==1 and p:get(8).cards:back().rank==13)
            UndoTest(); assert(p:get(0).cards:size()==2)
            local before=SaveState(); Click(7,-1); assert(SaveState()==before and p:get(7).cards:empty())
        )");
        Init(game,"pyramid");
        Script(game,R"(
            conservation(); assert(IsBlocked(p,2) and not IsBlocked(p,23))
            assert(not CanPickup(p,2,0) and CanPickup(p,23,0))
            p:get(3).cards:clear(); assert(IsBlocked(p,2)); p:get(4).cards:clear(); assert(not IsBlocked(p,2))
            clear(); add(23,13); Click(23,0); assert(p:get(23).cards:empty() and p:get(30).cards:size()==1)
            UndoTest(); assert(p:get(23).cards:size()==1)
            clear(); add(23,5); add(24,8); Click(23,0); Click(24,0)
            assert(p:get(23).cards:empty() and p:get(24).cards:empty() and p:get(30).cards:size()==2)
            UndoTest(); assert(p:get(23).cards:size()==1 and p:get(24).cards:size()==1)
            clear(); add(1,4); add(1,7)
            for pass=1,2 do Click(0,-1); Click(0,-1); Click(0,-1) end
            local before=SaveState(); Click(0,-1); assert(SaveState()==before and p:get(0).cards:empty())
        )");
    }
    static void PuzzleAndTutorial() {
        Game game(false);
        Init(game,"hanoi");
        Script(game,R"(
            assert(count()==52 and p:get(0).cards:size()==5)
            assert(not CanDrop(p,0,1,stack(0)))
            Action('easier'); Action('easier'); assert(p:get(0).cards:size()==3)
            UndoTest(); assert(p:get(0).cards:size()==4); RedoTest(); assert(p:get(0).cards:size()==3)
            function solve(n,source,target,spare)
                if n==0 then return end
                solve(n-1,source,spare,target)
                Action('peg:'..source); Action('peg:'..target)
                solve(n-1,spare,target,source)
            end
            solve(3,0,2,1)
            assert(IsWon(p) and SaveState()=='3,7' and GetScore()==7)
            UndoTest(); assert(not IsWon(p) and SaveState()=='3,6'); RedoTest(); assert(IsWon(p))
            Action('engine:restart'); assert(p:get(0).cards:size()==5 and SaveState()=='5,0')
        )");
        Init(game,"example");
        Script(game,R"(
            conservation()
            assert(not CanDrop(p,0,5,stack(0)) and CanDrop(p,0,5,stack(0,3)))
            for i=0,2 do for j=1,4 do Click(i,-1) end end
            assert(not IsWon(p),'tutorial won with undrawn stock')
            for i=1,40 do Click(3,-1); Click(4,-1) end
            assert(IsWon(p) and p:get(5).cards:size()==52); conservation()
            UndoTest(); assert(not IsWon(p) and p:get(4).cards:size()==1)
            RedoTest(); assert(IsWon(p))
        )");
        Init(game,"malicious");
        for (int i=1;i<=14;++i) game.HandleAction("probe:"+std::to_string(i));
        if (game.m_score != 14) throw std::runtime_error("Sandbox probes failed or did not preserve results");
        game.Undo();
        if (game.m_score != 13) throw std::runtime_error("Sandbox report undo failed");
        game.Redo();
        Script(game,"Action('reset'); assert(GetScore()==0); UndoTest(); assert(GetScore()==14)");
        if (lua_gethook(game.m_lua.lua_state())) throw std::runtime_error("Sandbox probe left instruction hook active");
        Init(game,"dungeon");
        const auto state = game.CaptureState();
        game.HandleAction("hint");
        if (game.StateChanged(state) || !game.m_undoStack.empty())
            throw std::runtime_error("Crawler advice changed the run or undo history");
    }
    static void PokerRules() {
        Game game(false);
        Init(game,"texas_holdem");
        Script(game,R"(
            conservation()
            local function hand(list)
                local cards={}; for _,spec in ipairs(list) do local c=Card.new(); c.rank,c.suit=spec[1],spec[2]; cards[#cards+1]=c end
                return EvaluateHand(cards)
            end
            assert(hand({{1,0},{2,1},{3,2},{4,3},{5,0},{9,1},{13,2}})==4000005,'wheel straight')
            assert(hand({{1,0},{1,1},{1,2},{13,0},{13,1},{13,2},{2,0}})==6000000+14*16+13,'double trips')
            assert(hand({{9,0},{10,0},{11,0},{12,0},{13,0},{1,1},{1,2}})==8000013,'straight flush')
            Action('start'); assert(Poker.dealer==1 and Poker.sb==2 and Poker.bb==3 and Poker.turn==4 and Poker.pot==30)
            conservation(); UndoTest(); assert(Poker.phase=='Ready' and Poker.pot==0)
            RedoTest(); assert(Poker.phase=='PreFlop' and Poker.pot==30)
            local initial=SaveState()
            Action('tick'); local after=SaveState(); UndoTest(); assert(SaveState()==initial)
            Action('tick'); assert(SaveState()==after,'undo changed bot randomness')
            local steps=0
            while Poker.phase~='Showdown' and Poker.phase~='Eliminated' and Poker.phase~='Finished' do
                if Poker.turn==1 then Action('call') else Action('tick') end
                steps=steps+1; assert(steps<200,'hand stalled')
                local chips=Poker.pot; for i=1,8 do chips=chips+Poker.players[i].chips end
                assert(chips==8000,'chips lost during betting'); conservation()
            end
            assert(Poker.pot==0)
            local final=SaveState(); PokerSettle(p); assert(SaveState()==final,'paid pot twice')
            -- Contribution tiers: P1 wins main pot, P2 side pot, P3 gets unmatched chips.
            clear(); Poker.phase='River'; Poker.dealer=8; Poker.pot=60
            for i=1,8 do
                local player=Poker.players[i]; player.chips,player.total,player.folded=1000,0,true
            end
            for i=1,3 do Poker.players[i].total=i*10; Poker.players[i].folded=false end
            add(7,1,0); add(7,1,1); add(8,13,0); add(8,13,1); add(9,12,0); add(9,12,1)
            add(2,2,0); add(3,4,1); add(4,6,2); add(5,8,3); add(6,10,0)
            PokerSettle(p)
            assert(Poker.players[1].payout==30 and Poker.players[2].payout==20 and Poker.players[3].payout==10,'side-pot winners')
            -- A shared board splits five chips 3/2 clockwise from the dealer.
            clear(); Poker.phase='River'; Poker.dealer=1; Poker.pot=5
            for i=1,8 do Poker.players[i].total,Poker.players[i].folded=0,true end
            Poker.players[1].total,Poker.players[1].folded=2,false
            Poker.players[2].total,Poker.players[2].folded=2,false
            Poker.players[3].total=1
            for i=2,5 do add(i,8+i,0) end; add(6,1,0)
            PokerSettle(p); assert(Poker.players[1].payout==2 and Poker.players[2].payout==3,'odd chip order')
        )");
        Init(game,"texas_holdem");
        Script(game,R"(
            for i=3,8 do Poker.players[i].chips=0 end
            Poker.dealer=2; Action('start')
            assert(Poker.dealer==1 and Poker.sb==1 and Poker.bb==2 and Poker.turn==1,'heads-up blinds')
            Action('call'); assert(Poker.turn==2); Action('tick'); Action('tick')
            assert(Poker.phase=='Flop' and Poker.turn==2,'heads-up postflop action')
            -- An all-in underraise does not reopen raising for a player who called.
            Poker.phase='Turn'; Poker.bet=50; Poker.minRaise=30; Poker.turn=1
            local hero=Poker.players[1]; hero.bet,hero.acted,hero.actedAt=20,true,20
            local other=Poker.players[2]; other.bet,other.acted,other.actedAt=50,true,50
            Poker.raise=10
            local before=SaveState(); Action('raise'); assert(SaveState()==before,'illegal subminimum raise')
            hero.actedAt=40; Poker.raise=30
            before=SaveState(); Action('raise'); assert(SaveState()==before,'short all-in reopened betting')
        )");
        Init(game,"texas_holdem");
        Script(game,R"(
            for i=3,8 do Poker.players[i].chips=0 end
            Poker.players[1].chips,Poker.players[2].chips=100,5
            Poker.dealer=2; Action('start')
            assert(Poker.bet==10 and Poker.turn==0 and Poker.pot==15,'short blind demanded an unmatched call')
            Action('tick'); assert(Poker.phase=='Flop' and Poker.turn==0)
        )");
        Init(game,"texas_holdem");
        Script(game,R"(
            -- Both short stacks go all-in. The board must run out without
            -- demanding impossible actions, and unmatched chips must return.
            for i=3,8 do Poker.players[i].chips=0 end
            Poker.players[1].chips,Poker.players[2].chips=10,5
            Poker.dealer=2; Action('start')
            assert(Poker.players[1].chips==0 and Poker.players[2].chips==0 and Poker.turn==0)
            for i=1,4 do Action('tick') end
            assert(Poker.phase=='Showdown' or Poker.phase=='Finished' or Poker.phase=='Eliminated')
            assert(Poker.pot==0 and Poker.players[1].chips+Poker.players[2].chips==15)
            for i=2,6 do assert(p:get(i).cards:size()==1) end; conservation()
            -- A contribution beyond every opponent's total is a refund,
            -- even when that contributor folded.
            clear(); Poker.phase='River'; Poker.pot=50
            for i=1,8 do Poker.players[i].total,Poker.players[i].folded=0,true end
            Poker.players[1].total,Poker.players[1].folded=20,false
            Poker.players[2].total=30; add(7,1,0); add(7,1,1)
            PokerSettle(p)
            assert(Poker.players[1].payout==40 and Poker.players[2].payout==10,'unmatched folded contribution')
        )");
        Init(game,"texas_holdem");
        Script(game,R"(
            Action('start')
            -- A short all-in raises the amount to call but does not reset
            -- previously acted seats or the last full raise size.
            Poker.turn,Poker.bet,Poker.minRaise,Poker.raise=1,20,20,20
            local hero=Poker.players[1]; hero.chips,hero.bet,hero.total,hero.acted=25,0,0,false
            local other=Poker.players[2]; other.bet,other.total,other.acted,other.actedAt=20,20,true,20
            Poker.players[3].bet,Poker.players[3].total=20,20; Poker.pot=40
            Action('raise')
            assert(hero.chips==0 and Poker.bet==25 and Poker.minRaise==20 and other.acted,'short all-in acted flags')
            UndoTest(); assert(Poker.players[1].chips==25 and Poker.bet==20)
            -- A full raise reopens every other eligible player.
            Poker.players[1].chips=100; Action('raise')
            assert(Poker.bet==40 and not Poker.players[2].acted and Poker.minRaise==20)
        )");
    }
};
int main() {
    try {
        RulesTestAccess::SolitaireRules();
        RulesTestAccess::PuzzleAndTutorial();
        RulesTestAccess::PokerRules();
        std::cout << "All bundled Lua rule regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Rule test failure: " << error.what() << '\n';
        return 1;
    }
}
