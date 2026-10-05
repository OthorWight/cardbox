-- Global variables expected by the C++ engine
GameName = "Example"

-- Cobalt window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {17, 22, 34}, BackgroundBottom = {11, 15, 25},
    Toolbar = {24, 32, 47}, ButtonRounding = 6,
    EmptyPile = {24, 32, 47, 180}, EmptyPileBorder = {64, 85, 115},
    EmptyPileText = {149, 172, 199}, CardBack = {35, 59, 87},
    CardBorder = {64, 85, 115}, CardHover = {126, 185, 245},
    Colors = {
        Text = {230, 237, 246}, TextDisabled = {149, 172, 199},
        WindowBg = {24, 32, 47}, ChildBg = {24, 32, 47},
        PopupBg = {33, 44, 63}, MenuBarBg = {24, 32, 47},
        Border = {64, 85, 115}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {35, 59, 89}, FrameBgHovered = {51, 80, 113},
        FrameBgActive = {69, 102, 137}, Button = {35, 59, 89},
        ButtonHovered = {51, 80, 113}, ButtonActive = {69, 102, 137},
        Header = {35, 59, 89}, HeaderHovered = {51, 80, 113},
        HeaderActive = {69, 102, 137}, TitleBg = {24, 32, 47},
        TitleBgActive = {33, 44, 63}, TitleBgCollapsed = {24, 32, 47},
        Separator = {64, 85, 115}, SeparatorHovered = {126, 185, 245},
        SeparatorActive = {126, 185, 245}, ScrollbarBg = {17, 22, 34},
        ScrollbarGrab = {64, 85, 115}, ScrollbarGrabHovered = {51, 80, 113},
        ScrollbarGrabActive = {69, 102, 137}, CheckMark = {126, 185, 245},
        SliderGrab = {126, 185, 245}, SliderGrabActive = {126, 185, 245},
        TextSelectedBg = {126, 185, 245, 65}, NavCursor = {126, 185, 245},
        ModalWindowDimBg = {11, 15, 25, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(94,132)
CornerRadius = 12
HelpText = [[A small tutorial for the Lua game API. Collect all 52 cards in the archive. Click Draw to expose a stock card, then click or drag an exposed card into the archive. Tableau groups may move to another work column in any order; only single cards enter the archive. Empty stock recycles the remaining waste. Undo restores cards, progress, and move count. Unlike a solitaire puzzle, every deal can be completed.]]
local game=Solitaire.new({kind="tutorial",first=0,last=2,safeLabel="Archive exposed",foundationFirst=5,foundationLast=5,stock=3,waste=4,goal=52,
    progress="Cards archived",mainLabel="Draw / recycle",description="Learn the controls. Collect all 52 cards in the archive; every deal is solvable.",
    tip="Click the exposed card in a work column or waste to archive it. Dragging works too."})
function Init(piles,deck)
    game:Reset(piles)
    for i=0,2 do
        game:Add(i,PileType.Tableau,120+i*260,335)
        for j=1,4 do local card=deck:take_back(); card.faceUp=true; piles:get(i).cards:push_back(card) end
    end
    game:Add(3,PileType.Stock,120,165,ImVec2.new(0.15,-0.3))
    game:Add(4,PileType.Waste,290,165,ImVec2.new(16,0))
    game:Add(5,PileType.Foundation,800,165); piles:get(3).cards=deck; game:Layout()
end
function CanPickup(piles,source,index)
    if source<0 or source>4 or source==3 then return false end
    local cards=piles:get(source).cards
    return index>=0 and index<cards:size() and cards:get(index).faceUp and (source~=4 or index==cards:size()-1)
end
function CanDrop(piles,source,target,cards)
    if source==target or source<0 or source>4 or source==3 or cards:empty() then return false end
    return (target>=0 and target<=2) or (target==5 and cards:size()==1)
end
function AfterMove() game:Record() end
function HandleClick(piles,source,index)
    if source==3 then
        local stock,waste=piles:get(3).cards,piles:get(4).cards
        if not stock:empty() then
            local card=stock:take_back(); card.faceUp=true; waste:push_back(card); game.draws=game.draws+1
        elseif not waste:empty() then
            while not waste:empty() do local card=waste:take_back(); card.faceUp=false; stock:push_back(card) end
            game.passes=game.passes+1
        else game.notice="Archive the remaining work cards to finish."; return end
        game:Record()
    elseif source>=0 and source<=4 and source~=3 then
        local cards=piles:get(source).cards
        if not cards:empty() and (not index or index<0 or index==cards:size()-1) then
            piles:get(5).cards:push_back(cards:take_back()); game:Record()
        end
    end
end
function HandleAction(piles,action)
    game.piles=piles
    if action=="main" then HandleClick(piles,3)
    elseif action=="hint" then game:Hint()
    elseif action=="safe" then
        local moved=false
        for i=0,4 do
            if i~=3 then
                local cards=piles:get(i).cards
                if not cards:empty() then piles:get(5).cards:push_back(cards:take_back()); moved=true end
            end
        end
        if moved then game:Record() end
    end
end
function IsWon() return game:Progress()==52 end
function AutoSolve(piles) game.piles=piles; game:Layout(); return {} end
function SaveState() return game:SaveState() end
function LoadState(piles,data) game:LoadState(piles,data) end
function DrawBackground() game:Background() end
function Draw()
    game:Draw()
    game:Text(120,140,"STOCK",Theme.EmptyPileText,16)
    game:Text(290,140,"WASTE",Theme.EmptyPileText,16)
    game:Text(800,140,"ARCHIVE",Theme.CardHover,16)
    for i=0,2 do game:Text(120+i*260,307,"WORK COLUMN "..(i+1),Theme.EmptyPileText,16) end
end
