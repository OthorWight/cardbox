GameName = "Spider"

-- Plum window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {25, 20, 34}, BackgroundBottom = {17, 13, 24},
    Toolbar = {35, 28, 46}, ButtonRounding = 6,
    EmptyPile = {35, 28, 46, 180}, EmptyPileBorder = {90, 74, 112},
    EmptyPileText = {176, 157, 193}, CardBack = {58, 43, 80},
    CardBorder = {90, 74, 112}, CardHover = {193, 166, 239},
    Colors = {
        Text = {240, 233, 245}, TextDisabled = {176, 157, 193},
        WindowBg = {35, 28, 46}, ChildBg = {35, 28, 46},
        PopupBg = {46, 37, 60}, MenuBarBg = {35, 28, 46},
        Border = {90, 74, 112}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {64, 48, 86}, FrameBgHovered = {83, 64, 109},
        FrameBgActive = {104, 80, 132}, Button = {64, 48, 86},
        ButtonHovered = {83, 64, 109}, ButtonActive = {104, 80, 132},
        Header = {64, 48, 86}, HeaderHovered = {83, 64, 109},
        HeaderActive = {104, 80, 132}, TitleBg = {35, 28, 46},
        TitleBgActive = {46, 37, 60}, TitleBgCollapsed = {35, 28, 46},
        Separator = {90, 74, 112}, SeparatorHovered = {193, 166, 239},
        SeparatorActive = {193, 166, 239}, ScrollbarBg = {25, 20, 34},
        ScrollbarGrab = {90, 74, 112}, ScrollbarGrabHovered = {83, 64, 109},
        ScrollbarGrabActive = {104, 80, 132}, CheckMark = {193, 166, 239},
        SliderGrab = {193, 166, 239}, SliderGrabActive = {193, 166, 239},
        TextSelectedBg = {193, 166, 239, 65}, NavCursor = {193, 166, 239},
        ModalWindowDimBg = {17, 13, 24, 190}
    }
}

NumDecks = 2
AutoCenter = false
CardSize = ImVec2.new(86, 120)
HelpText = 'One-suit Spider uses 104 cards. Build descending runs and collect eight complete King-to-Ace runs. Move only ordered face-up groups. Deal one card to all ten columns only when every column has a card. Hint does not move cards. Moves and completed runs are undoable. Ctrl+Z/Ctrl+Y undo/redo; F2 restarts.'
local game = Solitaire.new({kind="spider", first=9, last=18, stock=8, foundationFirst=0, foundationLast=7, goal=8, spacing=24,
    progress="Completed runs", mainLabel="Deal a row", description="One suit, two decks. Complete eight King-to-Ace runs.",
    tip="Fill every empty column before dealing. Completed runs are collected automatically."})

function Init(piles,deck)
    game:Reset(piles)
    for i=0,7 do game:Add(i,PileType.Foundation,240+i*94,160) end
    game:Add(8,PileType.Stock,64,160,ImVec2.new(0.12,-0.25))
    for i=0,9 do game:Add(9+i,PileType.Tableau,64+i*94,310) end
    for i=0,53 do
        local card=deck:take_back(); card.suit=Suit.Spades; card.faceUp=false
        piles:get(9+i%10).cards:push_back(card)
    end
    for i=9,18 do piles:get(i).cards:back().faceUp=true end
    while not deck:empty() do
        local card=deck:take_back(); card.suit=Suit.Spades; card.faceUp=false
        piles:get(8).cards:push_back(card)
    end
    game:Layout()
end
function CanPickup(piles,source,index)
    if source<9 or source>18 then return false end
    local cards=piles:get(source).cards
    if index<0 or index>=cards:size() or not cards:get(index).faceUp then return false end
    for i=index,cards:size()-2 do
        local a,b=cards:get(i),cards:get(i+1)
        if not b.faceUp or a.suit~=b.suit or a.rank~=b.rank+1 then return false end
    end
    return true
end
function CanDrop(piles,source,target,cards)
    if source<9 or target<9 or source==target or cards:empty() then return false end
    local destination=piles:get(target).cards
    return destination:empty() or destination:back().rank==cards:front().rank+1
end
local function CollectRuns(piles)
    for column=9,18 do
        local cards=piles:get(column).cards
        while cards:size()>=13 do
            local start,valid=cards:size()-13,true
            local suit=cards:get(start).suit
            for i=0,12 do
                local card=cards:get(start+i)
                if not card.faceUp or card.rank~=13-i or card.suit~=suit then valid=false; break end
            end
            if not valid then break end
            local destination=nil
            for i=0,7 do if piles:get(i).cards:empty() then destination=piles:get(i).cards; break end end
            if not destination then break end
            for i=start,start+12 do destination:push_back(cards:get(i)) end
            for i=1,13 do cards:pop_back() end
            if not cards:empty() then cards:back().faceUp=true end
        end
    end
end
function AfterMove(piles,source)
    local cards=piles:get(source).cards
    if source>=9 and not cards:empty() then cards:back().faceUp=true end
    CollectRuns(piles); game:Record()
end
function HandleClick(piles,source)
    if source~=8 then return end
    local stock=piles:get(8).cards
    if stock:empty() then game.notice="No stock rows remain."; return end
    for i=9,18 do
        if piles:get(i).cards:empty() then game.notice="Fill every empty column before dealing a row."; return end
    end
    if stock:size()<10 then game.notice="The stock cannot supply a full row."; return end
    for i=9,18 do local card=stock:take_back(); card.faceUp=true; piles:get(i).cards:push_back(card) end
    game.draws=game.draws+1; CollectRuns(piles); game:Record()
end

function IsWon() return game:Progress()==game.config.goal end
function AutoSolve(piles) game.piles=piles; game:Layout(); return {} end
function SaveState() return game:SaveState() end
function LoadState(piles,data) game:LoadState(piles,data) end
function DrawBackground() game:Background() end
function Draw() game:Draw() end
function HandleAction(piles,action)
    game.piles=piles
    if action=="hint" then game:Hint()
    elseif action=="safe" and game.config.foundationFirst and game.config.kind~="spider" then game:SafeFoundations()
    elseif action=="main" then
        if game.config.stock then HandleClick(piles,game.config.stock)
        elseif game.config.foundationFirst then game:SafeFoundations() end
    end
end
