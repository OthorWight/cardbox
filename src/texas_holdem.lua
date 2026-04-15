GameName = "Hold'em"
NumDecks = 1
AutoCenter = true
HelpText = "Fully playable 8-Player Texas Hold'em against 7 Bots!\nIncludes betting, folding, and hand evaluations."

-- 0: Ready to Deal, 1: Flop, 2: Turn, 3: River, 4: Showdown, 5: Done
GameState = "Ready" 
Dealer = 8
CurrentTurn = 1
Pot = 0
CurrentBet = 0
Players = {}
LogMsg = "Welcome to Texas Hold'em!"
PilesRef = nil
BotDelay = 0.0
PlayerRaiseAmount = 20
SmallBlindIdx = 0
BigBlindIdx = 0
NextHandTimer = 0.0

p_coords = {
    ImVec2.new(645.0, 520.0), -- P1 (Human, Bottom)
    ImVec2.new(300.0, 470.0), -- P2 (Bottom Left)
    ImVec2.new(180.0, 280.0), -- P3 (Mid Left)
    ImVec2.new(300.0, 90.0),  -- P4 (Top Left)
    ImVec2.new(645.0, 40.0),  -- P5 (Top)
    ImVec2.new(990.0, 90.0),  -- P6 (Top Right)
    ImVec2.new(1110.0, 280.0),-- P7 (Mid Right)
    ImVec2.new(990.0, 470.0)  -- P8 (Bottom Right)
}

function Init(piles, deck)
    GameState = "Ready"
    for i=1,8 do
        Players[i] = { chips = 1000, bet = 0, folded = false, is_bot = (i ~= 1), hand_eval = "", acted_this_round = false }
    end
    Dealer = 8
    Pot = 0
    CurrentBet = 0
    LogMsg = "Welcome to Texas Hold'em!"
    BotDelay = 0.0
    NextHandTimer = 0.0
    
    local stock = Pile.new()
    stock.id = 0
    stock.type = PileType.Stock
    stock.pos = ImVec2.new(1150.0, 50.0)
    stock.size = ImVec2.new(100.0, 140.0)
    stock.offset = ImVec2.new(0.2, -0.5)
    piles:push_back(stock)

    local burn = Pile.new()
    burn.id = 1
    burn.type = PileType.Invisible
    burn.pos = ImVec2.new(-1000.0, -1000.0)
    burn.size = ImVec2.new(100.0, 140.0)
    burn.offset = ImVec2.new(0, 0)
    piles:push_back(burn)

    for i = 0, 4 do
        local p = Pile.new()
        p.id = 2 + i
        p.type = PileType.FreeCellSlot
        p.pos = ImVec2.new(430.0 + i * 110.0, 280.0)
        p.size = ImVec2.new(100.0, 140.0)
        p.offset = ImVec2.new(0, 0)
        piles:push_back(p)
    end

    for i = 1, 8 do
        local p = Pile.new()
        p.id = 6 + i
        p.type = PileType.Tableau
        p.pos = p_coords[i]
        p.size = ImVec2.new(100.0, 140.0)
        p.offset = ImVec2.new(30.0, 0.0)
        piles:push_back(p)
    end

    while not deck:empty() do
        local c = deck:back()
        c.faceUp = false
        piles:get(0).cards:push_back(c)
        deck:pop_back()
    end
end

