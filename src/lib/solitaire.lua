-- Shared controls and state for the solitaire games. Loaded by the engine before
-- a game script; rule decisions remain in that script's callbacks.
Solitaire = {}

function Solitaire.new(config)
    local game = {config = config, moves = 0, draws = 0, passes = 0, notice = "", selected = -1}
    for name, method in pairs(Solitaire) do
        if type(method) == "function" then game[name] = method end
    end
    return game
end

function Solitaire:Reset(piles)
    self.piles, self.moves, self.draws, self.passes = piles, 0, 0, 0
    self.notice, self.selected = "", -1
    piles:clear()
    SetScore(0)
end

function Solitaire:Add(id, kind, x, y, offset)
    local pile = Pile.new()
    pile.id, pile.type = id, kind
    pile.pos, pile.size = ImVec2.new(x, y), ImVec2.new(CardSize.x, CardSize.y)
    pile.offset = offset or ImVec2.new(0, 0)
    self.piles:push_back(pile)
end

function Solitaire:Layout()
    if not self.piles then return end
    for i = self.config.first, self.config.last do
        local pile = self.piles:get(i)
        pile.offset.y = math.min(self.config.spacing or 26,
            (642 - pile.pos.y - pile.size.y) / math.max(1, pile.cards:size() - 1))
    end
end

function Solitaire:Progress()
    if self.config.kind == "golf" or self.config.kind == "pyramid" then
        local remaining = 0
        for i = self.config.first, self.config.last do remaining = remaining + self.piles:get(i).cards:size() end
        return self.config.goal - remaining
    end
    local count = 0
    for i = self.config.foundationFirst, self.config.foundationLast do count = count + self.piles:get(i).cards:size() end
    return self.config.kind == "spider" and count // 13 or count
end

function Solitaire:Record()
    self.moves, self.notice, self.selected = self.moves + 1, "", -1
    self:Layout()
    SetScore(self:Progress() * (self.config.kind == "spider" and 100 or 10))
end

function Solitaire:SaveState()
    return self.moves .. "," .. self.draws .. "," .. self.passes
end

function Solitaire:LoadState(piles, data)
    local moves, draws, passes = string.match(data, "^(%d+),(%d+),(%d+)$")
    assert(moves, "Invalid solitaire snapshot")
    self.piles, self.moves, self.draws, self.passes = piles, tonumber(moves), tonumber(draws), tonumber(passes)
    self.notice, self.selected = "", -1
    self:Layout()
end

function Solitaire:Name(index)
    if index == self.config.stock then return "stock" end
    if index == self.config.waste then return "waste" end
    if index >= self.config.first and index <= self.config.last then
        if self.config.kind == "pyramid" then return "pyramid card " .. (index - 1) end
        return "column " .. (index - self.config.first + 1)
    end
    if self.config.kind == "freecell" and index < 4 then return "free cell " .. (index + 1) end
    return "foundation"
end

function Solitaire:CardName(card)
    local ranks = {[1] = "Ace", [11] = "Jack", [12] = "Queen", [13] = "King"}
    local suits = {"hearts", "diamonds", "clubs", "spades"}
    return (ranks[card.rank] or card.rank) .. " of " .. suits[card.suit + 1]
end

function Solitaire:FindMove()
    for source = 0, self.piles:size() - 1 do
        local pile = self.piles:get(source)
        for index = pile.cards:size() - 1, 0, -1 do
            if CanPickup(self.piles, source, index) then
                local cards = VectorCard.new()
                for i = index, pile.cards:size() - 1 do cards:push_back(pile.cards:get(i)) end
                for target = 0, self.piles:size() - 1 do
                    local destination = self.piles:get(target)
                    local equivalent = index == 0 and pile.type == PileType.Tableau and
                        destination.type == PileType.Tableau and destination.cards:empty()
                    if target ~= source and not equivalent and CanDrop(self.piles, source, target, cards) then
                        return source, target, index
                    end
                end
            end
        end
    end
end

function Solitaire:Hint()
    local source, target, index = self:FindMove()
    if source then
        self.notice = "Move " .. self:CardName(self.piles:get(source).cards:get(index)) ..
            " from " .. self:Name(source) .. " to " .. self:Name(target) .. "."
    elseif self.config.stock and not self.piles:get(self.config.stock).cards:empty() then
        self.notice = "Draw a card from the stock."
        if self.config.kind == "spider" then
            self.notice = "Deal a new row from the stock."
            for i = self.config.first, self.config.last do
                if self.piles:get(i).cards:empty() then self.notice = "Fill every empty column before dealing a row."; break end
            end
        end
    elseif self.config.waste and not self.piles:get(self.config.waste).cards:empty() and
        (self.config.kind == "klondike" or self.config.kind == "tutorial" or
         self.config.kind == "pyramid" and self.passes < 2) then
        self.notice = "Recycle the waste to draw again."
    else
        self.notice = "No card moves found. Undo a move or try a new deal."
    end
end

