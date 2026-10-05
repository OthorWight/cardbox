GameName = "Yukon"

-- Glacier window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {19, 25, 34}, BackgroundBottom = {12, 18, 26},
    Toolbar = {25, 34, 46}, ButtonRounding = 6,
    EmptyPile = {25, 34, 46, 180}, EmptyPileBorder = {72, 89, 110},
    EmptyPileText = {159, 176, 196}, CardBack = {43, 64, 89},
    CardBorder = {72, 89, 110}, CardHover = {164, 204, 239},
    Colors = {
        Text = {232, 239, 246}, TextDisabled = {159, 176, 196},
        WindowBg = {25, 34, 46}, ChildBg = {25, 34, 46},
        PopupBg = {34, 45, 60}, MenuBarBg = {25, 34, 46},
        Border = {72, 89, 110}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {43, 60, 79}, FrameBgHovered = {58, 78, 99},
        FrameBgActive = {74, 99, 121}, Button = {43, 60, 79},
        ButtonHovered = {58, 78, 99}, ButtonActive = {74, 99, 121},
        Header = {43, 60, 79}, HeaderHovered = {58, 78, 99},
        HeaderActive = {74, 99, 121}, TitleBg = {25, 34, 46},
        TitleBgActive = {34, 45, 60}, TitleBgCollapsed = {25, 34, 46},
        Separator = {72, 89, 110}, SeparatorHovered = {164, 204, 239},
        SeparatorActive = {164, 204, 239}, ScrollbarBg = {19, 25, 34},
        ScrollbarGrab = {72, 89, 110}, ScrollbarGrabHovered = {58, 78, 99},
        ScrollbarGrabActive = {74, 99, 121}, CheckMark = {164, 204, 239},
        SliderGrab = {164, 204, 239}, SliderGrabActive = {164, 204, 239},
        TextSelectedBg = {164, 204, 239, 65}, NavCursor = {164, 204, 239},
        ModalWindowDimBg = {12, 18, 26, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(90, 126)
HelpText = 'Move any face-up group without requiring it to be ordered. Its bottom card must fit the next higher rank of the opposite color. Empty columns accept only Kings. Revealed cards turn face up. Foundations build up by suit. Hint and Safe foundations are explicit controls. Ctrl+Z/Ctrl+Y undo/redo; F2 restarts.'
local game = Solitaire.new({kind="yukon", first=4, last=10, foundationFirst=0, foundationLast=3, goal=52,
    progress="Foundation cards", mainLabel="Send safe cards", description="Move any face-up group, even if its cards are not in sequence.",
    tip="Place the group's first card on the next higher rank of the opposite color. Empty columns need Kings."})

function Init(piles, deck)
    game:Reset(piles)
    for i=0,3 do game:Add(i,PileType.Foundation,490+i*116,160) end
    for i=0,6 do
        game:Add(4+i,PileType.Tableau,64+i*128,310)
        for j=1,i+(i==0 and 1 or 5) do
            local card=deck:take_back(); card.faceUp=j>i
            piles:get(4+i).cards:push_back(card)
        end
    end
    game:Layout()
end
function CanPickup(piles,source,index)
    local cards=piles:get(source).cards
    if index<0 or index>=cards:size() or not cards:get(index).faceUp then return false end
    return source>=4 or index==cards:size()-1
end
function CanDrop(piles,source,target,cards)
    if source==target or cards:empty() then return false end
    local destination,card=piles:get(target).cards,cards:front()
    if target<4 then
        if cards:size()~=1 then return false end
        if destination:empty() then return card.rank==1 end
        local top=destination:back()
        return top.suit==card.suit and top.rank+1==card.rank
    end
    if destination:empty() then return card.rank==13 end
    local top=destination:back()
    return top.faceUp and top:IsRed()~=card:IsRed() and top.rank==card.rank+1
end
function AfterMove(piles,source)
    local cards=piles:get(source).cards
    if source>=4 and not cards:empty() then cards:back().faceUp=true end
    game:Record()
end
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