-- -------------------------------------------------------------
-- Poker Hand Evaluator
-- -------------------------------------------------------------
function EvaluateHand(cards)
    local r_counts = {}; local s_counts = {}
    for i=2,14 do r_counts[i] = {} end
    for i=0,3 do s_counts[i] = {} end
        for c_idx=1, #cards do
        local c = cards[c_idx]
        local r = c.rank == 1 and 14 or c.rank
        table.insert(r_counts[r], c)
        table.insert(s_counts[c.suit], c)
    end

    local flush_cards = nil
    for s=0,3 do
        if #s_counts[s] >= 5 then
            flush_cards = s_counts[s]
            table.sort(flush_cards, function(a,b)
                local ar = a.rank==1 and 14 or a.rank
                local br = b.rank==1 and 14 or b.rank
                return ar > br
            end)
            break
        end
    end

    local function get_straight(c_list)
        local present = {}
        for _, c in ipairs(c_list) do
            local r = c.rank==1 and 14 or c.rank
            present[r] = true
            if r == 14 then present[1] = true end
        end
        local streak = 0; local high = 0
        for r=14,1,-1 do
            if present[r] then
                if streak == 0 then high = r end
                streak = streak + 1
                if streak >= 5 then return high end
            else
                streak = 0
            end
        end
        return nil
    end

    local str_high = get_straight(cards)
    local sf_high = flush_cards and get_straight(flush_cards) or nil
    if sf_high then return 8000000 + sf_high, "Straight Flush" end

    local pairs = {}; local trips = {}; local quads = nil
    for r=14,2,-1 do
        local n = #r_counts[r]
        if n == 4 then quads = r
        elseif n == 3 then table.insert(trips, r)
        elseif n == 2 then table.insert(pairs, r)
        end
    end

    local function get_kickers(exclude, count)
        local k = {}
        for r=14,2,-1 do
            local skip = false
            for _, ex in ipairs(exclude) do if r == ex then skip = true break end end
            if not skip then
                for i=1,#r_counts[r] do
                    table.insert(k, r)
                    if #k == count then return k end
                end
            end
        end
        while #k < count do table.insert(k, 0) end
        return k
    end

    if quads then
        local k = get_kickers({quads}, 1)
        return 7000000 + quads * 16 + k[1], "Four of a Kind"
    end
    if #trips >= 1 and #pairs >= 1 then return 6000000 + trips[1] * 16 + pairs[1], "Full House" end
    if #trips >= 2 then return 6000000 + trips[1] * 16 + trips[2], "Full House" end

    if flush_cards then
        local s = 5000000; local m = 65536
        for i=1,5 do
            local r = flush_cards[i].rank==1 and 14 or flush_cards[i].rank
            s = s + r * m
            m = m / 16
        end
        return s, "Flush"
    end

    if str_high then return 4000000 + str_high, "Straight" end

    if #trips >= 1 then
        local k = get_kickers({trips[1]}, 2)
        return 3000000 + trips[1] * 256 + k[1] * 16 + k[2], "Three of a Kind"
    end
    if #pairs >= 2 then
        local k = get_kickers({pairs[1], pairs[2]}, 1)
        return 2000000 + pairs[1] * 256 + pairs[2] * 16 + k[1], "Two Pair"
    end
    if #pairs >= 1 then
        local k = get_kickers({pairs[1]}, 3)
        return 1000000 + pairs[1] * 4096 + k[1] * 256 + k[2] * 16 + k[3], "Pair"
    end

    local k = get_kickers({}, 5)
    return k[1] * 65536 + k[2] * 4096 + k[3] * 256 + k[4] * 16 + k[5], "High Card"
end

-- -------------------------------------------------------------
-- Game Logic
-- -------------------------------------------------------------
function DealCard(piles, targetPileIdx, faceUp)
    local deck = piles:get(0)
    if not deck.cards:empty() then
        local c = deck.cards:back()
        deck.cards:pop_back()
        c.faceUp = faceUp
        piles:get(targetPileIdx).cards:push_back(c)
    end
end

function ShuffleDeck(deck)
    for i=0, deck.cards:size()-1 do
        local j = math.random(0, deck.cards:size()-1)
        local c1 = deck.cards:get(i)
        local c2 = deck.cards:get(j)
        local tr, ts = c1.rank, c1.suit
        c1.rank, c1.suit = c2.rank, c2.suit
        c2.rank, c2.suit = tr, ts
    end
end

function StartBettingRound(piles, phase)
    GameState = phase
    for i=1,8 do 
        Players[i].bet = 0 
        Players[i].acted_this_round = false
    end
    CurrentBet = 0
    PlayerRaiseAmount = 20
    
    local first_actor = (Dealer % 8) + 1
    if phase == "PreFlop" then
        for r=1,2 do
            for i=1,8 do
                if not Players[i].folded then
                    local p_idx = i == 1 and 7 or (i + 6)
                    DealCard(piles, p_idx, i == 1)
                end
            end
        end
        
        local sb_idx = first_actor
        while Players[sb_idx].chips == 0 do sb_idx = (sb_idx % 8) + 1 end
        local bb_idx = (sb_idx % 8) + 1
        while Players[bb_idx].chips == 0 do bb_idx = (bb_idx % 8) + 1 end
        
        SmallBlindIdx = sb_idx
        BigBlindIdx = bb_idx

        PlaceBet(sb_idx, 10)
        PlaceBet(bb_idx, 20)
        CurrentBet = 20
        first_actor = (bb_idx % 8) + 1
    end

    local loop_safe = 0
    while (Players[first_actor].folded or Players[first_actor].chips == 0) and loop_safe < 8 do
        first_actor = (first_actor % 8) + 1
        loop_safe = loop_safe + 1
    end
    
    CurrentTurn = first_actor
    if Players[CurrentTurn].is_bot then BotDelay = 0.5 end