-- Foundation automation is an explicit action. Avoid moving ranks needed to
-- build the tableau: both opposite suits must reach rank-1, the same color rank-2.
function Solitaire:SafeFoundations()
    local moved = false
    for step = 1, 52 do
        local progress = {[0] = 0, [1] = 0, [2] = 0, [3] = 0}
        for f = self.config.foundationFirst, self.config.foundationLast do
            local cards = self.piles:get(f).cards
            if not cards:empty() then progress[cards:back().suit] = cards:back().rank end
        end
        local found = false
        for source = 0, self.piles:size() - 1 do
            local pile = self.piles:get(source)
            local index = pile.cards:size() - 1
            if pile.type ~= PileType.Foundation and index >= 0 and CanPickup(self.piles, source, index) then
                local card, safe = pile.cards:back(), true
                if card.rank > 2 then
                    for suit = 0, 3 do
                        if suit ~= card.suit then
                            local sameColor = (suit < 2) == card:IsRed()
                            if progress[suit] < card.rank - (sameColor and 2 or 1) then safe = false end
                        end
                    end
                end
                if safe then
                    local stack = VectorCard.new()
                    stack:push_back(card)
                    for f = self.config.foundationFirst, self.config.foundationLast do
                        if CanDrop(self.piles, source, f, stack) then
                            self.piles:get(f).cards:push_back(pile.cards:take_back())
                            AfterMove(self.piles, source, f, index)
                            found, moved = true, true
                            break
                        end
                    end
                end
            end
            if found then break end
        end
        if not found then break end
    end
    if not moved then self.notice = "No safe foundation moves are available." end
end

function Solitaire:Text(x, y, text, color, size, width)
    color = color or Theme.Colors.Text
    DrawBoardText(x, y, text, size or 18, width or 0, color[1], color[2], color[3])
end

function Solitaire:Background()
    local color = Theme.Toolbar
    DrawBoardPanel(40, 48, 956, 610, color[1], color[2], color[3])
    color = Theme.Colors.PopupBg
    DrawBoardPanel(1008, 48, 232, 610, color[1], color[2], color[3])
end

function Solitaire:Button(x, y, w, label, action, enabled)
    if DrawBoardButton(x, y, w, 36, label, enabled) then PerformAction(action) end
end

function Solitaire:Draw()
    if not self.piles then return end
    local progress = self:Progress()
    local won = progress == self.config.goal
    local stuck = not won and (self.config.kind == "golf" or self.config.kind == "pyramid" and self.passes >= 2) and
        self.piles:get(self.config.stock).cards:empty() and self:FindMove() == nil
    self:Text(62, 66, string.upper(GameName), Theme.CardHover, 32)
    self:Text(62, 110, self.config.description, Theme.EmptyPileText, 15, 900)
    if self.config.kind ~= "tutorial" then
        for _, entry in ipairs({{self.config.stock, "STOCK"}, {self.config.waste, "WASTE"},
            {self.config.foundationFirst, self.config.kind == "spider" and "COMPLETED RUNS" or "FOUNDATIONS"}}) do
            if entry[1] then
                local pile = self.piles:get(entry[1])
                self:Text(pile.pos.x, pile.pos.y - 23, entry[2], Theme.EmptyPileText, 14)
            end
        end
        if self.config.kind == "pyramid" then self:Text(900, 137, "CLEARED", Theme.EmptyPileText, 14) end
    end
    self:Text(1028, 74, won and "COMPLETE" or stuck and "NO MOVES" or "YOUR GAME", Theme.CardHover, 21)
    self:Text(1028, 116, self.config.progress .. "\n" .. progress .. " / " .. self.config.goal, nil, 19, 188)
    self:Text(1028, 174, "Moves: " .. self.moves .. "\nScore: " .. GetScore(), Theme.EmptyPileText, 17)
    local seconds = math.floor(GetTime())
    self:Text(1028, 219, string.format("Time: %02d:%02d", math.floor(seconds / 60), seconds % 60), Theme.EmptyPileText, 17)
    if self.config.foundationFirst and self.config.kind ~= "spider" then
        self:Button(1028, 248, 188, self.config.safeLabel or "Safe foundations", "safe", not won)
    elseif self.config.stock then
        self:Text(1028, 254, "Stock: " .. self.piles:get(self.config.stock).cards:size(), Theme.EmptyPileText, 17)
    end
    self:Text(1028, 298, won and "Well played. Undo to review your last move, or start a new deal." or
        stuck and "No moves or stock draws remain. Undo to try a different path, or start a new deal." or
        self.notice ~= "" and self.notice or self.config.tip, nil, 16, 188)
    self:Button(1028, 430, 188, "Hint", "hint", not won)
    if self.config.stock then
        local available = not self.piles:get(self.config.stock).cards:empty() or
            self.config.waste and not self.piles:get(self.config.waste).cards:empty() and
            (self.config.kind == "klondike" or self.config.kind == "tutorial" or self.config.kind == "pyramid" and self.passes < 2)
        if self.config.kind == "spider" then
            available = self.piles:get(self.config.stock).cards:size() >= 10
            for i = self.config.first, self.config.last do
                if self.piles:get(i).cards:empty() then available = false; break end
            end
        end
        self:Button(1028, 474, 188, self.config.mainLabel, "main", not won and available or false)
    else
        self:Text(1028, 477, "No stock. Every card\nis already in play.", Theme.EmptyPileText, 15, 188)
    end
    self:Button(1028, 522, 90, "Undo", "engine:undo", CanUndo())
    self:Button(1126, 522, 90, "Redo", "engine:redo", CanRedo())
    self:Button(1028, 570, 188, "New deal", "engine:restart", true)
    self:Text(1028, 620, "Ctrl+Z / Ctrl+Y\nF2: new deal", Theme.EmptyPileText, 14)
end
