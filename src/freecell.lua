GameName = "FreeCell"

-- Deep teal window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {13, 26, 29}, BackgroundBottom = {9, 18, 21},
    Toolbar = {18, 35, 39}, ButtonRounding = 6,
    EmptyPile = {18, 35, 39, 180}, EmptyPileBorder = {58, 94, 100},
    EmptyPileText = {146, 178, 177}, CardBack = {29, 66, 70},
    CardBorder = {58, 94, 100}, CardHover = {116, 215, 201},
    Colors = {
        Text = {226, 240, 236}, TextDisabled = {146, 178, 177},
        WindowBg = {18, 35, 39}, ChildBg = {18, 35, 39},
        PopupBg = {24, 45, 49}, MenuBarBg = {18, 35, 39},
        Border = {58, 94, 100}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {30, 63, 67}, FrameBgHovered = {44, 82, 85},
        FrameBgActive = {58, 103, 102}, Button = {30, 63, 67},
        ButtonHovered = {44, 82, 85}, ButtonActive = {58, 103, 102},
        Header = {30, 63, 67}, HeaderHovered = {44, 82, 85},
        HeaderActive = {58, 103, 102}, TitleBg = {18, 35, 39},
        TitleBgActive = {24, 45, 49}, TitleBgCollapsed = {18, 35, 39},
        Separator = {58, 94, 100}, SeparatorHovered = {116, 215, 201},
        SeparatorActive = {116, 215, 201}, ScrollbarBg = {13, 26, 29},
        ScrollbarGrab = {58, 94, 100}, ScrollbarGrabHovered = {44, 82, 85},
        ScrollbarGrabActive = {58, 103, 102}, CheckMark = {116, 215, 201},
        SliderGrab = {116, 215, 201}, SliderGrabActive = {116, 215, 201},
        TextSelectedBg = {116, 215, 201, 65}, NavCursor = {116, 215, 201},
        ModalWindowDimBg = {9, 18, 21, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(90, 126)
HelpText = 'Build foundations up by suit and tableau runs down in alternating colors. Free cells hold one card each. Any card may fill an empty column. Group capacity is (empty cells + 1) times 2 for each spare empty column; the destination column is not spare. Foundations can be moved back to the tableau. Use Hint and Safe foundations. Ctrl+Z/Ctrl+Y undo/redo; F2 restarts.'
local game = Solitaire.new({kind="freecell", first=8, last=15, foundationFirst=4, foundationLast=7, goal=52,
    progress="Foundation cards", mainLabel="Send safe cards", description="All cards are visible. Use free cells to move alternating-color runs.",
    tip="Moving a group needs spare cells and columns. An empty destination is not a spare column."})

function Init(piles, deck)
    game:Reset(piles)
    for i=0,3 do game:Add(i, PileType.FreeCellSlot, 64+i*116, 160) end
    for i=0,3 do game:Add(4+i, PileType.Foundation, 528+i*116, 160) end
    for i=0,7 do game:Add(8+i, PileType.Tableau, 64+i*116, 310) end
    local column=8
    while not deck:empty() do
        local card=deck:take_back(); card.faceUp=true
        piles:get(column).cards:push_back(card); column=8+(column-7)%8
    end
    game:Layout()
end

function CanPickup(piles, source, index)
    local pile=piles:get(source)
    if index<0 or index>=pile.cards:size() or not pile.cards:get(index).faceUp then return false end
    if source<8 then return index==pile.cards:size()-1 end
    for i=index,pile.cards:size()-2 do
        local a,b=pile.cards:get(i),pile.cards:get(i+1)
        if not b.faceUp or a:IsRed()==b:IsRed() or a.rank~=b.rank+1 then return false end
    end
    return true
end

function MoveCapacity(piles, source, target)
    local cells,columns=0,0
    for i=0,3 do if piles:get(i).cards:empty() then cells=cells+1 end end
    for i=8,15 do
        if i~=source and i~=target and piles:get(i).cards:empty() then columns=columns+1 end
    end
    return (cells+1)*2^columns
end

function CanDrop(piles, source, target, cards)
    if source==target or cards:empty() then return false end
    local pile,card=piles:get(target),cards:front()
    if target<4 then return cards:size()==1 and pile.cards:empty() end
    if target<8 then
        if cards:size()~=1 then return false end
        if pile.cards:empty() then return card.rank==1 end
        local top=pile.cards:back()
        return top.suit==card.suit and top.rank+1==card.rank
    end
    if cards:size()>MoveCapacity(piles,source,target) then return false end
    if pile.cards:empty() then return true end
    local top=pile.cards:back()
    return top:IsRed()~=card:IsRed() and top.rank==card.rank+1
end
function AfterMove() game:Record() end
function HandleClick() end

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
