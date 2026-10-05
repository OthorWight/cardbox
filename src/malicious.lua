GameName = "Security Sandbox Test"

-- Crimson window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {28, 19, 23}, BackgroundBottom = {19, 13, 16},
    Toolbar = {40, 27, 33}, ButtonRounding = 6,
    EmptyPile = {40, 27, 33, 180}, EmptyPileBorder = {105, 65, 78},
    EmptyPileText = {194, 155, 164}, CardBack = {70, 35, 46},
    CardBorder = {105, 65, 78}, CardHover = {240, 143, 151},
    Colors = {
        Text = {246, 230, 232}, TextDisabled = {194, 155, 164},
        WindowBg = {40, 27, 33}, ChildBg = {40, 27, 33},
        PopupBg = {53, 35, 42}, MenuBarBg = {40, 27, 33},
        Border = {105, 65, 78}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {78, 42, 54}, FrameBgHovered = {102, 58, 71},
        FrameBgActive = {127, 77, 89}, Button = {78, 42, 54},
        ButtonHovered = {102, 58, 71}, ButtonActive = {127, 77, 89},
        Header = {78, 42, 54}, HeaderHovered = {102, 58, 71},
        HeaderActive = {127, 77, 89}, TitleBg = {40, 27, 33},
        TitleBgActive = {53, 35, 42}, TitleBgCollapsed = {40, 27, 33},
        Separator = {105, 65, 78}, SeparatorHovered = {240, 143, 151},
        SeparatorActive = {240, 143, 151}, ScrollbarBg = {28, 19, 23},
        ScrollbarGrab = {105, 65, 78}, ScrollbarGrabHovered = {102, 58, 71},
        ScrollbarGrabActive = {127, 77, 89}, CheckMark = {240, 143, 151},
        SliderGrab = {240, 143, 151}, SliderGrabActive = {240, 143, 151},
        TextSelectedBg = {240, 143, 151, 65}, NavCursor = {240, 143, 151},
        ModalWindowDimBg = {19, 13, 16, 190}
    }
}

NumDecks = 1
AutoCenter = false
HelpText = [[A sandbox diagnostic, not a card game. Run each probe individually. A blocked probe is a pass; an unexpectedly available capability is a failure. Resource probes deliberately exercise the engine's memory and instruction limits. Results are undoable, and Reset clears the report. Probes run as protected actions after rendering.]]
local results,status,last={},"Choose a probe. A blocked capability is a pass.",0
local probes={
    {"OS commands",function() return os.execute("echo Hacked") end},
    {"File access",function() return io.open("hacked.txt","w") end},
    {"Dynamic libraries",function() return require("package") end},
    {"Memory limit",function() local s="spam"; while true do s=s..s end end},
    {"Instruction limit",function() while true do end end},
    {"Debug registry",function() return debug.getregistry() end},
    {"Dynamic code",function() return (loadstring or load)("return 1")() end},
    {"Global environment",function() return (_G or getfenv(0)).DrawBoardButton end},
    {"Metatable access",function() return getmetatable("").__index end},
    {"Recursion limit",function() local function recurse() return recurse()+1 end; return recurse() end},
    {"Coroutines",function() return coroutine.create(function() while true do end end) end},
    {"Binary bytecode",function() return (loadstring or load)("\27Lua")() end},
    {"Process exit",function() return os.exit(1) end},
    {"Large allocation",function() return #string.rep("HACK",1024*1024*256) end}
}
function Init(piles) piles:clear(); results,status,last={},"Choose a probe. A blocked capability is a pass.",0; SetScore(0) end
function CanPickup() return false end
function CanDrop() return false end
function AfterMove() end
function HandleClick() end
function AutoSolve() return {} end
function IsWon() return false end
function HandleAction(piles,action)
    if action=="reset" then Init(piles); return end
    local index=tonumber(string.match(action,"^probe:(%d+)$"))
    if not index or not probes[index] then return end
    local ok,err=pcall(probes[index][2])
    results[index]=ok and "FAIL" or "PASS"; last=index
    status=probes[index][1]..": "..results[index].."\n"..(ok and "Capability was unexpectedly available." or tostring(err))
    local count=0; for _,value in pairs(results) do if value=="PASS" then count=count+1 end end; SetScore(count)
end
function SaveState()
    local fields={last}
    for i=1,#probes do fields[#fields+1]=results[i] or "-" end
    return table.concat(fields,",").."\n"..status
end
function LoadState(piles,data)
    local values,message=string.match(data,"^([^\n]+)\n(.*)$"); assert(values and message)
    local fields={}; for token in string.gmatch(values,"[^,]+") do fields[#fields+1]=token end
    assert(#fields==15); results,last,status={},tonumber(fields[1]),message
    for i=1,#probes do if fields[i+1]~="-" then results[i]=fields[i+1] end end
end
local function Text(x,y,text,color,size,width)
    color=color or Theme.Colors.Text
    DrawBoardText(x,y,text,size or 18,width or 0,color[1],color[2],color[3])
end
local function Button(x,y,w,label,action,enabled)
    if DrawBoardButton(x,y,w,36,label,enabled) then PerformAction(action) end
end
function DrawBackground()
    local c=Theme.Toolbar; DrawBoardPanel(40,48,730,610,c[1],c[2],c[3])
    c=Theme.Colors.PopupBg; DrawBoardPanel(786,48,454,610,c[1],c[2],c[3])
end
function Draw()
    Text(64,70,"SECURITY SANDBOX",Theme.CardHover,30)
    Text(64,115,"Protected probes for the Lua runtime.",Theme.EmptyPileText,18)
    for i,probe in ipairs(probes) do
        local column,row=math.floor((i-1)/7),(i-1)%7
        local label=i..". "..probe[1]
        Button(64+column*350,163+row*60,242,label,"probe:"..i,true)
        Text(316+column*350,171+row*60,results[i] or "READY",results[i]=="PASS" and {114,210,166} or Theme.CardHover,16)
    end
    Text(812,78,"REPORT",Theme.CardHover,22)
    Text(812,123,"Blocked probes: "..GetScore().." / "..#probes,Theme.EmptyPileText,19)
    Text(812,170,status,nil,16,400)
    Button(812,482,188,"Clear report","reset",true)
    Button(812,534,90,"Undo","engine:undo",CanUndo())
    Button(910,534,90,"Redo","engine:redo",CanRedo())
    Text(812,594,"Each probe runs individually.\nFailures remain visible in the report.",Theme.EmptyPileText,15,400)
end
