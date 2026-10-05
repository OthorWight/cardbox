GameName = "Hanoi"

-- Amber window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {28, 22, 20}, BackgroundBottom = {20, 15, 14},
    Toolbar = {39, 30, 26}, ButtonRounding = 6,
    EmptyPile = {39, 30, 26, 180}, EmptyPileBorder = {105, 81, 62},
    EmptyPileText = {189, 167, 143}, CardBack = {70, 49, 36},
    CardBorder = {105, 81, 62}, CardHover = {239, 178, 111},
    Colors = {
        Text = {244, 235, 219}, TextDisabled = {189, 167, 143},
        WindowBg = {39, 30, 26}, ChildBg = {39, 30, 26},
        PopupBg = {52, 40, 33}, MenuBarBg = {39, 30, 26},
        Border = {105, 81, 62}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {78, 56, 37}, FrameBgHovered = {101, 74, 48},
        FrameBgActive = {123, 94, 64}, Button = {78, 56, 37},
        ButtonHovered = {101, 74, 48}, ButtonActive = {123, 94, 64},
        Header = {78, 56, 37}, HeaderHovered = {101, 74, 48},
        HeaderActive = {123, 94, 64}, TitleBg = {39, 30, 26},
        TitleBgActive = {52, 40, 33}, TitleBgCollapsed = {39, 30, 26},
        Separator = {105, 81, 62}, SeparatorHovered = {239, 178, 111},
        SeparatorActive = {239, 178, 111}, ScrollbarBg = {28, 22, 20},
        ScrollbarGrab = {105, 81, 62}, ScrollbarGrabHovered = {101, 74, 48},
        ScrollbarGrabActive = {123, 94, 64}, CheckMark = {239, 178, 111},
        SliderGrab = {239, 178, 111}, SliderGrabActive = {239, 178, 111},
        TextSelectedBg = {239, 178, 111, 65}, NavCursor = {239, 178, 111},
        ModalWindowDimBg = {20, 15, 14, 190}
    }
}

NumDecks = 1
AutoCenter = false
CardSize = ImVec2.new(94, 132)
HelpText = [[Move every disk to Peg 3. Only the top disk may move, and a smaller rank must sit on a larger rank. Drag a disk, or click its peg and then a destination. Choose 3–7 disks with the difficulty controls; changing difficulty resets this puzzle. Moves and difficulty survive undo/redo. The optimal solution takes 2^disks - 1 moves.]]
local disks, moves, selected, notice, PilesRef = 5, 0, -1, "", nil
local function Layout(piles)
    for i=0,2 do piles:get(i).offset=ImVec2.new(0,24) end
end
local function Setup(piles,deck)
    piles:clear(); PilesRef=piles; moves,selected,notice=0,-1,""; SetScore(0)
    for i=0,2 do
        local p=Pile.new(); p.id,p.type=i,PileType.Tableau
        p.pos,p.size=ImVec2.new(200+i*300,250),CardSize
        p.offset=ImVec2.new(0,28); piles:push_back(p)
    end
    -- Disk ranks form a puzzle; remaining playing cards are deliberately hidden.
    for rank=disks,1,-1 do
        local card=deck:take_back(); card.rank,card.suit,card.faceUp=rank,Suit.Spades,true
        piles:get(0).cards:push_back(card)
    end
    local hidden=Pile.new(); hidden.id,hidden.type=3,PileType.Invisible
    hidden.pos,hidden.size,hidden.offset=ImVec2.new(-1000,-1000),CardSize,ImVec2.new(0,0)
    hidden.cards=deck; piles:push_back(hidden); Layout(piles)
end
function Init(piles,deck) Setup(piles,deck) end
function CanPickup(piles,source,index)
    return source>=0 and source<=2 and index>=0 and index==piles:get(source).cards:size()-1
