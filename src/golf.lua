GameName = "Golf"

-- Forest window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {17, 27, 24}, BackgroundBottom = {11, 19, 17},
    Toolbar = {24, 38, 32}, ButtonRounding = 6,
    EmptyPile = {24, 38, 32, 180}, EmptyPileBorder = {65, 93, 76},
    EmptyPileText = {159, 180, 163}, CardBack = {40, 65, 49},
    CardBorder = {65, 93, 76}, CardHover = {163, 211, 139},
    Colors = {
        Text = {235, 240, 222}, TextDisabled = {159, 180, 163},
        WindowBg = {24, 38, 32}, ChildBg = {24, 38, 32},
        PopupBg = {32, 50, 42}, MenuBarBg = {24, 38, 32},
        Border = {65, 93, 76}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {44, 68, 50}, FrameBgHovered = {61, 88, 64},
        FrameBgActive = {78, 110, 80}, Button = {44, 68, 50},
        ButtonHovered = {61, 88, 64}, ButtonActive = {78, 110, 80},
        Header = {44, 68, 50}, HeaderHovered = {61, 88, 64},
        HeaderActive = {78, 110, 80}, TitleBg = {24, 38, 32},
        TitleBgActive = {32, 50, 42}, TitleBgCollapsed = {24, 38, 32},
        Separator = {65, 93, 76}, SeparatorHovered = {163, 211, 139},
        SeparatorActive = {163, 211, 139}, ScrollbarBg = {17, 27, 24},
        ScrollbarGrab = {65, 93, 76}, ScrollbarGrabHovered = {61, 88, 64},
        ScrollbarGrabActive = {78, 110, 80}, CheckMark = {163, 211, 139},
        SliderGrab = {163, 211, 139}, SliderGrabActive = {163, 211, 139},
        TextSelectedBg = {163, 211, 139, 65}, NavCursor = {163, 211, 139},
        ModalWindowDimBg = {11, 19, 17, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(90, 126)
HelpText = 'Clear all 35 tableau cards by playing an exposed card one rank above or below the waste. King and Ace wrap. Click or drag the exposed card. There are 16 stock draws and no recycling. Hint shows a legal move. If the stock is empty and no moves remain, undo or start a new deal.'
local game = Solitaire.new({kind="golf", first=0, last=6, stock=7, waste=8, goal=35, spacing=30,
    progress="Cards cleared", mainLabel="Draw a card", description="Play one rank above or below the waste. Kings and Aces wrap.",
    tip="Click an exposed card to play it. No stock recycling; plan before drawing."})

function Init(piles,deck)
    game:Reset(piles)
    for i=0,6 do
        game:Add(i,PileType.Tableau,64+i*128,310)
        for j=1,5 do local card=deck:take_back(); card.faceUp=true; piles:get(i).cards:push_back(card) end
    end
    game:Add(7,PileType.Stock,64,160,ImVec2.new(0.15,-0.3))
    game:Add(8,PileType.Waste,190,160,ImVec2.new(18,0))
    piles:get(7).cards=deck
    local card=piles:get(7).cards:take_back(); card.faceUp=true; piles:get(8).cards:push_back(card)
    game:Layout()
end
function CanPickup(piles,source,index)
    if source<0 or source>6 then return false end
    return index>=0 and index==piles:get(source).cards:size()-1
end
function CanDrop(piles,source,target,cards)
    if source<0 or source>6 or target~=8 or cards:size()~=1 then return false end
    local waste=piles:get(8).cards
    if waste:empty() then return true end
    local difference=math.abs(waste:back().rank-cards:front().rank)
    return difference==1 or difference==12
end
function AfterMove() game:Record() end
function HandleClick(piles,source,index)
    if source==7 then
        local stock=piles:get(7).cards
        if stock:empty() then game.notice="No stock cards remain. Undo or try a new deal."; return end
        local card=stock:take_back(); card.faceUp=true; piles:get(8).cards:push_back(card)
        game.draws=game.draws+1; game:Record()
    elseif source>=0 and source<=6 then
        local cards=piles:get(source).cards
        if cards:empty() or (index and index>=0 and index~=cards:size()-1) then return end
        local stack=VectorCard.new(); stack:push_back(cards:back())
        if CanDrop(piles,source,8,stack) then piles:get(8).cards:push_back(cards:take_back()); game:Record()
        else game.notice="That card must be one rank above or below the waste." end
    end
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
