GameName = "Klondike"

-- Slate blue window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {15, 23, 35}, BackgroundBottom = {10, 16, 26},
    Toolbar = {20, 30, 44}, ButtonRounding = 6,
    EmptyPile = {20, 30, 44, 180}, EmptyPileBorder = {65, 84, 105},
    EmptyPileText = {150, 168, 188}, CardBack = {40, 65, 96},
    CardBorder = {65, 84, 105}, CardHover = {135, 192, 244},
    Colors = {
        Text = {231, 236, 241}, TextDisabled = {150, 168, 188},
        WindowBg = {20, 30, 44}, ChildBg = {20, 30, 44},
        PopupBg = {28, 41, 58}, MenuBarBg = {20, 30, 44},
        Border = {65, 84, 105}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {37, 56, 78}, FrameBgHovered = {52, 75, 101},
        FrameBgActive = {70, 94, 120}, Button = {37, 56, 78},
        ButtonHovered = {52, 75, 101}, ButtonActive = {70, 94, 120},
        Header = {37, 56, 78}, HeaderHovered = {52, 75, 101},
        HeaderActive = {70, 94, 120}, TitleBg = {20, 30, 44},
        TitleBgActive = {28, 41, 58}, TitleBgCollapsed = {20, 30, 44},
        Separator = {65, 84, 105}, SeparatorHovered = {135, 192, 244},
        SeparatorActive = {135, 192, 244}, ScrollbarBg = {15, 23, 35},
        ScrollbarGrab = {65, 84, 105}, ScrollbarGrabHovered = {52, 75, 101},
        ScrollbarGrabActive = {70, 94, 120}, CheckMark = {135, 192, 244},
        SliderGrab = {135, 192, 244}, SliderGrabActive = {135, 192, 244},
        TextSelectedBg = {135, 192, 244, 65}, NavCursor = {135, 192, 244},
        ModalWindowDimBg = {10, 16, 26, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(90, 126)
HelpText = 'Draw-one Klondike. Build tableau runs down in alternating colors; only Kings fill empty columns. Build foundations up by suit from Ace to King. Unlimited stock recycling. Hint shows a legal move. Safe foundations only moves cards that will not trap lower ranks. Ctrl+Z/Ctrl+Y undo/redo; F2 starts a new deal.'
local game = Solitaire.new({kind="klondike", first=6, last=12, stock=0, waste=1, foundationFirst=2, foundationLast=5, goal=52,
    progress="Foundation cards", mainLabel="Draw / recycle", description="Draw one. Build alternating colors down; foundations climb by suit.",
    tip="Only Kings fill empty columns. Right-click an exposed card to send it to a foundation."})

function Init(piles, deck)
    game:Reset(piles)
    game:Add(0, PileType.Stock, 64, 160, ImVec2.new(0.15, -0.3))
    game:Add(1, PileType.Waste, 190, 160, ImVec2.new(18, 0))
    for i=0,3 do game:Add(2+i, PileType.Foundation, 490+i*116, 160) end
    for i=0,6 do
        game:Add(6+i, PileType.Tableau, 64+i*128, 330)
        for j=0,i do
            local card=deck:take_back(); card.faceUp=j==i
            piles:get(6+i).cards:push_back(card)
        end
    end
    piles:get(0).cards=deck
    game:Layout()
end

function CanPickup(piles, source, index)
    local pile=piles:get(source)
    if source==0 or index<0 or index>=pile.cards:size() then return false end
    if not pile.cards:get(index).faceUp then return false end
    if source<=5 then return index==pile.cards:size()-1 end
    for i=index,pile.cards:size()-2 do
        local a,b=pile.cards:get(i),pile.cards:get(i+1)
        if not b.faceUp or a:IsRed()==b:IsRed() or a.rank~=b.rank+1 then return false end
    end
    return true
end

function CanDrop(piles, source, target, cards)
    if source==target or cards:empty() or target<2 then return false end
    local pile,card=piles:get(target),cards:front()
    if target<=5 then
        if cards:size()~=1 then return false end
        if pile.cards:empty() then return card.rank==1 end
        local top=pile.cards:back()
        return top.suit==card.suit and top.rank+1==card.rank
    end
    if pile.cards:empty() then return card.rank==13 end
    local top=pile.cards:back()
    return top.faceUp and top:IsRed()~=card:IsRed() and top.rank==card.rank+1
end

function AfterMove(piles, source, target, index)
    local cards=piles:get(source).cards
    if source>=6 and not cards:empty() then cards:back().faceUp=true end
    game:Record()
end

function HandleClick(piles, source)
    if source~=0 then return end
    local stock,waste=piles:get(0).cards,piles:get(1).cards
    if not stock:empty() then
        local card=stock:take_back(); card.faceUp=true; waste:push_back(card)
        game.draws=game.draws+1
    elseif not waste:empty() then
        while not waste:empty() do local card=waste:take_back(); card.faceUp=false; stock:push_back(card) end
        game.passes=game.passes+1
    else game.notice="The stock and waste are empty."; return end
    game:Record()
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
