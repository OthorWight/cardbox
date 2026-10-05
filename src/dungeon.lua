GameName = "Crawler"
NumDecks = 1
AutoCenter = false
Theme = {
    Background = {13, 18, 27}, BackgroundBottom = {10, 14, 22},
    Toolbar = {19, 25, 36}, ButtonRounding = 6,
    EmptyPile = {19, 25, 36, 180}, EmptyPileBorder = {66, 80, 98},
    EmptyPileText = {155, 169, 184}, CardBack = {38, 54, 75},
    CardBorder = {97, 112, 131}, CardHover = {244, 201, 116},
    Colors = {
        Text = {235, 229, 213}, TextDisabled = {128, 142, 159},
        WindowBg = {19, 25, 36}, ChildBg = {19, 25, 36},
        PopupBg = {25, 32, 44}, MenuBarBg = {19, 25, 36},
        Border = {52, 65, 82}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {38, 49, 65}, FrameBgHovered = {52, 65, 82},
        FrameBgActive = {66, 80, 98},
        Button = {43, 57, 76}, ButtonHovered = {62, 80, 101},
        ButtonActive = {79, 99, 121},
        Header = {43, 57, 76}, HeaderHovered = {62, 80, 101},
        HeaderActive = {79, 99, 121},
        TitleBg = {19, 25, 36}, TitleBgActive = {29, 37, 51},
        TitleBgCollapsed = {19, 25, 36},
        Separator = {52, 65, 82}, SeparatorHovered = {244, 201, 116},
        SeparatorActive = {244, 201, 116},
        ScrollbarBg = {13, 18, 27}, ScrollbarGrab = {52, 65, 82},
        ScrollbarGrabHovered = {66, 80, 98}, ScrollbarGrabActive = {97, 112, 131},
        CheckMark = {244, 201, 116}, SliderGrab = {244, 201, 116},
        SliderGrabActive = {255, 216, 146}, TextSelectedBg = {244, 201, 116, 65},
        NavCursor = {244, 201, 116}, ModalWindowDimBg = {6, 9, 15, 190}
    }
}
HelpText = [[Explore a dungeon of 52 cards. Clear the deck and every encounter to survive.

Spades are monsters. Fight with your weapon to reduce damage, or fight bare-handed to save its durability. Weapons last three strikes. Aces are worth 14.
Clubs are weapons. Equipping one replaces the old weapon and restores three strikes. You begin with the six of clubs.
Hearts are potions. Drink at most one per room. Keep a potion for the next room, or leave it behind.
Diamonds are treasure. Collect gold, or spend 8 gold to recover up to 4 HP at camp once per room.

Advance when the room is empty, or when only one non-monster remains; that card carries into the next room. Camp is available at the same point.
Retreat costs 2 HP and returns every remaining encounter to the bottom of the deck. You cannot retreat twice in a row, or retreat from the final room.

Click an encounter to take its main action. The buttons offer alternatives; hover a card to see the exact result. Undo restores health, gold, and weapon durability too.
Score: gold carried + 5 per monster defeated + 2 per remaining HP on victory. F2 starts a new run.]]

local ROOM, WEAPON, STOCK, DISCARD = 0, 4, 5, 6
local MAX_HP, STRIKES, CAMP_COST, CAMP_HEAL = 20, 3, 8, 4
local CARD_X, CARD_STEP, CARD_Y = 324, 141, 285
local FIELDS = {"hp", "gold", "room", "kills", "dur", "potions", "rested",
                "retreats", "canRetreat", "turns", "finished"}
local COLORS = {
    ink = Theme.Colors.Text, muted = Theme.EmptyPileText, gold = Theme.CardHover,
    monster = {245, 119, 131}, potion = {114, 210, 166}, weapon = {136, 190, 244}
}
Dungeon = {}
local PilesRef = nil
local Notice = ""

local function Value(card) return card.rank == 1 and 14 or card.rank end
local function RoomCount(piles)
    local count, monsters = 0, 0
    for i = ROOM, ROOM + 3 do
        local cards = piles:get(i).cards
        if not cards:empty() then
            count = count + 1
            if cards:back().suit == Suit.Spades then monsters = monsters + 1 end
        end
    end
    return count, monsters
