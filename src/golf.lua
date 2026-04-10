GameName = "Golf"
NumDecks = 1
HelpText = "Clear the 7 tableaus by moving cards to the waste pile.\nCards can be moved if they are exactly one rank higher or lower than the top waste card.\nWrapping between King and Ace is allowed.\nClick the stock to draw a new card."

function Init(piles, deck)
    local startX, startY = 20.0, 40.0
    local padX = 110.0

    -- 0-6: Tableaus (7 columns of 5 cards each)
    for i = 0, 6 do
        local p = Pile.new()
        p.id = i
        p.pos = ImVec2.new(startX + i * padX, startY + 160.0)
        p.size = ImVec2.new(100.0, 140.0)
        p.offset = ImVec2.new(0.0, 30.0)
        p.type = PileType.Tableau
        for j = 1, 5 do
            local c = deck:back()
            c.faceUp = true
            p.cards:push_back(c)
            deck:pop_back()
        end
        piles:push_back(p)
    end

    -- 7: Stock
    local stock = Pile.new()
    stock.id = 7
    stock.pos = ImVec2.new(startX, startY)
    stock.size = ImVec2.new(100.0, 140.0)
    stock.offset = ImVec2.new(2.0, 0.0)
    stock.type = PileType.Stock
    piles:push_back(stock)

    -- 8: Waste
    local waste = Pile.new()
    waste.id = 8
    waste.pos = ImVec2.new(startX + padX, startY)
    waste.size = ImVec2.new(100.0, 140.0)
    waste.offset = ImVec2.new(20.0, 0.0)
    waste.type = PileType.Waste
    piles:push_back(waste)

    -- Put remaining 17 cards in stock
    while not deck:empty() do
        local c = deck:back()
        c.faceUp = false
        piles:get(7).cards:push_back(c)
        deck:pop_back()
    end

    -- Deal first card to waste
    if not piles:get(7).cards:empty() then
        local c = piles:get(7).cards:back()
        c.faceUp = true
        piles:get(8).cards:push_back(c)
        piles:get(7).cards:pop_back()
    end
end

function CanPickup(piles, pileIdx, cardIdx)
    -- Can only pick up the bottom-most exposed card of a tableau
    if pileIdx > 6 then return false end
    local p = piles:get(pileIdx)
    return cardIdx == p.cards:size() - 1
end

function CanDrop(piles, srcIdx, dstIdx, dragCards)
    -- Can only drop 1 card onto waste
    if dstIdx ~= 8 or dragCards:size() > 1 then return false end
    local waste = piles:get(8)
    local dragCard = dragCards:get(0)
    
    if waste.cards:empty() then return true end
    
    local top = waste.cards:back()
    local diff = math.abs(top.rank - dragCard.rank)
    
    -- Allow exactly 1 rank up/down, or Ace-to-King wrapping (13 <-> 1)
    return diff == 1 or diff == 12
end

function AfterMove(piles, srcIdx, dstIdx, cardIdx)
end

function HandleClick(piles, pileIdx)
    -- Draw from stock to waste
    if pileIdx == 7 then 
        local stock = piles:get(7)
        local waste = piles:get(8)
        if not stock.cards:empty() then
            local c = stock.cards:back()
            stock.cards:pop_back()
            c.faceUp = true
            waste.cards:push_back(c)
        end
    
    -- Fast-play: clicking a tableau card attempts to auto-move it to waste
    elseif pileIdx <= 6 then 
        local tab = piles:get(pileIdx)
        local waste = piles:get(8)
        if not tab.cards:empty() and not waste.cards:empty() then
            local tabCard = tab.cards:back()
            local wasteCard = waste.cards:back()
            local diff = math.abs(tabCard.rank - wasteCard.rank)
            if diff == 1 or diff == 12 then
                local c = tab.cards:back()
                tab.cards:pop_back()
                waste.cards:push_back(c)
            end
        elseif not tab.cards:empty() and waste.cards:empty() then
            local c = tab.cards:back()
            tab.cards:pop_back()
            waste.cards:push_back(c)
        end
    end
end

function IsWon(piles)
    -- Won when all tableaus are empty
    for i = 0, 6 do
        if not piles:get(i).cards:empty() then return false end
    end
    return true
end

function AutoSolve(piles)
    return {}
end