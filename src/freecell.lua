GameName = "FreeCell"
HelpText = "Build the four foundations up in suit from Ace to King.\nCards on the tableau are built down by alternating color.\nUse the four free cells to temporarily store single cards."
NumDecks = 1

function Init(piles, deck)
    -- 0-3: Free cells (Top Left)
    for i = 0, 3 do
        local p = Pile.new()
        p.pos = ImVec2.new(20 + i * 110, 20)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 0)
        p.type = PileType.FreeCellSlot
        piles:push_back(p)
    end

    -- 4-7: Foundations (Top Right)
    for i = 0, 3 do
        local p = Pile.new()
        p.pos = ImVec2.new(500 + i * 110, 20)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 0)
        p.type = PileType.Foundation
        piles:push_back(p)
    end

    -- 8-15: Tableaus (Bottom)
    for i = 0, 7 do
        local p = Pile.new()
        p.pos = ImVec2.new(20 + i * 110, 180)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 30)
        p.type = PileType.Tableau
        piles:push_back(p)
    end

    -- Deal all 52 cards into the 8 tableaus
    local tab = 8
    while not deck:empty() do
        local c = deck:back()
        c.faceUp = true
        piles:get(tab).cards:push_back(c)
        deck:pop_back()
        tab = tab + 1
        if tab > 15 then tab = 8 end
    end
end

function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    if p.cards:empty() then return false end
    
    if p.type == PileType.Foundation then return false end
    if p.type == PileType.FreeCellSlot then return true end
    
    local c = p.cards:get(cardIdx)
    if not c.faceUp then return false end
    
    -- For multi-card drag, it must be a valid alternating sequence
    for i = cardIdx, p.cards:size() - 2 do
        local c1 = p.cards:get(i)
        local c2 = p.cards:get(i + 1)
        if c1:IsRed() == c2:IsRed() or c1.rank - 1 ~= c2.rank then
            return false
        end
    end
    
    -- Check if we have enough empty cells to move this many cards
    local numCards = p.cards:size() - cardIdx
    if numCards == 1 then return true end
    
    local emptyFreeCells = 0
    for i = 0, 3 do
        if piles:get(i).cards:empty() then emptyFreeCells = emptyFreeCells + 1 end
    end
    
    local emptyTableaus = 0
    for i = 8, 15 do
        if i ~= pileIdx and piles:get(i).cards:empty() then emptyTableaus = emptyTableaus + 1 end
    end
    
    -- Max cards moving formula for FreeCell: (free cells + 1) * 2^(empty tableaus)
    local maxMove = (emptyFreeCells + 1) * (2 ^ emptyTableaus)
    return numCards <= maxMove
end

function CanDrop(piles, srcIdx, dstIdx, dragCards)
    local dst = piles:get(dstIdx)
    local c = dragCards:get(0)
    
    if dst.type == PileType.FreeCellSlot then
        return dst.cards:empty() and dragCards:size() == 1
    elseif dst.type == PileType.Foundation then
        if dragCards:size() > 1 then return false end
        if dst.cards:empty() then
            return c.rank == Rank.Ace
        else
            local top = dst.cards:back()
            return top.suit == c.suit and top.rank + 1 == c.rank
        end
    elseif dst.type == PileType.Tableau then
        if dst.cards:empty() then return true end
        local top = dst.cards:back()
        return top:IsRed() ~= c:IsRed() and top.rank - 1 == c.rank
    end
    
    return false
end

function AfterMove(piles, srcIdx, dstIdx, cardIdx)
end

function HandleClick(piles, pileIdx)
end

function IsWon(piles)
    for i = 4, 7 do
        if piles:get(i).cards:size() ~= 13 then return false end
    end
    return true
end

function AutoSolve(piles)
    for i = 0, 15 do
        if i < 4 or i > 7 then -- Check FreeCells and Tableaus
            local p = piles:get(i)
            if not p.cards:empty() then
                local c = p.cards:back()
                for f = 4, 7 do
                    local fnd = piles:get(f)
                    if fnd.cards:empty() then
                        if c.rank == Rank.Ace then
                            return {i, f, p.cards:size() - 1}
                        end
                    else
                        local top = fnd.cards:back()
                        if top.suit == c.suit and top.rank + 1 == c.rank then
                            return {i, f, p.cards:size() - 1}
                        end
                    end
                end
            end
        end
    end
    return {}
end