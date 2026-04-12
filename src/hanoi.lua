GameName = "Tower of Hanoi"
NumDecks = 1
AutoCenter = false
HelpText = "A classic puzzle! Move all disks from Peg 1 to Peg 3.\nRules:\n1. Move only one card at a time.\n2. A higher rank card cannot be placed on top of a lower rank card."

function Init(piles, deck)
    local num_disks = 5

    -- Create 3 Pegs (Tableaus)
    for i = 0, 2 do
        local p = Pile.new()
        p.id = i
        p.type = PileType.Tableau
        p.pos = ImVec2.new(200.0 + i * 200.0, 450.0) 
        p.size = ImVec2.new(100.0, 140.0)
        -- Negative Y offset means the cards will stack UPWARDS like a tower!
        p.offset = ImVec2.new(0.0, -30.0) 
        piles:push_back(p)
    end

    -- Setup first peg with disks (Rank 5 at the bottom, down to Rank 1 at the top)
    local peg1 = piles:get(0)
    for r = num_disks, 1, -1 do
        if not deck:empty() then
            -- Move a card from the deck to our peg safely
            local card = deck:back()
            peg1.cards:push_back(card)
            deck:pop_back()
            
            -- Override the card's visual and logical state to be our "disk"
            local newly_added = peg1.cards:back()
            newly_added.rank = r
            newly_added.suit = Suit.Spades
            newly_added.faceUp = true
        end
    end
    
    -- Hide the remainder of the deck off-screen
    local hidden = Pile.new()
    hidden.id = 3
    hidden.type = PileType.Invisible
    hidden.pos = ImVec2.new(-1000.0, -1000.0)
    hidden.cards = deck
    piles:push_back(hidden)
end

function CanPickup(piles, pileIdx, cardIdx)
    if pileIdx > 2 then return false end
    local p = piles:get(pileIdx)
    -- Strict Hanoi rule: Only allow picking up the very top card of the pile
    return cardIdx == p.cards:size() - 1
end

function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards)
    if targetPileIdx > 2 then return false end
    
    local target = piles:get(targetPileIdx)
    if target.cards:empty() then return true end

    local dropCard = dragCards:get(0)
    local targetTop = target.cards:back()

    -- Strict Hanoi rule: Can only place a smaller "disk" (lower rank) on a larger "disk" (higher rank)
    return dropCard.rank < targetTop.rank
end

function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx) end
function HandleClick(piles, pileIdx) end
function AutoSolve(piles) return {} end

function IsWon(piles)
    -- Game is won when all 5 disks successfully make it to the 3rd peg
    return piles:get(2).cards:size() == 5
end

function Draw()
    DrawBoardText(220.0, 600.0, "Peg 1")
    DrawBoardText(420.0, 600.0, "Peg 2")
    DrawBoardText(620.0, 600.0, "Peg 3")
    
    DrawBoardText(200.0, 50.0, "Tower of Hanoi")
    DrawBoardText(200.0, 100.0, "Rules:")
    DrawBoardText(200.0, 130.0, "- Move all cards to Peg 3.")
    DrawBoardText(200.0, 160.0, "- You can only move one card at a time.")
    DrawBoardText(200.0, 190.0, "- You cannot place a higher rank card on top of a lower rank card.")
end