end
function CanDrop(piles,source,target,cards)
    if source<0 or source>2 or target<0 or target>2 or source==target or cards:size()~=1 then return false end
    local destination=piles:get(target).cards
    return destination:empty() or cards:front().rank<destination:back().rank
end
function AfterMove() moves=moves+1; selected,notice=-1,""; SetScore(moves) end
function HandleClick(piles,target)
    if target<0 or target>2 then return end
    if selected>=0 and selected~=target then
        local cards=piles:get(selected).cards
        local stack=VectorCard.new(); if not cards:empty() then stack:push_back(cards:back()) end
        if CanDrop(piles,selected,target,stack) then
            piles:get(target).cards:push_back(cards:take_back()); AfterMove(); return
        end
        notice="A larger disk cannot cover a smaller disk."
    end
    selected=piles:get(target).cards:empty() and -1 or target
end
function HandleAction(piles,action)
    if action=="easier" or action=="harder" or action=="reset" then
        disks=math.max(3,math.min(7,disks+(action=="easier" and -1 or action=="harder" and 1 or 0)))
        local deck=VectorCard.new()
        for i=0,piles:size()-1 do
            local cards=piles:get(i).cards
            while not cards:empty() do deck:push_back(cards:take_back()) end
        end
        Setup(piles,deck)
    end
end
function AutoSolve(piles) PilesRef=piles; return {} end
function IsWon(piles) return piles:get(2).cards:size()==disks end
function SaveState() return disks..","..moves end
function LoadState(piles,data)
    local d,m=string.match(data,"^(%d+),(%d+)$"); assert(d and m,"Invalid Hanoi snapshot")
    disks,moves=tonumber(d),tonumber(m); assert(disks>=3 and disks<=7)
    PilesRef,selected,notice=piles,-1,""; Layout(piles)
end
local function Text(x,y,text,color,size,width)
    color=color or Theme.Colors.Text
    DrawBoardText(x,y,text,size or 20,width or 0,color[1],color[2],color[3])
end
local function Button(x,y,w,label,action,enabled)
    if DrawBoardButton(x,y,w,36,label,enabled) then PerformAction(action) end
end
function DrawBackground()
    local c=Theme.Toolbar; DrawBoardPanel(40,48,1200,610,c[1],c[2],c[3])
    c=Theme.Colors.PopupBg; DrawBoardPanel(64,545,1152,92,c[1],c[2],c[3])
end
function Draw()
    if not PilesRef then return end
    local won=IsWon(PilesRef)
    Text(72,70,"TOWER OF HANOI",Theme.CardHover,32)
    Text(72,120,"One disk at a time. Smaller ranks sit on larger ranks.",Theme.EmptyPileText,18)
    Text(72,164,disks.." disks   |   Moves: "..moves.."   |   Optimal: "..math.floor(2^disks-1),nil,20)
    Button(780,76,120,"Fewer disks","easier",disks>3 and not won)
    Button(912,76,120,"More disks","harder",disks<7 and not won)
    Button(1044,76,160,"Reset puzzle","reset",not won)
    for i=0,2 do
        Button(180+i*300,200,134,"Peg "..(i+1)..(selected==i and " selected" or ""),"peg:"..i,not won)
    end
    Text(86,562,won and (moves==2^disks-1 and "Perfect solution!" or "Puzzle solved. Try reaching the optimal move count.") or
        notice~="" and notice or "Drag the top disk, or select its peg and then a destination.",nil,18,750)
    Text(86,597,"Ctrl+Z / Ctrl+Y: undo / redo     F2: new puzzle",Theme.EmptyPileText,16)
    Button(908,575,90,"Undo","engine:undo",CanUndo())
    Button(1010,575,90,"Redo","engine:redo",CanRedo())
    Button(1110,575,90,"New","engine:restart",true)
end
local Action=HandleAction
function HandleAction(piles,action)
    local peg=string.match(action,"^peg:([0-2])$")
    if peg then HandleClick(piles,tonumber(peg)) else Action(piles,action) end
end
