GameName = "Klondike"
NumDecks = 1
HelpText = "Klondike Solitaire\n\nGoal: Move all 52 cards to the four foundation piles at the top right.\nFoundations are built up by suit, from Ace to King.\nTableau piles can be built down by alternating colors.\nClick the stock pile (top left) to deal more cards."

function Init(piles, deck)
    local startX, startY = 20.0, 40.0
    local padX = 100.0 + 20.0

    local stock = Pile.new()
    stock.id = 0; stock.type = PileType.Stock
    stock.pos = ImVec2.new(startX, startY)
    stock.size = ImVec2.new(100.0, 140.0)
    stock.offset = ImVec2.new(0.5, 0.5)
    piles:push_back(stock)

    local waste = Pile.new()
    waste.id = 1; waste.type = PileType.Waste
    waste.pos = ImVec2.new(startX + padX + 20, startY)
    waste.size = ImVec2.new(100.0, 140.0)
    waste.offset = ImVec2.new(20.0, 0.0)
    piles:push_back(waste)

    for i = 0, 3 do
        local found = Pile.new()
        found.id = 2 + i; found.type = PileType.Foundation
        found.pos = ImVec2.new(startX + padX * (3 + i), startY)
        found.size = ImVec2.new(100.0, 140.0)
        found.offset = ImVec2.new(0.0, 0.0)
        piles:push_back(found)
    end

    for i = 0, 6 do
        local tab = Pile.new()
        tab.id = 6 + i; tab.type = PileType.Tableau
        tab.pos = ImVec2.new(startX + padX * i, startY + 140.0 + 60.0)
        tab.size = ImVec2.new(100.0, 140.0)
        tab.offset = ImVec2.new(0.0, 25.0)

        for j = 0, i do
            local c = deck:back()
            deck:pop_back()
            c.faceUp = (j == i)
            tab.cards:push_back(c)
        end
        piles:push_back(tab)
    end

    -- Finally, put the remaining cards into the stock pile
    piles:get(0).cards = deck
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
            return dragBottom.rank == 1 -- Ace
        else
            local top = tp.cards:back()
            return top.suit == dragBottom.suit and top.rank + 1 == dragBottom.rank
        end
    elseif tp.type == PileType.Tableau then
        if tp.cards:empty() then
            return dragBottom.rank == 13 -- King
        else
            local top = tp.cards:back()
            return top:Color() ~= dragBottom:Color() and top.rank - 1 == dragBottom.rank
        end
    end
    return false
end

function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx)
    local sp = piles:get(sourcePileIdx)
    if not sp.cards:empty() and sp.type == PileType.Tableau and not sp.cards:back().faceUp then
        sp.cards:back().faceUp = true
    end
end

function HandleClick(piles, pileIdx)
    local p = piles:get(pileIdx)
    local wasteIdx = 1
    local waste = piles:get(wasteIdx)
    
    if not p.cards:empty() then
        local c = p.cards:back()
        p.cards:pop_back()
        c.faceUp = true
        waste.cards:push_back(c)
    else
        while not waste.cards:empty() do
            local c = waste.cards:back()
            waste.cards:pop_back()
            c.faceUp = false
            p.cards:push_back(c)
        end
    end
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
    local allFaceUp = true
    local hasCardsInPlay = false
    for i = 0, piles:size() - 1 do
        local p = piles:get(i)
        if (p.type == PileType.Stock or p.type == PileType.Waste) and not p.cards:empty() then
            allFaceUp = false
            break
        end
    end
    if allFaceUp then
        for i = 0, piles:size() - 1 do
            local p = piles:get(i)
            if p.type == PileType.Tableau then
                for j = 0, p.cards:size() - 1 do
                    if not p.cards:get(j).faceUp then allFaceUp = false break end
                end
                if not p.cards:empty() then hasCardsInPlay = true end
            end
            if not allFaceUp then break end
        end
    end

    if allFaceUp and hasCardsInPlay then
        for i = 0, piles:size() - 1 do
            local p = piles:get(i)
            if p.type == PileType.Tableau and not p.cards:empty() then
                local cardIdx = p.cards:size() - 1
                local stack = VectorCard.new()
                stack:push_back(p.cards:back())
                for f = 0, piles:size() - 1 do
                    local tp = piles:get(f)
                    if tp.type == PileType.Foundation and CanDrop(piles, i, f, stack) then
                        return {i, f, cardIdx} -- Return 0-based indices as expected by C++
                    end
                end
            end
        end
    end
    return {}
end