end

local function Attack(piles)
    local cards = piles:get(WEAPON).cards
    return cards:empty() and 0 or Value(cards:back())
end

local function Quiet(piles)
    local count, monsters = RoomCount(piles)
    return count <= 1 and monsters == 0
end

local function Alive() return Dungeon.hp > 0 and Dungeon.finished == 0 end
local function UpdateScore()
    SetScore(Dungeon.gold + Dungeon.kills * 5 + (Dungeon.finished == 1 and Dungeon.hp * 2 or 0))
end

local function Log(message)
    table.insert(Dungeon.log, 1, message)
    while #Dungeon.log > 4 do table.remove(Dungeon.log) end
    Notice = ""
end

local function Finish(piles)
    if Dungeon.hp > 0 and piles:get(STOCK).cards:empty() and RoomCount(piles) == 0 then
        Dungeon.finished = 1
        Log("Dungeon cleared. You survived with " .. Dungeon.hp .. " HP!")
    end
    UpdateScore()
end

local function DealRoom(piles)
    local stock = piles:get(STOCK).cards
    local dealt = 0
    for i = ROOM, ROOM + 3 do
        local cards = piles:get(i).cards
        if cards:empty() and not stock:empty() then
            local card = stock:take_back()
            card.faceUp = true
            cards:push_back(card)
            dealt = dealt + 1
        end
    end
    return dealt
end

local function AddPile(piles, id, kind, x, y)
    local pile = Pile.new()
    pile.id, pile.type = id, kind
    pile.pos = ImVec2.new(x, y)
    pile.size = ImVec2.new(100, 140)
    pile.offset = ImVec2.new(0, 0)
    piles:push_back(pile)
end

function Init(piles, deck)
    Dungeon = {hp = MAX_HP, gold = 0, room = 1, kills = 0, dur = 0, potions = 0,
               rested = 0, retreats = 0, canRetreat = 1, turns = 0, finished = 0, log = {}}
    PilesRef, Notice = piles, ""
    for i = 0, 3 do AddPile(piles, i, PileType.Tableau, CARD_X + i * CARD_STEP, CARD_Y) end
    -- A tableau slot avoids the solitaire-specific "Free" placeholder.
    AddPile(piles, WEAPON, PileType.Tableau, 110, 487)
    AddPile(piles, STOCK, PileType.Stock, 110, 275)
    AddPile(piles, DISCARD, PileType.Waste, 1090, 495)
    while not deck:empty() do
        local card = deck:take_back()
        if card.suit == Suit.Clubs and card.rank == 6 and Dungeon.dur == 0 then
            card.faceUp = true
            piles:get(WEAPON).cards:push_back(card)
            Dungeon.dur = STRIKES
        else
            card.faceUp = false
            piles:get(STOCK).cards:push_back(card)
        end
    end
    DealRoom(piles)
    Log("Room 1. Choose the order of your encounters.")
    UpdateScore()
end

function CanPickup() return false end
function CanDrop() return false end
function AfterMove() end
function AutoSolve(piles) PilesRef = piles; return {} end