end

function PlaceBet(idx, amount)
    local p = Players[idx]
    if p.chips < amount then amount = p.chips end
    p.chips = p.chips - amount
    p.bet = p.bet + amount
    Pot = Pot + amount
end

function DoShowdown(piles)
    for i=8,14 do
        local p = piles:get(i)
        for c_idx=0, p.cards:size()-1 do p.cards:get(c_idx).faceUp = true end
    end

    local comm = {}
    for i=2,6 do
        local p = piles:get(i)
        if not p.cards:empty() then table.insert(comm, p.cards:get(0)) end
    end

    local best_score = -1
    local winners = {}

    for i=1,8 do
        if not Players[i].folded then
            local p_idx = i == 1 and 7 or (i + 6)
            local hand = {}
            for _, c in ipairs(comm) do table.insert(hand, c) end
            local p = piles:get(p_idx)
            for c_idx=0, p.cards:size()-1 do table.insert(hand, p.cards:get(c_idx)) end
            
            local score, name = EvaluateHand(hand)
            Players[i].hand_eval = name
            if score > best_score then
                best_score = score
                winners = {i}
            elseif score == best_score then
                table.insert(winners, i)
            end
        end
    end

    local pot_split = math.floor(Pot / #winners)
    local win_names = ""
    for _, w in ipairs(winners) do
        Players[w].chips = Players[w].chips + pot_split
        win_names = win_names .. "Player " .. w .. " "
    end
    local total_won = Pot
    LogMsg = win_names .. "wins " .. total_won .. " chips with " .. Players[winners[1]].hand_eval .. "!"
    NextHandTimer = 10.0
end

function AdvancePhase(piles)
    if GameState == "PreFlop" then
        DealCard(piles, 1, false) 
        for i=2,4 do DealCard(piles, i, true) end
        StartBettingRound(piles, "Flop")
    elseif GameState == "Flop" then
        DealCard(piles, 1, false)
        DealCard(piles, 5, true)
        StartBettingRound(piles, "Turn")
    elseif GameState == "Turn" then
        DealCard(piles, 1, false)
        DealCard(piles, 6, true)
        StartBettingRound(piles, "River")
    elseif GameState == "River" then
        GameState = "Showdown"
        DoShowdown(piles)
    end
end

function CheckWinEarly(piles)
    local active_count = 0
    local winner = 0
    for i=1,8 do if not Players[i].folded then active_count = active_count + 1; winner = i end end
    if active_count == 1 then
        local won_amount = Pot
        Players[winner].chips = Players[winner].chips + Pot
        GameState = "Showdown"
        LogMsg = "Player " .. winner .. " wins " .. won_amount .. " chips by default!"
        NextHandTimer = 10.0
        return true
    end
    return false
end

function CheckRoundOver()
    local active_count = 0
    for i=1,8 do
        if not Players[i].folded and Players[i].chips > 0 then
            active_count = active_count + 1
        end
    end

    local unsettled = false
    for i=1,8 do
        if not Players[i].folded and Players[i].chips > 0 then
            if Players[i].bet < CurrentBet then
                unsettled = true
            elseif not Players[i].acted_this_round and active_count > 1 then
                unsettled = true
            end
        end
    end
    
    return not unsettled
end

function FoldPlayer(piles, idx)
    Players[idx].folded = true
    Players[idx].acted_this_round = true
    local p_idx = idx == 1 and 7 or (idx + 6)
    local p = piles:get(p_idx)
    local burn = piles:get(1)
    while not p.cards:empty() do
        local c = p.cards:back()
        p.cards:pop_back()
        burn.cards:push_back(c)
    end
end

function NextTurn(piles)
    if CheckWinEarly(piles) then return end
    if CheckRoundOver() then return end

    local start = CurrentTurn
    repeat
        CurrentTurn = (CurrentTurn % 8) + 1
        if CurrentTurn == start then break end
    until not Players[CurrentTurn].folded and Players[CurrentTurn].chips > 0

    if Players[CurrentTurn].is_bot then BotDelay = 0.5 end
end

function TakeBotAction(piles, idx)
    local p = Players[idx]
    p.acted_this_round = true
    local to_call = CurrentBet - p.bet
    
    -- 1. Gather cards visible to the bot (hole cards + community)
    local hand = {}
    local comm = {}
    for i=2,6 do
        local cp = piles:get(i)
        if not cp.cards:empty() then
            table.insert(hand, cp.cards:get(0))
            table.insert(comm, cp.cards:get(0))
        end
    end
    local p_idx = idx == 1 and 7 or (idx + 6)
    local player_pile = piles:get(p_idx)
    for c_idx=0, player_pile.cards:size()-1 do
        table.insert(hand, player_pile.cards:get(c_idx))
    end

    -- 2. Evaluate the hand and calculate a rough strength (0.0 to 1.0)
    local score, _ = EvaluateHand(hand)
    local comm_score = 0
    if #comm > 0 then
        comm_score, _ = EvaluateHand(comm)
    end
    local strength = 0.0
    
    -- Score > 2,000,000 is Two Pair or better
    if score >= 2000000 then 
        strength = 0.9
    -- Score > 1,000,000 is a Pair
    elseif score >= 1000000 then 
        strength = 0.6
    -- High card: scale based on highest card (Aces = ~917504)
    else 
        strength = math.min(0.4, score / 2500000.0) 
    end

    -- If our hand score is exactly equal to the board's score, we are "playing the board".
    -- We have no advantage and should drastically reduce our betting strength.
    if score == comm_score then
        if score >= 4000000 then
            strength = 0.4 -- The board is very strong (Straight+), be cautious but willing to call small bets
        else
            strength = 0.1 -- The board is weak and we have nothing, give up
        end
    end

    -- Add a little bit of bluffing/randomness variance (-0.2 to +0.2)
    strength = strength + (math.random() * 0.4 - 0.2)

    -- Calculate pot odds (how much it costs to call relative to the pot)
    local pot_odds = to_call / (Pot + to_call)
    
    -- Consider the bot "pot committed" if their remaining stack is tiny compared to the pot
    local pot_committed = (p.chips < Pot * 0.2)

    if to_call > 0 then
        if strength < pot_odds and not pot_committed then
            FoldPlayer(piles, idx)
            LogMsg = "Bot " .. idx .. " folded."
        elseif strength < 0.7 or p.chips <= to_call then
            PlaceBet(idx, to_call)
            LogMsg = "Bot " .. idx .. " calls " .. to_call .. "."
        else
            -- Raise sizing: 50% to 100% of the current pot!
            local raise = to_call + math.floor(Pot * (math.random(5, 10) / 10.0))
            PlaceBet(idx, raise)
            CurrentBet = p.bet
            LogMsg = "Bot " .. idx .. " raises to " .. p.bet .. "!"
        end
    else
        if strength < 0.5 then
            LogMsg = "Bot " .. idx .. " checks."
        else
            -- Bet sizing: 50% to 100% of the current pot!
            local bet_amount = math.floor(Pot * (math.random(5, 10) / 10.0))
            if bet_amount < 20 then bet_amount = 20 end -- Minimum bet
            PlaceBet(idx, bet_amount)
            CurrentBet = p.bet
            LogMsg = "Bot " .. idx .. " bets " .. bet_amount .. "."
        end
    end
end

function ResetHand(piles)
    for i=1,14 do
        local p = piles:get(i)
        while not p.cards:empty() do
            local c = p.cards:back()
            p.cards:pop_back()
            c.faceUp = false
            piles:get(0).cards:push_back(c)
        end
    end
    ShuffleDeck(piles:get(0))

    Dealer = (Dealer % 8) + 1
    Pot = 0
    local active_players = 0
    for i=1,8 do
        Players[i].bet = 0
        Players[i].hand_eval = ""
        if Players[i].chips == 0 then
            Players[i].folded = true
        else
            Players[i].folded = false
            active_players = active_players + 1
        end
    end

    if active_players < 2 then
        GameState = "Showdown"
        LogMsg = "Game over! Not enough players. Press F2 to restart."
        NextHandTimer = 0.0
        return
    end
    StartBettingRound(piles, "PreFlop")
end

-- Disable manual dragging
function CanPickup(piles, pileIdx, cardIdx) return false end
function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards) return false end
function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx) end
function HandleClick(piles, pileIdx) end

