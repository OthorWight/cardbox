GameName = "Yukon"
HelpText = "Build the four foundations up in suit from Ace to King.\nMove any face-up group of cards, regardless of sequence, as long as the top card matches the target in alternating color and descending rank. Only Kings can go on empty tableaus."
NumDecks = 1
AutoCenter = true

function Init(piles, deck)
    -- 0-3: Foundations
    for i = 0, 3 do
        local p = Pile.new()
        p.pos = ImVec2.new(350 + i * 110, 20)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 0)
        p.type = PileType.Foundation
        piles:push_back(p)
    end

    -- 4-10: Tableaus
    for i = 0, 6 do
        local p = Pile.new()
        p.pos = ImVec2.new(20 + i * 110, 180)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 30)
        p.type = PileType.Tableau
        piles:push_back(p)
    end

    -- Deal cards into Yukon's specific format
    for i = 0, 6 do
        local p = piles:get(4 + i)
        -- Down cards
        for j = 1, i do
            local c = deck:back()
            c.faceUp = false
            p.cards:push_back(c)
            deck:pop_back()
        end
        -- Up cards (1 on the first stack, 5 on all others)
        local upCards = 5
        if i == 0 then upCards = 1 end
        for j = 1, upCards do
            local c = deck:back()
            c.faceUp = true
            p.cards:push_back(c)
            deck:pop_back()
        end
    end
end

function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    if p.cards:empty() then return false end
    
    if p.type == PileType.Foundation then
        return cardIdx == p.cards:size() - 1
    end

    -- In Yukon, you can grab literally any face up card and its children!
    local c = p.cards:get(cardIdx)
    return c.faceUp
end

function CanDrop(piles, srcIdx, dstIdx, dragCards)
    local dst = piles:get(dstIdx)
    local c = dragCards:get(0)
    
    if dst.type == PileType.Foundation then
        if dragCards:size() > 1 then return false end
        if dst.cards:empty() then
            return c.rank == Rank.Ace
        else
            local top = dst.cards:back()
            return top.suit == c.suit and top.rank + 1 == c.rank
        end
    elseif dst.type == PileType.Tableau then
        if dst.cards:empty() then 
            return c.rank == Rank.King
        end
        local top = dst.cards:back()
        return top:IsRed() ~= c:IsRed() and top.rank - 1 == c.rank
    end
    
    return false
end

function AfterMove(piles, srcIdx, dstIdx, cardIdx)
    local src = piles:get(srcIdx)
    if not src.cards:empty() and not src.cards:back().faceUp then
        src.cards:back().faceUp = true
    end
end

function HandleClick(piles, pileIdx)
end

function IsWon(piles)
    for i = 0, 3 do
        if piles:get(i).cards:size() ~= 13 then return false end
    end
    return true
end

function AutoSolve(piles)
    local readyToAutoSolve = true
    for i = 4, 10 do
        local p = piles:get(i)
        if not p.cards:empty() then
            for j = 0, p.cards:size() - 1 do
                if not p.cards:get(j).faceUp then
                    readyToAutoSolve = false
                    break
                end
                if j < p.cards:size() - 1 then
                    local c1 = p.cards:get(j)
                    local c2 = p.cards:get(j + 1)
                    if c1:IsRed() == c2:IsRed() or c1.rank - 1 ~= c2.rank then
                        readyToAutoSolve = false
                        break
                    end
                end
            end
        end
        if not readyToAutoSolve then break end
    end

    if not readyToAutoSolve then
        return {}
    end

    for i = 4, 10 do
        local p = piles:get(i)
        if not p.cards:empty() then
            local c = p.cards:back()
            for f = 0, 3 do
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
    return {}
end