GameName = "FreeCell"
NumDecks = 1
HelpText = "FreeCell Solitaire\n\nGoal: Move all cards to the four foundation piles at the top right.\nFoundations are built up by suit, from Ace to King.\nTableau piles can be built down by alternating colors.\nUse the four free cells (top left) to temporarily store single cards."

function Init(piles, deck)
    local startX, startY = 20.0, 40.0
    local padX = 100.0 + 20.0

    for i = 0, 3 do
        local freeCell = Pile.new()
        freeCell.id = i; freeCell.type = PileType.FreeCellSlot
        freeCell.pos = ImVec2.new(startX + padX * i, startY)
        freeCell.size = ImVec2.new(100.0, 140.0)
        freeCell.offset = ImVec2.new(0.0, 0.0)
        piles:push_back(freeCell)
    end

    for i = 0, 3 do
        local found = Pile.new()
        found.id = 4 + i; found.type = PileType.Foundation
        found.pos = ImVec2.new(startX + padX * (4 + i), startY)
        found.size = ImVec2.new(100.0, 140.0)
        found.offset = ImVec2.new(0.0, 0.0)
        piles:push_back(found)
    end

    for i = 0, 7 do
        local tab = Pile.new()
        tab.id = 8 + i; tab.type = PileType.Tableau
        tab.pos = ImVec2.new(startX + padX * i, startY + 140.0 + 30.0)
        tab.size = ImVec2.new(100.0, 140.0)
        tab.offset = ImVec2.new(0.0, 25.0)

        local count = 6
        if i < 4 then count = 7 end
        for j = 1, count do
            local c = deck:back()
            deck:pop_back()
            c.faceUp = true
            tab.cards:push_back(c)
        end
        piles:push_back(tab)
    end
end

function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    local c = p.cards:get(cardIdx)
    if not c.faceUp then return false end

    for i = cardIdx, p.cards:size() - 2 do
        local c1 = p.cards:get(i)
        local c2 = p.cards:get(i + 1)
        if c1:Color() == c2:Color() or c1.rank - 1 ~= c2.rank then
            return false
        end
    end
    return true
end

function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    local tp = piles:get(targetPileIdx)
    local dragBottom = dragCards:get(0)

    if tp.type == PileType.Foundation then
        if dragCards:size() > 1 then return false end
        if tp.cards:empty() then
            return dragBottom.rank == 1
        else
            local top = tp.cards:back()
            return top.suit == dragBottom.suit and top.rank + 1 == dragBottom.rank
        end
    elseif tp.type == PileType.Tableau then
        if tp.cards:empty() then
            return true
        else
            local top = tp.cards:back()
            return top:Color() ~= dragBottom:Color() and top.rank - 1 == dragBottom.rank
        end
    elseif tp.type == PileType.FreeCellSlot then
        return dragCards:size() == 1 and tp.cards:empty()
    end
    return false
end

function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx)
end

function HandleClick(piles, pileIdx)
end

function IsWon(piles)
    local foundCount = 0
    for i = 0, piles:size() - 1 do
        if piles:get(i).type == PileType.Foundation then
            foundCount = foundCount + piles:get(i).cards:size()
        end
    end
    return foundCount == 52
end

function AutoSolve(piles)
    return {}
end