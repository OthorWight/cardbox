GameName = "Crawler"
NumDecks = 1
AutoCenter = true
HelpText = "Crawler!\nSpades = Monsters (Take Damage)\nClubs = Weapons (Equip to reduce Damage)\nHearts = Health Potion (Heal)\nDiamonds = Treasure (Score)\n\nYou must clear the room to advance. You can flee if 1 card is left, but you CANNOT flee from a lone monster!"

HP = 20
MaxHP = 20
Score = 0
LogMsg = ""
PilesRef = nil

-- Calculate the RPG value of a card (Face value, J=11, Q=12, K=13, A=14)
local function GetRankValue(c)
    if c.rank == 1 then return 14 end -- Ace is the strongest
    return c.rank
end

function Init(piles, deck)
    HP = 20
    Score = 0
    LogMsg = "Welcome to the Dungeon! Click cards to interact."
    PilesRef = nil

    local padX = 120.0
    local startX = 300.0
    local startY = 250.0

    -- 0-3: The Room (Tableau)
    for i = 0, 3 do
        local p = Pile.new()
        p.id = i
        p.type = PileType.Tableau
        p.pos = ImVec2.new(startX + i * padX, startY)
        p.size = ImVec2.new(100.0, 140.0)
        p.offset = ImVec2.new(0, 0)
        piles:push_back(p)
    end

    -- 4: Weapon Slot (FreeCellSlot)
    local w = Pile.new()
    w.id = 4
    w.type = PileType.FreeCellSlot
    w.pos = ImVec2.new(startX, startY + 200.0)
    w.size = ImVec2.new(100.0, 140.0)
    w.offset = ImVec2.new(0, 0)
    piles:push_back(w)

    -- 5: Deck (Stock / Unexplored Dungeon)
    local s = Pile.new()
    s.id = 5
    s.type = PileType.Stock
    s.pos = ImVec2.new(20.0, startY)
    s.size = ImVec2.new(100.0, 140.0)
    s.offset = ImVec2.new(0.2, -0.5)
    piles:push_back(s)

    -- 6: Discard (Waste / Graveyard)
    local d = Pile.new()
    d.id = 6
    d.type = PileType.Waste
    d.pos = ImVec2.new(startX + 4 * padX, startY)
    d.size = ImVec2.new(100.0, 140.0)
    d.offset = ImVec2.new(0.1, 0.0)
    piles:push_back(d)

    -- Load deck into stock
    local stock = piles:get(5)
    while not deck:empty() do
        local c = deck:back()
        c.faceUp = false
        stock.cards:push_back(c)
        deck:pop_back()
    end

    -- Deal the initial 4 cards to the room
    for i = 0, 3 do
        if not stock.cards:empty() then
            local c = stock.cards:back()
            stock.cards:pop_back()
            c.faceUp = true
            piles:get(i).cards:push_back(c)
        end
    end
end

-- Disable manual drag & drop. Everything is handled by clicking.
function CanPickup(piles, pileIdx, cardIdx) return false end
function CanDrop(piles, sourceIdx, targetIdx, dragCards) return false end
function AfterMove(piles, sourceIdx, targetIdx, cardIdx) end

-- Keep track of global piles for the Draw() HUD
function AutoSolve(piles)
    PilesRef = piles
    return {}
end

function HandleClick(piles, pileIdx)
    if HP <= 0 then return end

    local discard = piles:get(6)

    -- 1. Clicked the Deck (Attempt to advance to next room)
    if pileIdx == 5 then
        local total = 0
        local monsters = 0
        for i = 0, 3 do
            local p = piles:get(i)
            if not p.cards:empty() then
                total = total + 1
                if p.cards:back().suit == Suit.Spades then monsters = monsters + 1 end
            end
        end

        if total <= 1 and monsters == 0 then
            local stock = piles:get(5)
            local dealt = 0
            for i = 0, 3 do
                local p = piles:get(i)
                if p.cards:empty() and not stock.cards:empty() then
                    local c = stock.cards:back()
                    stock.cards:pop_back()
                    c.faceUp = true
                    p.cards:push_back(c)
                    dealt = dealt + 1
                end
            end
            if dealt > 0 then LogMsg = "You advanced deeper into the dungeon."
            else LogMsg = "The dungeon is empty!" end
        elseif total <= 1 and monsters > 0 then
            LogMsg = "You must kill the last monster before fleeing!"
        else
            LogMsg = "You must clear more cards before advancing."
        end
        return
    end

    -- 2. Clicked a Room Card (Interact)
    if pileIdx >= 0 and pileIdx <= 3 then
        local p = piles:get(pileIdx)
        if p.cards:empty() then return end

        local c = p.cards:back()
        local val = GetRankValue(c)
        p.cards:pop_back()

        if c.suit == Suit.Hearts then
            local healed = math.min(MaxHP - HP, val)
            HP = HP + healed
            LogMsg = "Drank a potion: +" .. healed .. " HP"
            discard.cards:push_back(c)
        elseif c.suit == Suit.Diamonds then
            Score = Score + val
            LogMsg = "Looted treasure: +" .. val .. " Gold"
            discard.cards:push_back(c)
        elseif c.suit == Suit.Clubs then
            local weaponSlot = piles:get(4)
            if not weaponSlot.cards:empty() then
                -- Discard old weapon
                local old_w = weaponSlot.cards:back()
                weaponSlot.cards:pop_back()
                discard.cards:push_back(old_w)
            end
            weaponSlot.cards:push_back(c)
            LogMsg = "Equipped new weapon: " .. val .. " ATK"
        elseif c.suit == Suit.Spades then
            local weaponSlot = piles:get(4)
            local w_val = 0
            if not weaponSlot.cards:empty() then w_val = GetRankValue(weaponSlot.cards:back()) end

            local damage = math.max(0, val - w_val)
            HP = HP - damage
            
            if damage > 0 then LogMsg = "Fought a monster! Took " .. damage .. " damage."
            else LogMsg = "Fought a monster! Blocked all damage." end

            -- Weapon breaks if the monster is as strong or stronger than it
            if w_val > 0 and val >= w_val then
                LogMsg = LogMsg .. " Your weapon broke!"
                local broken = weaponSlot.cards:back()
                weaponSlot.cards:pop_back()
                discard.cards:push_back(broken)
            end

            discard.cards:push_back(c)
            if HP <= 0 then LogMsg = "You were slain by the monster! GAME OVER." end
        end
    end
end

function IsWon(piles)
    if HP <= 0 then return false end
    local roomEmpty = true
    for i = 0, 3 do
        if not piles:get(i).cards:empty() then
            roomEmpty = false
            break
        end
    end
    -- Game is won if the deck is empty AND the room is clear
    return piles:get(5).cards:empty() and roomEmpty
end

function Draw()
    DrawBoardText(300.0, 30.0, string.format("HP: %d / %d", HP, MaxHP))
    DrawBoardText(300.0, 60.0, string.format("Gold: %d", Score))
    
    if LogMsg ~= "" then
        DrawBoardText(300.0, 100.0, LogMsg)
    end

    DrawBoardText(300.0, 220.0, "The Room (Click to interact)")
    DrawBoardText(300.0, 430.0, "Equipped Weapon (Clubs)")
    DrawBoardText(20.0,  220.0, "Dungeon Deck")
    DrawBoardText(780.0, 220.0, "Graveyard")
    
    if HP <= 0 then
        DrawBoardText(300.0, 500.0, "DEFEAT! Press F2 to Restart.")
    elseif PilesRef and IsWon(PilesRef) then
        DrawBoardText(300.0, 500.0, "VICTORY! You survived the dungeon.")
    end
end