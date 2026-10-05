GameName = "Hold'em"

-- Burgundy and gold window theme. Colors use RGB or RGBA byte channels.
Theme = {
    Background = {28, 18, 26}, BackgroundBottom = {18, 12, 18},
    Toolbar = {40, 25, 34}, ButtonRounding = 6,
    EmptyPile = {40, 25, 34, 180}, EmptyPileBorder = {103, 71, 81},
    EmptyPileText = {189, 160, 174}, CardBack = {68, 37, 51},
    CardBorder = {103, 71, 81}, CardHover = {238, 198, 126},
    Colors = {
        Text = {245, 233, 226}, TextDisabled = {189, 160, 174},
        WindowBg = {40, 25, 34}, ChildBg = {40, 25, 34},
        PopupBg = {53, 33, 44}, MenuBarBg = {40, 25, 34},
        Border = {103, 71, 81}, BorderShadow = {0, 0, 0, 0},
        FrameBg = {77, 45, 58}, FrameBgHovered = {99, 61, 75},
        FrameBgActive = {122, 80, 91}, Button = {77, 45, 58},
        ButtonHovered = {99, 61, 75}, ButtonActive = {122, 80, 91},
        Header = {77, 45, 58}, HeaderHovered = {99, 61, 75},
        HeaderActive = {122, 80, 91}, TitleBg = {40, 25, 34},
        TitleBgActive = {53, 33, 44}, TitleBgCollapsed = {40, 25, 34},
        Separator = {103, 71, 81}, SeparatorHovered = {238, 198, 126},
        SeparatorActive = {238, 198, 126}, ScrollbarBg = {28, 18, 26},
        ScrollbarGrab = {103, 71, 81}, ScrollbarGrabHovered = {99, 61, 75},
        ScrollbarGrabActive = {122, 80, 91}, CheckMark = {238, 198, 126},
        SliderGrab = {238, 198, 126}, SliderGrabActive = {238, 198, 126},
        TextSelectedBg = {238, 198, 126, 65}, NavCursor = {238, 198, 126},
        ModalWindowDimBg = {18, 12, 18, 190}
    }
}

function EvaluateHand(cards)
    local r_counts = {}; local s_counts = {}
    for i=2,14 do r_counts[i] = {} end
    for i=0,3 do s_counts[i] = {} end
        for c_idx=1, #cards do
        local c = cards[c_idx]
        local r = c.rank == 1 and 14 or c.rank
        table.insert(r_counts[r], c)
        table.insert(s_counts[c.suit], c)
    end

    local flush_cards = nil
    for s=0,3 do
        if #s_counts[s] >= 5 then
            flush_cards = s_counts[s]
            table.sort(flush_cards, function(a,b)
                local ar = a.rank==1 and 14 or a.rank
                local br = b.rank==1 and 14 or b.rank
                return ar > br
            end)
            break
        end
    end

    local function get_straight(c_list)
        local present = {}
        for _, c in ipairs(c_list) do
            local r = c.rank==1 and 14 or c.rank
            present[r] = true
            if r == 14 then present[1] = true end
        end
        local streak = 0; local high = 0
        for r=14,1,-1 do
            if present[r] then
                if streak == 0 then high = r end
                streak = streak + 1
                if streak >= 5 then return high end
            else
                streak = 0
            end
        end
        return nil
    end

    local str_high = get_straight(cards)
    local sf_high = flush_cards and get_straight(flush_cards) or nil
    if sf_high then return 8000000 + sf_high, "Straight Flush" end

    local pairs = {}; local trips = {}; local quads = nil
    for r=14,2,-1 do
        local n = #r_counts[r]
        if n == 4 then quads = r
        elseif n == 3 then table.insert(trips, r)
        elseif n == 2 then table.insert(pairs, r)
        end
    end

    local function get_kickers(exclude, count)
        local k = {}
        for r=14,2,-1 do
            local skip = false
            for _, ex in ipairs(exclude) do if r == ex then skip = true break end end
            if not skip then
                for i=1,#r_counts[r] do
                    table.insert(k, r)
                    if #k == count then return k end
                end
            end
        end
        while #k < count do table.insert(k, 0) end
        return k
    end

    if quads then
        local k = get_kickers({quads}, 1)
        return 7000000 + quads * 16 + k[1], "Four of a Kind"
    end
    if #trips >= 1 and #pairs >= 1 then return 6000000 + trips[1] * 16 + pairs[1], "Full House" end
    if #trips >= 2 then return 6000000 + trips[1] * 16 + trips[2], "Full House" end

    if flush_cards then
        local s = 5000000; local m = 65536
        for i=1,5 do
            local r = flush_cards[i].rank==1 and 14 or flush_cards[i].rank
            s = s + r * m
            m = m / 16
        end
        return s, "Flush"
    end

    if str_high then return 4000000 + str_high, "Straight" end

    if #trips >= 1 then
        local k = get_kickers({trips[1]}, 2)
        return 3000000 + trips[1] * 256 + k[1] * 16 + k[2], "Three of a Kind"
    end
    if #pairs >= 2 then
        local k = get_kickers({pairs[1], pairs[2]}, 1)
        return 2000000 + pairs[1] * 256 + pairs[2] * 16 + k[1], "Two Pair"
    end
    if #pairs >= 1 then
        local k = get_kickers({pairs[1]}, 3)
        return 1000000 + pairs[1] * 4096 + k[1] * 256 + k[2] * 16 + k[3], "Pair"
    end

    local k = get_kickers({}, 5)
    return k[1] * 65536 + k[2] * 4096 + k[3] * 256 + k[4] * 16 + k[5], "High Card"