local function Resolve(piles, index, mode)
    if not Alive() or index < 0 or index > 3 then return end
    local cards = piles:get(index).cards
    if cards:empty() then return end
    local card = cards:back()
    local value, suit = Value(card), card.suit
    if mode == "leave" and suit ~= Suit.Hearts and suit ~= Suit.Clubs then return end
    if mode == "bare" and suit ~= Suit.Spades then return end
    if suit == Suit.Hearts and mode ~= "leave" then
        if Dungeon.potions == 1 then Notice = "One potion per room. Carry this one forward, or leave it behind."; return end
        if Dungeon.hp == MAX_HP then Notice = "Health is full. Save the potion for later, or leave it behind."; return end
    end

    -- take_back returns a value, preserving animation without retaining a
    -- reference to an element that has been removed from the vector.
    card = cards:take_back()
    local discard = piles:get(DISCARD).cards
    Dungeon.turns = Dungeon.turns + 1
    if mode == "leave" then
        discard:push_back(card)
        Log(suit == Suit.Hearts and "Left a potion behind." or "Kept your current weapon; left the spare behind.")
    elseif suit == Suit.Hearts then
        local healed = math.min(MAX_HP - Dungeon.hp, value)
        Dungeon.hp = Dungeon.hp + healed
        Dungeon.potions = 1
        discard:push_back(card)
        Log("Potion: recovered " .. healed .. " HP.")
    elseif suit == Suit.Diamonds then
        Dungeon.gold = Dungeon.gold + value
        discard:push_back(card)
        Log("Treasure: collected " .. value .. " gold.")
    elseif suit == Suit.Clubs then
        local weapon = piles:get(WEAPON).cards
        if not weapon:empty() then discard:push_back(weapon:take_back()) end
        weapon:push_back(card)
        Dungeon.dur = STRIKES
        Log("Equipped " .. value .. " ATK. Three strikes remaining.")
    elseif suit == Suit.Spades then
        local power = mode == "bare" and 0 or Attack(piles)
        local damage = math.max(0, value - power)
        Dungeon.hp = math.max(0, Dungeon.hp - damage)
        Dungeon.kills = Dungeon.kills + 1
        discard:push_back(card)
        local broke = false
        if power > 0 then
            Dungeon.dur = Dungeon.dur - 1
            if Dungeon.dur == 0 then
                discard:push_back(piles:get(WEAPON).cards:take_back())
                broke = true
            end
        end
        Log("Monster " .. value .. ": " .. damage .. " damage" ..
            (mode == "bare" and " (bare-handed)." or ".") .. (broke and " Weapon broke." or ""))
        if Dungeon.hp == 0 then Log("You fell in room " .. Dungeon.room .. ". Undo to rethink the fight, or press F2.") end
    end
    EmitBoardParticles(CARD_X + index * CARD_STEP + 50, CARD_Y + 70, 100, 140)
    Finish(piles)
end

local function Advance(piles)
    if not Alive() then return end
    if not Quiet(piles) then Notice = "Clear the room, or leave only one non-monster, before advancing."; return end
    if piles:get(STOCK).cards:empty() then Notice = "Final room. Resolve every remaining encounter to escape."; return end
    local carried = RoomCount(piles)
    if DealRoom(piles) == 0 then return end
    Dungeon.room = Dungeon.room + 1
    Dungeon.potions, Dungeon.rested, Dungeon.canRetreat = 0, 0, 1
    Dungeon.turns = Dungeon.turns + 1
    Log("Entered room " .. Dungeon.room .. (carried == 1 and " with one encounter carried forward." or "."))
end

local function Retreat(piles)
    if not Alive() then return end
    if Dungeon.canRetreat == 0 then Notice = "Clear this room before retreating again."; return end
    if Dungeon.hp <= 2 then Notice = "Retreat costs 2 HP. You need at least 3 HP to survive it."; return end
    if piles:get(STOCK).cards:empty() or RoomCount(piles) == 0 then Notice = "There is nowhere left to retreat."; return end
    local returned = VectorCard.new()
    for i = 0, 3 do
        local cards = piles:get(i).cards
        if not cards:empty() then
            local card = cards:take_back()
            card.faceUp = false
            returned:push_back(card)
        end
    end
    local stock = piles:get(STOCK)
    -- Prepend returned encounters, keeping the unexplored deck's order intact.
    for i = 0, stock.cards:size() - 1 do returned:push_back(stock.cards:get(i)) end
    stock.cards = returned
    Dungeon.hp = Dungeon.hp - 2
    Dungeon.retreats = Dungeon.retreats + 1
    Dungeon.room = Dungeon.room + 1
    Dungeon.potions, Dungeon.rested, Dungeon.canRetreat = 0, 0, 0
    Dungeon.turns = Dungeon.turns + 1
    DealRoom(piles)
    Log("Retreated for 2 HP. Encounters returned to the dungeon; no second retreat yet.")
