-- Global variables expected by the C++ engine
GameName = "API Example"
NumDecks = 1
HelpText = "This is a dummy game demonstrating all available Lua API bindings."

-- Init is called once when the game starts or restarts.
-- @param piles: A VectorPile object (std::vector<Pile> in C++)
-- @param deck: A VectorCard object (std::vector<Card> in C++) containing the shuffled deck
function Init(piles, deck)
    -- ==========================================
    -- ImVec2 API
    -- ==========================================
    local pos1 = ImVec2.new()           -- Default constructor (0,0)
    local pos2 = ImVec2.new(100.0, 20)  -- Constructor with x, y
    pos1.x = 20.0                       -- Modify x property
    pos1.y = 20.0                       -- Modify y property

    -- ==========================================
    -- VectorPile API
    -- ==========================================
    piles:clear()                       -- Clear all piles (already empty at start, but good for demo)
    local isPilesEmpty = piles:empty()  -- Returns true if empty
    local numPiles = piles:size()       -- Returns 0

    -- ==========================================
    -- Pile API
    -- ==========================================
    local p = Pile.new()
    p.id = 0
    p.pos = pos1
    p.size = ImVec2.new(100.0, 140.0)
    p.offset = ImVec2.new(0.0, 25.0)
    
    -- PileType Enum: Stock, Waste, Tableau, Foundation, FreeCellSlot, Invisible
    p.type = PileType.Tableau 

    -- ==========================================
    -- VectorCard API
    -- ==========================================
    local tempCards = VectorCard.new()  -- You can create your own VectorCard instances
    tempCards:clear()

    if not deck:empty() then
        local numCards = deck:size()
        local firstCard = deck:front()  -- Returns the first Card (index 0)
        local lastCard = deck:back()    -- Returns the last Card
        local specificCard = deck:get(0)-- Returns Card at index 0
        
        -- ==========================================
        -- Card API
        -- ==========================================
        lastCard.faceUp = true          -- Set faceUp boolean
        
        -- Rank Enum: Ace, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King
        lastCard.rank = Rank.Ace        
        
        -- Suit Enum: Hearts, Diamonds, Clubs, Spades
        lastCard.suit = Suit.Spades     
        
        -- Card Methods
        local isRed = lastCard:IsRed()  -- Returns boolean (true for Hearts/Diamonds)
        local color = lastCard:Color()  -- Returns 0 for Black, 1 for Red
        
        -- Moving a card from the deck to our pile
        p.cards:push_back(lastCard)     -- Add card to end of pile
        deck:pop_back()                 -- Remove card from end of deck
    end

    -- Put the pile into the game board
    piles:push_back(p)
    
    -- Put the remaining deck into a stock pile so it's not lost
    local stock = Pile.new()
    stock.id = 1
    stock.type = PileType.Stock
    stock.pos = pos2
    stock.cards = deck                  -- Reassign entire VectorCard at once
    piles:push_back(stock)
end

-- Called by C++ to determine if a specific card can be dragged.
function CanPickup(piles, pileIdx, cardIdx)
    local p = piles:get(pileIdx)
    local c = p.cards:get(cardIdx)
    
    -- Only allow picking up face-up cards
    return c.faceUp
end

-- Called by C++ to determine if a dragged stack of cards can be dropped on a target pile.
function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    local targetPile = piles:get(targetPileIdx)
    local bottomDragCard = dragCards:get(0)
    
    -- Example: Only drop if the target pile is empty and we're dragging a King
    return targetPile.cards:empty() and bottomDragCard.rank == Rank.King
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
    if p.type == PileType.Stock and not p.cards:empty() then
        -- Example action: flip the top card
        p.cards:back().faceUp = not p.cards:back().faceUp
    end
end

-- Called by C++ every frame if no drag is happening. 
-- Return a table formatted as {sourcePileIdx, targetPileIdx, cardIdx} to automatically move a card, or {} to do nothing.
function AutoSolve(piles)
    return {}
end

-- Called by C++ every frame to check if the fireworks should trigger.
function IsWon(piles)
    -- Example condition: game is won if the first pile is completely empty
    return piles:size() > 0 and piles:get(0).cards:empty()
end