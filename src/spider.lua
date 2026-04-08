GameName = "Spider"
NumDecks = 2
HelpText = "Spider Solitaire\n\nGoal: Assemble 13 cards of a suit, in descending sequence from King to Ace.\nOnce a full suit is assembled, it is removed from play.\nCards can be moved to other tableau columns if they are exactly one lower in rank."

function Init(piles, deck)
    -- For simplicity, make it a 1-suit spider game for now by forcing all to Spades
    for i = 0, deck:size() - 1 do
        deck:get(i).suit = 3 -- Spades
    end

    local startX, startY = 20.0, 40.0
    local padX = 100.0 + 10.0

    for i = 0, 9 do
        local tab = Pile.new()
        tab.id = i; tab.type = PileType.Tableau
        tab.pos = ImVec2.new(startX + padX * i, startY + 140.0 + 30.0)
        tab.size = ImVec2.new(100.0, 140.0)
        tab.offset = ImVec2.new(0.0, 20.0)

        local count = 5
        if i < 4 then count = 6 end
        for j = 1, count do
            local c = deck:back()
            deck:pop_back()
            c.faceUp = (j == count)
            tab.cards:push_back(c)
        end
        piles:push_back(tab)
    end

    local stock = Pile.new()
    stock.id = 10; stock.type = PileType.Stock
    stock.pos = ImVec2.new(startX, startY)
    stock.size = ImVec2.new(100.0, 140.0)
    stock.offset = ImVec2.new(10.0, 0.0)
    stock.cards = deck
    piles:push_back(stock)
end

function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    local c = p.cards:get(cardIdx)
    if not c.faceUp then return false end

    for i = cardIdx, p.cards:size() - 2 do
        local c1 = p.cards:get(i)
        local c2 = p.cards:get(i + 1)
        if c1.suit ~= c2.suit or c1.rank - 1 ~= c2.rank then
            return false
        end
    end
    return true
end

function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    local tp = piles:get(targetPileIdx)
    local dragBottom = dragCards:get(0)

    if tp.type == PileType.Tableau then
        if tp.cards:empty() then
            return true
        else
            local top = tp.cards:back()
            return top.rank - 1 == dragBottom.rank
        end
    end
    return false
end

function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx)
    local sp = piles:get(sourcePileIdx)
    local tp = piles:get(targetPileIdx)

    if not sp.cards:empty() and sp.type == PileType.Tableau and not sp.cards:back().faceUp then
        sp.cards:back().faceUp = true
    end

    if tp.type == PileType.Tableau and tp.cards:size() >= 13 then
        local fullSet = true
        local s = tp.cards:back().suit
        local checkStart = tp.cards:size() - 13

        for i = 0, 12 do
            local c = tp.cards:get(checkStart + i)
            if not c.faceUp or c.suit ~= s or c.rank ~= 13 - i then
                fullSet = false
                break
            end
        end

        if fullSet then
            for i = 1, 13 do
                tp.cards:pop_back()
            end
            if not tp.cards:empty() and not tp.cards:back().faceUp then
                tp.cards:back().faceUp = true
            end
        end
    end
end

function HandleClick(piles, pileIdx)
    local p = piles:get(pileIdx)
    if p.type == PileType.Stock and not p.cards:empty() then
        for i = 0, piles:size() - 1 do
            if piles:get(i).type == PileType.Tableau then
                if p.cards:empty() then break end
                local c = p.cards:back()
                p.cards:pop_back()
                c.faceUp = true
                piles:get(i).cards:push_back(c)
            end
        end
    end
end

function IsWon(piles)
    local hasCards = false
    for i = 0, piles:size() - 1 do
        if not piles:get(i).cards:empty() then
            hasCards = true
            break
        end
    end
    return not hasCards
end

function AutoSolve(piles)
    return {}
end