end

local function Camp(piles)
    if not Alive() then return end
    if not Quiet(piles) then Notice = "Make the room safe before making camp."; return end
    if Dungeon.rested == 1 then Notice = "You already camped in this room."; return end
    if Dungeon.gold < CAMP_COST then Notice = "Camp costs 8 gold."; return end
    if Dungeon.hp == MAX_HP then Notice = "You are already at full health."; return end
    local healed = math.min(CAMP_HEAL, MAX_HP - Dungeon.hp)
    Dungeon.gold = Dungeon.gold - CAMP_COST
    Dungeon.hp = Dungeon.hp + healed
    Dungeon.rested = 1
    Dungeon.turns = Dungeon.turns + 1
    Log("Camp: spent 8 gold to recover " .. healed .. " HP.")
    UpdateScore()
end

function HandleClick(piles, index)
    PilesRef = piles
    Notice = ""
    if index == STOCK then Advance(piles)
    elseif index >= 0 and index <= 3 then Resolve(piles, index, "main") end
end

function HandleAction(piles, action)
    PilesRef = piles
    Notice = ""
    if action == "hint" then
        if not Alive() then Notice = "The run has ended. Undo to revisit a choice, or start a new run."; return end
        local best, priority, advice = nil, -1, ""
        local power = Attack(piles)
        for i = 0, 3 do
            local cards = piles:get(i).cards
            if not cards:empty() then
                local card, rank = cards:back(), Value(cards:back())
                local weight, text = 0, ""
                if card.suit == Suit.Diamonds then
                    weight, text = 100, "Loot encounter " .. (i + 1) .. " for " .. rank .. " gold."
                elseif card.suit == Suit.Clubs then
                    weight = rank > power and 90 or Dungeon.dur <= 1 and 80 or 10
                    text = "Encounter " .. (i + 1) .. ": " .. rank .. " ATK with three fresh strikes; compare your current weapon."
                elseif card.suit == Suit.Hearts then
                    weight = Dungeon.potions == 0 and Dungeon.hp < MAX_HP and 95 or 5
                    text = "Encounter " .. (i + 1) .. ": heal up to " .. math.min(rank, MAX_HP - Dungeon.hp) .. " HP. Save it if you cannot drink yet."
                else
                    local damage = math.max(0, rank - power)
                    weight, text = 50 - damage, "Fight encounter " .. (i + 1) .. ": " .. damage .. " HP damage"
                    text = text .. (power > 0 and ", using one weapon strike." or " with bare hands.")
                    if damage >= Dungeon.hp then weight, text = -10, "Combat would be fatal. Consider a retreat, potion, or stronger weapon." end
                end
                if weight > priority then best, priority, advice = i, weight, text end
            end
        end
        Notice = best and advice or "The room is clear. Enter the next room, or camp if you need health."
    elseif action == "advance" then Advance(piles)
    elseif action == "retreat" then Retreat(piles)
    elseif action == "camp" then Camp(piles)
    else
        local mode, index = string.match(action, "^(%a+):([0-3])$")
        if mode == "main" or mode == "bare" or mode == "leave" then Resolve(piles, tonumber(index), mode) end
    end
end

function IsWon(piles)
    return Dungeon.hp > 0 and piles:get(STOCK).cards:empty() and RoomCount(piles) == 0
end

