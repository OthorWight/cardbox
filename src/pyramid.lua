GameName = "Pyramid"
NumDecks = 1
HelpText = "Clear the pyramid by matching pairs of cards that sum to 13.\nKings are worth 13 and can be removed individually.\nQueens=12, Jacks=11, Aces=1.\nDrag a card onto another to pair them.\nRight-click a King to remove it.\nClick the Stock to deal cards."

function Init(piles, deck)
    local startX, startY = 1280 / 2 - 50, 40.0
    local padX = 110.0
    local padY = 60.0

    -- 0: Stock
    local stock = Pile.new()
    stock.id = 0; stock.type = PileType.Stock
    stock.pos = ImVec2.new(220, startY)
    stock.size = ImVec2.new(100.0, 140.0)
    stock.offset = ImVec2.new(0.2, -0.5)
    piles:push_back(stock)

    -- 1: Waste
    local waste = Pile.new()
    waste.id = 1; waste.type = PileType.Waste
    waste.pos = ImVec2.new(220 + 120, startY)
    waste.size = ImVec2.new(100.0, 140.0)
    waste.offset = ImVec2.new(20.0, 0.0)
    piles:push_back(waste)

    -- 2 to 29: Pyramid (28 cards)
    local idx = 2
    for row = 0, 6 do
        local rowStartX = startX - (row * padX * 0.5)
        local rowY = startY + (row * padY)
        for col = 0, row do
            local p = Pile.new()
            p.id = idx; p.type = PileType.Invisible
            p.pos = ImVec2.new(rowStartX + col * padX, rowY)
            p.size = ImVec2.new(100.0, 140.0)
            p.offset = ImVec2.new(0.0, 0.0)

            local c = deck:back()
            deck:pop_back()
            c.faceUp = true
            p.cards:push_back(c)

            piles:push_back(p)
            idx = idx + 1
        end
    end

    -- 30: Foundation (discard pile)
    local found = Pile.new()
    found.id = 30; found.type = PileType.Foundation
    found.pos = ImVec2.new(1000 - 120, startY)
    found.size = ImVec2.new(100.0, 140.0)
    found.offset = ImVec2.new(0.0, 0.0)
    piles:push_back(found)

    -- Put remaining deck in stock
    piles:get(0).cards = deck
end

-- Helper to check if a card in the pyramid is covered by the row below it
function IsBlocked(piles, pileIdx)
    if pileIdx < 2 or pileIdx > 29 then return false end
    local i = pileIdx - 2
    local row, sum = 0, 0
    while sum + row < i do
        row = row + 1
        sum = sum + row
    end
    if row == 6 then return false end -- Bottom row is always exposed
    
    local leftChild = i + row + 1
    local rightChild = i + row + 2
    if not piles:get(leftChild + 2).cards:empty() then return true end
    if not piles:get(rightChild + 2).cards:empty() then return true end
    return false
end

function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    if not p.cards:get(cardIdx).faceUp then return false end
    if cardIdx ~= p.cards:size() - 1 then return false end
    if IsBlocked(piles, pileIdx) then return false end
    return true
end

function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    if dragCards:size() ~= 1 then return false end
    local dragCard = dragCards:get(0)

    -- Kings can be dragged to the Foundation
    if targetPileIdx == 30 and dragCard.rank == 13 then
        return true
    end

    -- Pair matching
    local tp = piles:get(targetPileIdx)
    if targetPileIdx >= 1 and targetPileIdx <= 29 and not tp.cards:empty() then
        if IsBlocked(piles, targetPileIdx) then return false end
        if dragCard.rank + tp.cards:back().rank == 13 then
            return true
        end
    end

    return false
end

function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx)
    if targetPileIdx == 30 then return end -- Was just a King dropping on foundation

    -- It was a pair match! The target pile now has BOTH cards. Pop them and send to Foundation.
    local tp = piles:get(targetPileIdx)
    local fp = piles:get(30)

    local c1 = tp.cards:back()
    tp.cards:pop_back()
    local c2 = tp.cards:back()
    tp.cards:pop_back()

    fp.cards:push_back(c2)
    fp.cards:push_back(c1)
end

function HandleClick(piles, pileIdx)
    local p = piles:get(pileIdx)
    local waste = piles:get(1)
    
    if pileIdx == 0 then -- Stock clicked
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
end

function AutoSolve(piles)
    return {}
end