-- -------------------------------------------------------------
-- Game Loop & Rendering
-- -------------------------------------------------------------
function AutoSolve(piles)
    PilesRef = piles
    if GameState == "Showdown" then
        if NextHandTimer > 0 then
            NextHandTimer = NextHandTimer - 0.016
            if NextHandTimer <= 0 then
                ResetHand(piles)
            end
        end
        return {}
    end
    if GameState == "Ready" then return {} end
    
    if CheckRoundOver() then
        AdvancePhase(piles)
        return {}
    end

    if Players[CurrentTurn].is_bot and not Players[CurrentTurn].folded and Players[CurrentTurn].chips > 0 then
        BotDelay = BotDelay - 0.016
        if BotDelay <= 0 then
            TakeBotAction(piles, CurrentTurn)
            NextTurn(piles)
        end
    end
    return {}
end

function IsWon(piles)
    return false
end

function Draw()
    DrawBoardText(150.0, 30.0, "Pot: " .. Pot)
    if LogMsg then DrawBoardText(150.0, 0.0, LogMsg) end
    
    for i=1,8 do
        local px, py = p_coords[i].x, p_coords[i].y
        
        local role = ""
        if GameState ~= "Ready" then
            if i == Dealer then role = " [D]"
            elseif i == SmallBlindIdx then role = " [SB]"
            elseif i == BigBlindIdx then role = " [BB]" end
        end
        
        local info = "P" .. i .. role .. " Chips: " .. Players[i].chips
        if Players[i].folded then info = info .. " (Folded)"
        elseif CurrentTurn == i and GameState ~= "Showdown" and GameState ~= "Ready" then info = ">> " .. info .. " <<" end
        if Players[i].bet > 0 then info = info .. "\nBet: " .. Players[i].bet end
        if GameState == "Showdown" and Players[i].hand_eval ~= "" then info = info .. "\n" .. Players[i].hand_eval end
        
        DrawBoardText(px, py + 150.0, info)
    end

    if CurrentTurn == 1 and GameState ~= "Showdown" and GameState ~= "Ready" and not Players[1].folded and Players[1].chips > 0 then
        local to_call = CurrentBet - Players[1].bet
        
        if DrawBoardButton(550, 425, 100, 40, "Fold") then
            FoldPlayer(PilesRef, 1)
            LogMsg = "You folded."
            NextTurn(PilesRef)
        end
        
        local call_text = to_call > 0 and ("Call " .. to_call) or "Check"
        if DrawBoardButton(670, 425, 100, 40, call_text) then
            PlaceBet(1, to_call)
            Players[1].acted_this_round = true
            LogMsg = "You " .. (to_call > 0 and "called." or "checked.")
            NextTurn(PilesRef)
        end
        
        local can_raise = Players[1].chips > to_call
        if can_raise then
            local min_raise = math.max(20, CurrentBet)
            local max_raise = Players[1].chips - to_call
            if min_raise > max_raise then min_raise = max_raise end

            if PlayerRaiseAmount > max_raise then PlayerRaiseAmount = max_raise end
            if PlayerRaiseAmount < min_raise then PlayerRaiseAmount = min_raise end

            if DrawBoardButton(550, 475, 100, 40, "Min") then PlayerRaiseAmount = min_raise end
            if DrawBoardButton(670, 475, 100, 40, "1/2 Pot") then PlayerRaiseAmount = math.min(max_raise, math.max(min_raise, math.floor(Pot / 2))) end

            if DrawBoardButton(790, 475, 30, 40, "-") then PlayerRaiseAmount = math.max(min_raise, PlayerRaiseAmount - 10) end
            if DrawBoardButton(825, 475, 30, 40, "+") then PlayerRaiseAmount = math.min(max_raise, PlayerRaiseAmount + 10) end
            if DrawBoardButton(860, 475, 40, 40, "Max") then PlayerRaiseAmount = max_raise end

            if DrawBoardButton(790, 425, 110, 40, "Raise " .. PlayerRaiseAmount) then
                PlaceBet(1, to_call + PlayerRaiseAmount)
                CurrentBet = Players[1].bet
                Players[1].acted_this_round = true
                LogMsg = "You raised " .. PlayerRaiseAmount .. "!"
                PlayerRaiseAmount = math.max(20, CurrentBet)
                NextTurn(PilesRef)
            end
        end
    end

    if GameState == "Showdown" or GameState == "Ready" then
        local btn_text = GameState == "Ready" and "Start Game" or "Next Hand"
        if GameState == "Showdown" and NextHandTimer > 0 then
            btn_text = btn_text .. " (" .. math.ceil(NextHandTimer) .. ")"
        end
        if DrawBoardButton(600, 450, 200, 50, btn_text) then
            ResetHand(PilesRef)
        end
    end
end