end


NumDecks=1
AutoCenter=false
CardSize=ImVec2.new(76,106)
HelpText=[[An eight-seat no-limit Hold'em tournament with 1,000 play chips per seat.
Blinds are 10/20. Call, check, fold, or raise by at least the last full raise. Smaller all-in raises do not reopen betting for players who already acted. Side pots restrict winnings to each player's contribution; odd chips go clockwise from the dealer.
The dealer is the small blind in heads-up play. Bots use only their hole cards and the board. Pause or step bots to study a hand. Undo restores bets, chips, cards, and the bot random state; it pauses bots until you resume them. Next hand is an explicit action. F2 starts a new tournament.]]

Poker={}
local PilesRef,Paused,NextActionAt=nil,false,0
local Seats={{560,520},{170,470},{60,295},{170,135},{560,115},{950,135},{1110,295},{950,470}}
local function Clock() return GetTime() end
local function Random()
    Poker.rng=(Poker.rng*48271)%2147483647
    return Poker.rng/2147483647
end
local function Active()
    local players,able={},{}
    for i=1,8 do
        if not Poker.players[i].folded then
            players[#players+1]=i
            if Poker.players[i].chips>0 then able[#able+1]=i end
        end
    end
    return players,able
end
local function NextLive(start)
    for step=1,8 do
        local i=(start+step-1)%8+1
        if Poker.players[i].chips>0 then return i end
    end
end
local function AddPile(piles,id,kind,x,y,offset)
    local pile=Pile.new(); pile.id,pile.type=id,kind
    pile.pos,pile.size,pile.offset=ImVec2.new(x,y),CardSize,offset or ImVec2.new(0,0)
    piles:push_back(pile)
end
function Init(piles,deck)
    Poker={phase="Ready",dealer=8,turn=1,pot=0,bet=0,minRaise=20,raise=20,hand=0,sb=0,bb=0,lastPot=0,
        rng=math.random(1,2147483646),log="Start a tournament. Eight players, 8,000 chips in play.",players={}}
    PilesRef,Paused,NextActionAt=piles,false,0
    for i=1,8 do Poker.players[i]={chips=1000,bet=0,total=0,folded=false,acted=false,actedAt=0,payout=0} end
    AddPile(piles,0,PileType.Stock,1160,130,ImVec2.new(0.1,-0.2))
    AddPile(piles,1,PileType.Invisible,-1000,-1000)
    for i=0,4 do AddPile(piles,2+i,PileType.Tableau,430+i*84,285) end
    for i=1,8 do AddPile(piles,6+i,PileType.Tableau,Seats[i][1],Seats[i][2],ImVec2.new(42,0)) end
    piles:get(0).cards=deck
    SetScore(1000)
end
local function Deal(piles,target,faceUp)
    assert(not piles:get(0).cards:empty(),"Poker deck exhausted")
    local card=piles:get(0).cards:take_back(); card.faceUp=faceUp
    piles:get(target).cards:push_back(card)
end
local function Bet(index,amount)
    local player=Poker.players[index]
    amount=math.min(player.chips,math.max(0,math.floor(amount)))
    player.chips,player.bet,player.total=player.chips-amount,player.bet+amount,player.total+amount
    Poker.pot=Poker.pot+amount
    return amount
end
local function NeedsAction(index)
    local p=Poker.players[index]
    if p.folded or p.chips==0 then return false end
    local _,able=Active()
    return p.bet<Poker.bet or (#able>1 and not p.acted)
end
local function RoundOver()
    for i=1,8 do if NeedsAction(i) then return false end end
    return true
end
local function ChooseTurn(start)
    for step=0,7 do
        local i=(start+step-1)%8+1
        if NeedsAction(i) then Poker.turn=i; NextActionAt=Clock()+0.6; return end
    end
    Poker.turn=0
end
local function StartRound(piles,phase)
    Poker.phase,Poker.bet,Poker.minRaise,Poker.raise=phase,0,20,20
    for i=1,8 do
        local p=Poker.players[i]; p.bet,p.acted,p.actedAt=0,false,0
    end
    local first=Poker.dealer%8+1
    if phase=="PreFlop" then
        local active=Active()
        Poker.sb=#active==2 and Poker.dealer or NextLive(Poker.dealer)
        Poker.bb=NextLive(Poker.sb)
        Bet(Poker.sb,10); Bet(Poker.bb,20)
        Poker.bet=20
        local _,able=Active()
        if #able<2 then
            -- With no opponent able to raise, only the actual blind wager
            -- needs matching; a short blind does not force a refundable bet.
            Poker.bet=0
            for i=1,8 do
                if not Poker.players[i].folded then Poker.bet=math.max(Poker.bet,Poker.players[i].bet) end
            end
        end
        first=(Poker.bb%8)+1
    end
    ChooseTurn(first)
end
function PokerStartHand(piles)
    local live=0
    for i=1,8 do if Poker.players[i].chips>0 then live=live+1 end end
    if live<2 or Poker.players[1].chips==0 then return end
    for i=1,14 do
        local cards=piles:get(i).cards
        while not cards:empty() do local card=cards:take_back(); card.faceUp=false; piles:get(0).cards:push_back(card) end
    end
    local deck=piles:get(0).cards
    for i=deck:size()-1,1,-1 do
        local j=math.floor(Random()*(i+1))
        local a,b=deck:get(i),deck:get(j)
        a.rank,b.rank=b.rank,a.rank; a.suit,b.suit=b.suit,a.suit
        a.faceUp,b.faceUp=false,false
    end
    Poker.dealer=NextLive(Poker.dealer)
    Poker.hand,Poker.pot,Poker.lastPot=Poker.hand+1,0,0
    for i=1,8 do
        local p=Poker.players[i]
        p.bet,p.total,p.payout,p.acted,p.actedAt,p.folded=0,0,0,false,0,p.chips==0
    end
    for round=1,2 do
        for offset=1,8 do
            local seat=(Poker.dealer+offset-1)%8+1
            if not Poker.players[seat].folded then Deal(piles,6+seat,seat==1) end
        end
    end
    Poker.log="Hand "..Poker.hand..". Blinds 10 / 20."
    Paused=false
    StartRound(piles,"PreFlop")
end
local function Hand(piles,index)
    local hand={}
    for i=2,6 do
        local cards=piles:get(i).cards
        if not cards:empty() then hand[#hand+1]=cards:back() end
    end
    local cards=piles:get(6+index).cards
    for i=0,cards:size()-1 do hand[#hand+1]=cards:get(i) end
    return hand
end
function PokerSettle(piles)
    if Poker.phase=="Showdown" or Poker.phase=="Finished" or Poker.phase=="Eliminated" then return end
    local levels,seen,scores={},{},{}
    for i=1,8 do
        local p=Poker.players[i]; p.payout=0
        if p.total>0 and not seen[p.total] then levels[#levels+1]=p.total; seen[p.total]=true end
        if not p.folded then
            scores[i]=EvaluateHand(Hand(piles,i))
            local cards=piles:get(6+i).cards
            for j=0,cards:size()-1 do cards:get(j).faceUp=true end
        end
    end
    table.sort(levels)
    local previous,awarded=0,0
    for _,level in ipairs(levels) do
        local contributors,eligible={},{}
        for i=1,8 do
            if Poker.players[i].total>=level then
                contributors[#contributors+1]=i
                if not Poker.players[i].folded then eligible[#eligible+1]=i end
            end
        end
        local amount=(level-previous)*#contributors
        local winners,best={},-1
        if #contributors==1 then winners=contributors -- unmatched chips are returned, even to a folded seat
        elseif #eligible==0 then
            -- A defensive refund for an inconsistent externally supplied state.
            for _,i in ipairs(contributors) do
                Poker.players[i].payout=Poker.players[i].payout+(level-previous)
            end
        else
            for _,i in ipairs(eligible) do
                if scores[i]>best then best,winners=scores[i],{i}
                elseif scores[i]==best then winners[#winners+1]=i end
            end
        end
        if #winners>0 then
            table.sort(winners,function(a,b) return (a-Poker.dealer-1)%8<(b-Poker.dealer-1)%8 end)
            local share,extra=math.floor(amount/#winners),amount%#winners
            for n,i in ipairs(winners) do Poker.players[i].payout=Poker.players[i].payout+share+(n<=extra and 1 or 0) end
        end
        awarded,previous=awarded+amount,level
    end
    assert(awarded==Poker.pot,"Poker contributions do not match the pot")
    local results={}
    for i=1,8 do
        local p=Poker.players[i]; p.chips=p.chips+p.payout
        if p.payout>0 then results[#results+1]=(i==1 and "You" or "P"..i).." +"..p.payout end
    end
    Poker.lastPot,Poker.pot,Poker.turn=Poker.pot,0,0
    Poker.phase="Showdown"
    Poker.log="Hand settled: "..table.concat(results,", ").."."
    local funded=0
    for i=1,8 do if Poker.players[i].chips>0 then funded=funded+1 end end
    if Poker.players[1].chips==0 then Poker.phase="Eliminated"; Poker.log=Poker.log.." You are out. Start a new tournament."
    elseif funded==1 then Poker.phase="Finished"; Poker.log="You won the tournament! All 8,000 chips are yours." end
    SetScore(Poker.players[1].chips)
end
local function Advance(piles)
    local active=Active()
    if #active==1 or Poker.phase=="River" then PokerSettle(piles); return end
    Deal(piles,1,false)
    if Poker.phase=="PreFlop" then
        for i=2,4 do Deal(piles,i,true) end
        StartRound(piles,"Flop")
    elseif Poker.phase=="Flop" then Deal(piles,5,true); StartRound(piles,"Turn")
    elseif Poker.phase=="Turn" then Deal(piles,6,true); StartRound(piles,"River") end
end
local function CanRaise(index)
    local p=Poker.players[index]
    local _,able=Active()
    return #able>1 and p.chips>math.max(0,Poker.bet-p.bet) and
        (not p.acted or Poker.bet-p.actedAt>=Poker.minRaise)
end
local function Act(piles,index,action,raise)
    if Poker.turn~=index or not NeedsAction(index) then return false end
    local p=Poker.players[index]
    local call=math.max(0,Poker.bet-p.bet)
    if action=="fold" then
        p.folded=true
        local cards=piles:get(6+index).cards
        while not cards:empty() do piles:get(1).cards:push_back(cards:take_back()) end
        Poker.log=(index==1 and "You" or "P"..index).." folded."
    elseif action=="call" then
        local paid=Bet(index,call)
        Poker.log=(index==1 and "You" or "P"..index)..(call==0 and " checked." or " called "..paid..".")
    elseif action=="raise" then
        if not CanRaise(index) then return false end
        local remaining=p.chips-call
        raise=math.min(math.max(1,math.floor(raise or Poker.raise)),remaining)
        if raise<Poker.minRaise and raise<remaining then return false end
        local previous=Poker.bet
        Bet(index,call+raise); Poker.bet=p.bet
        local increment=Poker.bet-previous
        if increment>=Poker.minRaise then
            Poker.minRaise=increment
            for i=1,8 do if i~=index then Poker.players[i].acted=false end end
        end
        Poker.log=(index==1 and "You" or "P"..index).." raised to "..Poker.bet.."."
    else return false end
    p.acted,p.actedAt=true,Poker.bet
    Poker.raise=Poker.minRaise
    local active=Active()
    if #active==1 then PokerSettle(piles) else ChooseTurn(index%8+1) end
    SetScore(Poker.players[1].chips)
    return true
end
local function Bot(piles,index)
    local p=Poker.players[index]
    local score=EvaluateHand(Hand(piles,index))
    local call=math.max(0,Poker.bet-p.bet)
    local strength=score>=2000000 and 0.85 or score>=1000000 and 0.55 or 0.25
    strength=strength+(Random()-0.5)*0.3
    local odds=call/math.max(1,Poker.pot+call)
    if call>0 and strength<odds and p.chips>Poker.pot*0.2 then Act(piles,index,"fold")
    elseif strength>0.65 and CanRaise(index) and Random()<0.5 then
        Act(piles,index,"raise",math.max(Poker.minRaise,math.floor(Poker.pot/2)))
    else Act(piles,index,"call") end
end
function HandleAction(piles,action)
    PilesRef=piles
    if action=="start" and (Poker.phase=="Ready" or Poker.phase=="Showdown") then PokerStartHand(piles)
    elseif action=="pause" then Paused=not Paused; NextActionAt=Clock()+0.6
    elseif action=="tick" then
        if Poker.phase=="Ready" or Poker.phase=="Showdown" or Poker.phase=="Finished" or Poker.phase=="Eliminated" then return end
        if RoundOver() then Advance(piles)
        elseif Poker.turn~=1 and Poker.turn>0 then Bot(piles,Poker.turn) end
        NextActionAt=Clock()+0.6
    elseif action=="fold" or action=="call" or action=="raise" then Act(piles,1,action,Poker.raise)
    elseif action=="raise-min" then Poker.raise=Poker.minRaise
    elseif action=="raise-half" then Poker.raise=math.max(Poker.minRaise,math.floor(Poker.pot/2))
    elseif action=="raise-max" then Poker.raise=math.max(0,Poker.players[1].chips-math.max(0,Poker.bet-Poker.players[1].bet))
    elseif action=="raise-less" then Poker.raise=math.max(Poker.minRaise,Poker.raise-10)
    elseif action=="raise-more" then Poker.raise=Poker.raise+10 end
end
function AutoSolve(piles)
    PilesRef=piles
    return {}
end
function CanPickup() return false end
function CanDrop() return false end
function AfterMove() end
function HandleClick() end
function IsWon() return Poker.phase=="Finished" end
local scalar={"phase","dealer","turn","pot","bet","minRaise","raise","hand","sb","bb","lastPot","rng"}
local playerFields={"chips","bet","total","folded","acted","actedAt","payout"}
function SaveState()
    local parts={}
    for _,key in ipairs(scalar) do parts[#parts+1]=Poker[key] end
    for i=1,8 do
        for _,key in ipairs(playerFields) do
            local value=Poker.players[i][key]
            parts[#parts+1]=type(value)=="boolean" and (value and 1 or 0) or value
        end
    end
    return table.concat(parts,",").."\n"..Poker.log
end
function LoadState(piles,data)
    local header,log=string.match(data,"^([^\n]+)\n(.*)$")
    assert(header,"Invalid poker snapshot")
    local parts={}; for value in string.gmatch(header,"[^,]+") do parts[#parts+1]=value end
    assert(#parts==#scalar+8*#playerFields,"Incomplete poker snapshot")
    local state,index={players={},log=log},1
    for _,key in ipairs(scalar) do
        state[key]=key=="phase" and parts[index] or tonumber(parts[index])
        assert(state[key]~=nil,"Invalid poker field"); index=index+1
    end
    for i=1,8 do
        state.players[i]={}
        for _,key in ipairs(playerFields) do
            local value=assert(tonumber(parts[index]),"Invalid poker player field")
            state.players[i][key]=(key=="folded" or key=="acted") and value==1 or
                (key~="folded" and key~="acted" and value or false)
            index=index+1
        end
    end
    Poker,PilesRef,Paused,NextActionAt=state,piles,true,Clock()+0.6
end
local function Text(x,y,text,color,size,width)
    color=color or Theme.Colors.Text
    DrawBoardText(x,y,text,size or 17,width or 0,color[1],color[2],color[3])
end
local function Button(x,y,w,label,action,enabled)
    if DrawBoardButton(x,y,w,36,label,enabled) then PerformAction(action) end
end
function DrawBackground()
    local c=Theme.Toolbar; DrawBoardPanel(40,48,1200,636,c[1],c[2],c[3])
    c=Theme.Colors.PopupBg; DrawBoardPanel(415,240,438,164,c[1],c[2],c[3])
end
function Draw()
    if not PilesRef then return end
    Text(62,66,"HOLD'EM",Theme.CardHover,30)
    Text(255,78,"Hand "..Poker.hand.." / "..Poker.phase,Theme.EmptyPileText,18)
    Button(900,66,90,"Undo","engine:undo",CanUndo())
    Button(998,66,90,"Redo","engine:redo",CanRedo())
    Button(1096,66,122,"New match","engine:restart",true)
    Text(430,255,"COMMUNITY",Theme.EmptyPileText,16)
    Text(695,250,"Pot: "..Poker.pot,Theme.CardHover,24)
    Text(200,310,Poker.log,nil,14,195)
    Text(875,315,Paused and "Bots paused" or "Bots playing",Theme.EmptyPileText,16)
    Button(875,347,185,Paused and "Resume bots" or "Pause bots","pause",true)
    Button(875,390,185,"Step / reveal","tick",Paused and Poker.phase~="Ready" and Poker.phase~="Showdown")
    for i=1,8 do
        local p=Poker.players[i]
        local role=i==Poker.dealer and " D" or i==Poker.sb and " SB" or i==Poker.bb and " BB" or ""
        local label=(i==1 and "YOU" or "P"..i)..role.." / "..p.chips
        if p.folded then label=label..(p.chips==0 and " out" or " folded") elseif p.chips==0 then label=label.." all-in" end
        if p.bet>0 then label=label.."\nBet "..p.bet end
        if p.payout>0 then label=label.."\nWon "..p.payout end
        Text(i==5 and 425 or Seats[i][1],i==5 and 163 or Seats[i][2]+112,label,
            p.folded and Theme.EmptyPileText or i==Poker.turn and Theme.CardHover or nil,14,i==5 and 124 or 175)
    end
    if Poker.phase=="Ready" or Poker.phase=="Showdown" then
        Button(490,440,300,Poker.phase=="Ready" and "Start tournament" or "Next hand","start",true)
    elseif Poker.phase=="Eliminated" or Poker.phase=="Finished" then
        Button(490,440,300,"Play a new tournament","engine:restart",true)
    elseif Poker.turn==1 and NeedsAction(1) then
        local p=Poker.players[1]
        local call=math.max(0,Poker.bet-p.bet)
        Button(440,425,116,"Fold","fold",true)
        Button(568,425,128,call==0 and "Check" or call>=p.chips and "All-in "..p.chips or "Call "..call,"call",true)
        local amount=math.min(Poker.raise,math.max(0,p.chips-call))
        local canRaise=CanRaise(1)
        Button(708,425,150,not canRaise and "Raise" or amount==p.chips-call and "All-in "..p.chips or "Raise +"..amount,"raise",canRaise)
        Button(440,475,88,"Min","raise-min",CanRaise(1))
        Button(536,475,38,"-","raise-less",CanRaise(1))
        Button(582,475,38,"+","raise-more",CanRaise(1))
        Button(628,475,108,"Half pot","raise-half",CanRaise(1))
        Button(744,475,114,"All-in","raise-max",CanRaise(1))
    else Text(455,438,"Waiting for the next action...",Theme.EmptyPileText,18,350) end
    -- Queue automation after user controls so a click to pause, undo, or restart
    -- takes priority over a bot action due in the same frame.
    if not Paused and Poker.phase~="Ready" and Poker.phase~="Showdown" and Poker.phase~="Finished" and Poker.phase~="Eliminated" and
        Clock()>=NextActionAt and (RoundOver() or Poker.turn~=1) then PerformAction("tick") end
end
