GameName = "Pyramid"

-- Sandstone window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {29, 24, 20}, BackgroundBottom = {20, 17, 15},
    Toolbar = {39, 32, 25}, ButtonRounding = 6,
    EmptyPile = {39, 32, 25, 180}, EmptyPileBorder = {98, 82, 60},
    EmptyPileText = {183, 166, 140}, CardBack = {65, 52, 37},
    CardBorder = {98, 82, 60}, CardHover = {233, 194, 112},
    Colors = {
        Text = {244, 235, 214}, TextDisabled = {183, 166, 140},
        WindowBg = {39, 32, 25}, ChildBg = {39, 32, 25},
        PopupBg = {52, 43, 33}, MenuBarBg = {39, 32, 25},
        Border = {98, 82, 60}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {73, 60, 40}, FrameBgHovered = {94, 77, 50},
        FrameBgActive = {116, 95, 62}, Button = {73, 60, 40},
        ButtonHovered = {94, 77, 50}, ButtonActive = {116, 95, 62},
        Header = {73, 60, 40}, HeaderHovered = {94, 77, 50},
        HeaderActive = {116, 95, 62}, TitleBg = {39, 32, 25},
        TitleBgActive = {52, 43, 33}, TitleBgCollapsed = {39, 32, 25},
        Separator = {98, 82, 60}, SeparatorHovered = {233, 194, 112},
        SeparatorActive = {233, 194, 112}, ScrollbarBg = {29, 24, 20},
        ScrollbarGrab = {98, 82, 60}, ScrollbarGrabHovered = {94, 77, 50},
        ScrollbarGrabActive = {116, 95, 62}, CheckMark = {233, 194, 112},
        SliderGrab = {233, 194, 112}, SliderGrabActive = {233, 194, 112},
        TextSelectedBg = {233, 194, 112, 65}, NavCursor = {233, 194, 112},
        ModalWindowDimBg = {20, 17, 15, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(80, 112)
HelpText = 'Clear the 28-card pyramid. Pair exposed cards totaling 13: Ace=1, Jack=11, Queen=12, King=13. Click two cards or drag one onto its partner. Click or right-click an exposed King to remove it. You may use the waste top. Two stock redeals are allowed. Ctrl+Z/Ctrl+Y undo/redo restores draws, redeals, and pairs.'
local game = Solitaire.new({kind="pyramid", first=2, last=29, stock=0, waste=1, goal=28,
    progress="Pyramid cleared", mainLabel="Draw / recycle", description="Pair exposed cards totaling 13. Click Kings to remove them.",
    tip="Click one exposed card, then its partner. Dragging also works. Two stock redeals are allowed."})

function Init(piles,deck)
    game:Reset(piles)
    game:Add(0,PileType.Stock,64,160,ImVec2.new(0.15,-0.3))
    game:Add(1,PileType.Waste,180,160,ImVec2.new(16,0))
    local index=2
    for row=0,6 do
        for column=0,row do
            game:Add(index,PileType.Invisible,510-row*46+column*92,160+row*48)
            local card=deck:take_back(); card.faceUp=true; piles:get(index).cards:push_back(card)
            index=index+1
        end
    end
    game:Add(30,PileType.Foundation,900,160)
    piles:get(0).cards=deck
    local card=piles:get(0).cards:take_back(); card.faceUp=true; piles:get(1).cards:push_back(card)
end
function IsBlocked(piles,index)
    if index<2 or index>29 then return false end
    local offset,row,start=index-2,0,0
    while start+row<offset do start=start+row+1; row=row+1 end
    if row==6 then return false end
    return not piles:get(index+row+1).cards:empty() or not piles:get(index+row+2).cards:empty()
end
function CanPickup(piles,source,index)
    if source<1 or source>29 or IsBlocked(piles,source) then return false end
    local cards=piles:get(source).cards
    return index>=0 and index==cards:size()-1 and cards:get(index).faceUp
end
function CanDrop(piles,source,target,cards)
    if source==target or cards:size()~=1 or source<1 or source>29 or IsBlocked(piles,source) then return false end
    if target==30 then return cards:front().rank==13 end
    if target<1 or target>29 or IsBlocked(piles,target) then return false end
    local destination=piles:get(target).cards
    return not destination:empty() and destination:back().rank+cards:front().rank==13
end
function AfterMove(piles,source,target)
    if target~=30 then
        local cards=piles:get(target).cards
        local a,b=cards:take_back(),cards:take_back()
        piles:get(30).cards:push_back(b); piles:get(30).cards:push_back(a)
    end
    game:Record()
end
function HandleClick(piles,source)
    if source==0 then
        local stock,waste=piles:get(0).cards,piles:get(1).cards
        if not stock:empty() then
            local card=stock:take_back(); card.faceUp=true; waste:push_back(card); game.draws=game.draws+1
        elseif not waste:empty() and game.passes<2 then
            while not waste:empty() do local card=waste:take_back(); card.faceUp=false; stock:push_back(card) end
            game.passes=game.passes+1
        else game.notice="No stock redeals remain. Find an exposed pair or undo."; return end
        game:Record(); return
    end
    local cards=piles:get(source).cards
    if not CanPickup(piles,source,cards:size()-1) then game.notice="That card is still covered."; return end
    if cards:back().rank==13 then
        piles:get(30).cards:push_back(cards:take_back()); game:Record(); return
    end
    if game.selected==source then game.selected=-1; game.notice="Selection cleared."; return end
    if game.selected>=1 then
        local selected=piles:get(game.selected).cards
        if not selected:empty() and not IsBlocked(piles,game.selected) and
            selected:back().rank+cards:back().rank==13 then
            piles:get(30).cards:push_back(selected:take_back())
            piles:get(30).cards:push_back(cards:take_back()); game:Record(); return
        end
    end
    game.selected=source
    game.notice="Selected "..game:CardName(cards:back())..". Choose an exposed "..(13-cards:back().rank).."."
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
