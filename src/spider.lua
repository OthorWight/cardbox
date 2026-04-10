GameName = "Spider (1 Suit)"
HelpText = "Build 8 sequences of Spades from King to Ace.\nCompleted sequences automatically move to the foundations."
NumDecks = 2

function Init(piles, deck)
    -- 0 to 7: Foundations
    for i = 0, 7 do
        local p = Pile.new()
        p.pos = ImVec2.new(350 + i * 110, 20)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 0)
        p.type = PileType.Foundation
        piles:push_back(p)
    end

    -- 8: Stock
    local stock = Pile.new()
    stock.pos = ImVec2.new(20, 20)
    stock.size = ImVec2.new(100, 140)
    stock.offset = ImVec2.new(2, 0)
    stock.type = PileType.Stock
    piles:push_back(stock)

    -- 9 to 18: Tableaus
    for i = 0, 9 do
        local p = Pile.new()
        p.pos = ImVec2.new(20 + i * 110, 180)
        p.size = ImVec2.new(100, 140)
        p.offset = ImVec2.new(0, 25)
        p.type = PileType.Tableau
        piles:push_back(p)
    end

    -- Deal tableaus (54 cards)
    for i = 0, 53 do
        local c = deck:back()
        c.suit = Suit.Spades -- Force 1 suit
        c.faceUp = false
        local tabIdx = 9 + (i % 10)
        piles:get(tabIdx).cards:push_back(c)
        deck:pop_back()
    end

    -- Face up top cards
    for i = 9, 18 do
        local p = piles:get(i)
        p.cards:back().faceUp = true
    end

    -- Put remaining 50 cards into Stock
    while not deck:empty() do
        local c = deck:back()
        c.suit = Suit.Spades
        c.faceUp = false
        piles:get(8).cards:push_back(c)
        deck:pop_back()
    end
end

function CanPickup(piles, pileIdx, cardIdx)
    if pileIdx < 9 then return false end
    local p = piles:get(pileIdx)
    if p.cards:empty() then return false end
    
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

function CanDrop(piles, srcIdx, dstIdx, dragCards)
    if dstIdx < 9 then return false end
    local dst = piles:get(dstIdx)
    if dst.cards:empty() then return true end
    
    local top = dst.cards:back()
    local c = dragCards:get(0)
    return top.rank - 1 == c.rank
end

function AfterMove(piles, srcIdx, dstIdx, cardIdx)
    local src = piles:get(srcIdx)
    if not src.cards:empty() and not src.cards:back().faceUp then
        src.cards:back().faceUp = true
    end
    
    -- Check if we completed a King to Ace sequence!
    local dst = piles:get(dstIdx)
    if dst.cards:size() >= 13 then
        local startIdx = dst.cards:size() - 13
        local valid = true
        local curRank = Rank.King
        for i = 0, 12 do
            if dst.cards:get(startIdx + i).rank ~= curRank or not dst.cards:get(startIdx + i).faceUp then
                valid = false
                break
            end
            curRank = curRank - 1
        end
        
        if valid then
            for f = 0, 7 do
                local fnd = piles:get(f)
                if fnd.cards:empty() then
                    for i = 0, 12 do
                        fnd.cards:push_back(dst.cards:get(startIdx + i))
                    end
                    for i = 0, 12 do
                        dst.cards:pop_back()
                    end
                    if not dst.cards:empty() and not dst.cards:back().faceUp then
                        dst.cards:back().faceUp = true
                    end
                    break
                end
            end
        end
    end
end

function HandleClick(piles, pileIdx)
    if pileIdx == 8 then
        local stock = piles:get(8)
        if stock.cards:empty() then return end
        
        -- Deal 1 card to each tableau
        local limit = math.min(10, stock.cards:size())
        for i = 0, limit - 1 do
            local c = stock.cards:back()
            c.faceUp = true
            stock.cards:pop_back()
            piles:get(9 + i).cards:push_back(c)
        end
        
        -- Verify if this deal accidentally completed a sequence
        for i = 9, 18 do
            AfterMove(piles, 8, i, 0)
        end
    end
end

function IsWon(piles)
    for i = 0, 7 do
        if piles:get(i).cards:size() ~= 13 then return false end
    end
    return true
end

function AutoSolve(piles)
    return {}
end