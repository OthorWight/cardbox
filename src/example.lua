-- Global variables expected by the C++ engine
GameName = "Example"
NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(120.0, 120.0) -- Let's make them square!
CornerRadius = 20.0                 -- Extra round corners
HelpText = "This is a dummy game demonstrating all available Lua API bindings.\nMove all cards out of the first pile to trigger the win fireworks!"

ButtonClickCount = 0

-- Init is called once when the game starts or restarts.
-- @param piles: A VectorPile object (std::vector<Pile> in C++)
-- @param deck: A VectorCard object (std::vector<Card> in C++) containing the shuffled deck
function Init(piles, deck)
    -- ==========================================
    -- ImVec2 API
    -- ==========================================
    local pos1 = ImVec2.new()           -- Default constructor (0,0)
    local pos2 = ImVec2.new(20.0, 60.0) -- Constructor with x, y
    local pos3 = ImVec2.new(180.0, 60.0)
    local pos4 = ImVec2.new(380.0, 60.0)
    pos1.x = 20.0                       -- Modify x property
    pos1.y = 240.0                      -- Modify y property

    -- ==========================================
    -- VectorPile API
    -- ==========================================
    piles:clear()                       -- Clear all piles (already empty at start, but good for demo)
    local isPilesEmpty = piles:empty()  -- Returns true if empty
    local numPiles = piles:size()       -- Returns 0

    -- ==========================================
    -- Pile API & VectorCard API
    -- ==========================================
    -- 0: Tableau
    local p = Pile.new()
    p.id = 0
    p.pos = pos1
    p.size = CardSize
    p.offset = ImVec2.new(0.0, 25.0)
    
    -- PileType Enum: Stock, Waste, Tableau, Foundation, FreeCellSlot, Invisible
    p.type = PileType.Tableau 

    -- Deal 5 cards to Tableau to play around with
    for i = 1, 5 do
        if not deck:empty() then
            local c = deck:back()
            c.faceUp = true
            p.cards:push_back(c)
            deck:pop_back()
        end
    end

    -- Extra API Demo (doesn't impact game state)
    if not p.cards:empty() then
        local demoCard = p.cards:back()
        local isRed = demoCard:IsRed()
        local color = demoCard:Color()
    end

    -- Put the pile into the game board
    piles:push_back(p)
    
    -- 1: Stock (contains remaining deck)
    local stock = Pile.new()
    stock.id = 1
    stock.type = PileType.Stock
    stock.pos = pos2
    stock.size = CardSize
    stock.offset = ImVec2.new(0.2, -0.5)
    stock.cards = deck                  -- Reassign entire VectorCard at once
    piles:push_back(stock)

    -- 2: Waste (target for stock clicks)
    local waste = Pile.new()
    waste.id = 2
    waste.type = PileType.Waste
    waste.pos = pos3
    waste.size = CardSize
    waste.offset = ImVec2.new(20.0, 0.0)
    piles:push_back(waste)

    -- 3: Foundation (target for Tableau cards)
    local foundation = Pile.new()
    foundation.id = 3
    foundation.type = PileType.Foundation
    foundation.pos = pos4
    foundation.size = CardSize
    foundation.offset = ImVec2.new(0.0, 0.0)
    piles:push_back(foundation)
end

-- Called by C++ to determine if a specific card can be dragged.
function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    local c = p.cards:get(cardIdx)
    
    -- Cannot pick up from Stock
    if p.type == PileType.Stock then return false end

    -- Only allow picking up face-up cards
    return c.faceUp
end

-- Called by C++ to determine if a dragged stack of cards can be dropped on a target pile.
function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    local targetPile = piles:get(targetPileIdx)
    
    -- Drop anywhere except Stock
    return targetPile.type ~= PileType.Stock
end

-- Called by C++ AFTER a successful drop to handle game-specific rules (like flipping the newly exposed card).
function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx)
    local sourcePile = piles:get(sourcePileIdx)
    if not sourcePile.cards:empty() then
        sourcePile.cards:back().faceUp = true
    end
end

-- Called by C++ when a pile is clicked without dragging (e.g., clicking the Stock pile).
function HandleClick(piles, pileIdx)
    local p = piles:get(pileIdx)
    
    -- Deal cards from Stock to Waste
    if p.type == PileType.Stock then
        local waste = piles:get(2)
        if not p.cards:empty() then
            local c = p.cards:back()
            p.cards:pop_back()
            c.faceUp = true
            waste.cards:push_back(c)
        else
            -- Recycle waste back to stock
            while not waste.cards:empty() do
                local c = waste.cards:back()
                waste.cards:pop_back()
                c.faceUp = false
                p.cards:push_back(c)
            end
        end
    end
end

-- Called by C++ every frame if no drag is happening. 
-- Return a table formatted as {sourcePileIdx, targetPileIdx, cardIdx} to automatically move a card, or {} to do nothing.
function AutoSolve(piles)
    -- Example AutoSolve: automatically move Aces from Tableau to Foundation
    local tableau = piles:get(0)
    if not tableau.cards:empty() then
        local topCard = tableau.cards:back()
        if topCard.rank == Rank.Ace then
            return {0, 3, tableau.cards:size() - 1}
        end
    end
    return {}
end

-- Called by C++ every frame to check if the fireworks should trigger.
function IsWon(piles)
    -- Example condition: game is won if the Tableau (pile 0) is completely empty
    return piles:size() > 0 and piles:get(0).cards:empty()
end

-- Called by C++ every frame to draw custom graphics/text on the board.
function Draw()
    -- DrawBoardText coordinates scale identically to the Pile positioning
    DrawBoardText(20.0, 210.0, "Tableau")
    DrawBoardText(20.0, 10.0, "Stock")
    DrawBoardText(180.0, 10.0, "Waste")
    DrawBoardText(380.0, 10.0, "Foundation")
    
    DrawBoardText(20.0, 520.0, "Move all cards from the Tableau to the Foundation to see the fireworks!")

    -- DrawBoardButton returns true when clicked
    if DrawBoardButton(20.0, 560.0, 200.0, 40.0, "Click Me! (" .. ButtonClickCount .. ")") then
        ButtonClickCount = ButtonClickCount + 1
    end
end