-- Only scalar run data and the event log enter the snapshot. Pile references
-- and presentation notices are recreated rather than serialized or evaluated.
function SaveState()
    local fields = {}
    for _, key in ipairs(FIELDS) do fields[#fields + 1] = tostring(Dungeon[key]) end
    return table.concat(fields, ",") .. "\n" .. table.concat(Dungeon.log, "\n")
end

function LoadState(piles, data)
    local numbers, log = string.match(data, "^([^\n]+)\n(.*)$")
    assert(numbers and log, "Invalid dungeon snapshot")
    local state, index = {log = {}}, 1
    for token in string.gmatch(numbers, "[^,]+") do
        local value = tonumber(token)
        assert(value and FIELDS[index], "Invalid dungeon snapshot field")
        state[FIELDS[index]], index = value, index + 1
    end
    assert(index == #FIELDS + 1, "Incomplete dungeon snapshot")
    for line in string.gmatch(log, "[^\n]+") do state.log[#state.log + 1] = line end
    Dungeon, PilesRef, Notice = state, piles, ""
end

local function Text(x, y, text, color, size, width)
    local rgb = COLORS[color or "ink"]
    DrawBoardText(x, y, text, size or 20, width or 0, rgb[1], rgb[2], rgb[3])
end

local function Button(x, y, w, label, action, enabled)
    if DrawBoardButton(x, y, w, 36, label, enabled) then PerformAction(action) end
end

function DrawBackground()
    local function Panel(x, y, w, h, color)
        DrawBoardPanel(x, y, w, h, color[1], color[2], color[3])
    end
    Panel(40, 48, 1200, 630, Theme.Colors.WindowBg)
    Panel(68, 199, 195, 455, Theme.Colors.TitleBgActive)
    Panel(293, 199, 610, 455, Theme.Colors.PopupBg)
    Panel(925, 199, 290, 455, Theme.Colors.TitleBgActive)
    Panel(655, 67, 560, 98, Theme.Colors.TitleBgActive)
end

function Draw()
    if not PilesRef then return end
    local piles, alive = PilesRef, Alive()
    local power = Attack(piles)
    local stock = piles:get(STOCK).cards:size()
    local count = RoomCount(piles)
    local quiet = Quiet(piles)

    Text(70, 67, "CRAWLER", "gold", 36)
    Text(72, 112, "Every card is a choice. Survive the whole deck.", "muted", 19)
    Button(72, 151, 90, "Undo", "engine:undo", CanUndo())
    Button(174, 151, 90, "Redo", "engine:redo", CanRedo())
    Button(276, 151, 132, "New run", "engine:restart", true)
    Button(420, 151, 120, "Advice", "hint", alive)
    Text(675, 82, "HEALTH   " .. Dungeon.hp .. " / " .. MAX_HP, Dungeon.hp <= 5 and "monster" or "potion", 20)
    DrawBoardPanel(675, 120, 216, 12, 17, 23, 32)
    DrawBoardPanel(675, 120, 216 * Dungeon.hp / MAX_HP, 12, Dungeon.hp <= 5 and 245 or 114,
        Dungeon.hp <= 5 and 119 or 210, Dungeon.hp <= 5 and 131 or 166)
    Text(930, 82, "GOLD", "muted", 16)
    Text(930, 109, tostring(Dungeon.gold), "gold", 27)
    Text(1080, 82, "ROOM", "muted", 16)
    Text(1080, 109, string.format("%02d", Dungeon.room), "ink", 27)

    Text(89, 220, "THE DUNGEON", "muted", 18)
    Text(90, 432, stock .. (stock == 1 and " card remains" or " cards remain"), "muted", 17)
    Text(89, 456, "YOUR WEAPON", "weapon", 18)
    Text(87, 635, power > 0 and (power .. " ATK / " .. Dungeon.dur .. " strikes") or "Bare hands", power > 0 and "weapon" or "muted", 17)
    DrawBoardTooltip(110, 487, 100, 140, power > 0 and
        (power .. " attack. " .. Dungeon.dur .. " strikes left. Bare-handed fights preserve the weapon.") or
        "No weapon equipped. Monsters deal their full value as damage.")

    Text(314, 218, Dungeon.finished == 1 and "DUNGEON CLEARED" or Dungeon.hp == 0 and "THE RUN HAS ENDED" or "CHOOSE YOUR ENCOUNTER", "gold", 21)
    Text(314, 249, "Click a card, or choose an action below it.", "muted", 17)
    for i = 0, 3 do
        local x, cards = CARD_X + i * CARD_STEP, piles:get(i).cards
        if cards:empty() then
            Text(x + 14, 445, "Cleared", "muted", 17)
        else
            local card = cards:back()
            local value, suit = Value(card), card.suit
            local title, detail, action, color, tip
            local mainEnabled = alive
            if suit == Suit.Spades then
                local damage = math.max(0, value - power)
                title, detail, action, color = "MONSTER", damage .. " HP damage", "Fight", "monster"
                if damage >= Dungeon.hp then detail = "LETHAL: " .. damage .. " HP" end
                tip = "Monster strength: " .. value .. ". Your weapon blocks " .. math.min(value, power) ..
                    ". You take " .. damage .. " damage" .. (damage >= Dungeon.hp and " (lethal)." or ".")
                if power > 0 then tip = tip .. " Uses one strike" .. (Dungeon.dur == 1 and " and breaks the weapon." or ".") end
                if power > 0 then Button(x - 5, 543, 125,
                    value >= Dungeon.hp and "Bare: lethal" or ("Bare: " .. value .. " HP"), "bare:" .. i, alive) end
            elseif suit == Suit.Hearts then
                local healed = math.min(MAX_HP - Dungeon.hp, value)
                title, color = "POTION", "potion"
                detail = Dungeon.potions == 1 and "Used this room" or healed == 0 and "Health full" or ("Heal " .. healed .. " HP")
                action, mainEnabled = "Drink", alive and Dungeon.potions == 0 and healed > 0
                tip = "Potion value: " .. value .. ". Heals " .. healed .. " HP. Only one potion per room; carry it forward or leave it behind."
                Button(x - 5, 543, 125, "Leave behind", "leave:" .. i, alive)
            elseif suit == Suit.Clubs then
                title, detail, action, color = "WEAPON", value .. " ATK / 3 hits", "Equip", "weapon"
                tip = "Replace your current weapon with " .. value .. " attack and three strikes."
                if power > value then tip = tip .. " This lowers your attack." end
                Button(x - 5, 543, 125, "Keep current", "leave:" .. i, alive)
            else
                title, detail, action, color = "TREASURE", "+" .. value .. " gold", "Loot", "gold"
                tip = "Collect " .. value .. " gold. Gold contributes to your score and can pay for camp healing."
            end
            Text(x - 2, 439, title, color, 17)
            Text(x - 2, 465, detail, color, 16, 128)
            Button(x - 5, 499, 125, action, "main:" .. i, mainEnabled)
            DrawBoardTooltip(x, CARD_Y, 100, 140, tip)
        end
    end
    Button(319, 602, 244, count == 1 and "Carry on to next room" or "Enter next room", "advance", alive and quiet and stock > 0)
    Button(583, 602, 294, "Retreat (-2 HP)", "retreat", alive and Dungeon.hp > 2 and Dungeon.canRetreat == 1 and stock > 0 and count > 0)

    Text(945, 221, "EXPEDITION", "muted", 18)
    local status = Dungeon.finished == 1 and "YOU SURVIVED" or Dungeon.hp == 0 and "YOU HAVE FALLEN" or "Room " .. Dungeon.room .. " / " .. count .. " encounters"
    Text(945, 260, status, Dungeon.hp == 0 and "monster" or "gold", 21, 250)
    Text(945, 301, Notice ~= "" and Notice or Dungeon.log[1] or "", "ink", 18, 248)
    Text(945, 392, Dungeon.kills .. " monsters defeated", "muted", 17)
    Text(945, 421, "Score " .. GetScore() .. "  /  " .. Dungeon.retreats .. " retreats", "muted", 17)
    Text(945, 458, Dungeon.potions == 1 and "Potion: used this room" or "Potion: unused this room", "potion", 17)
    Button(945, 493, 125, "Camp: 8 gold", "camp", alive and quiet and Dungeon.rested == 0 and Dungeon.gold >= CAMP_COST and Dungeon.hp < MAX_HP)
    Text(945, 543, "Heal +" .. math.min(CAMP_HEAL, MAX_HP - Dungeon.hp) .. " HP\nOnce per room", "muted", 14, 125)
    Text(1090, 643, "Discard", "muted", 16)
    Text(945, 598, Dungeon.finished == 1 and "F2 for a new run." or "Undo: Ctrl+Z\nRetry: F2", "muted", 16, 125)
end
