GameName = "Security Sandbox Test"
NumDecks = 1
AutoCenter = false
HelpText = "Use the buttons to trigger malicious actions. The engine should catch these and not crash!"

function Init(piles, deck) end
function CanPickup(piles, pileIdx, cardIdx) return false end
function CanDrop(piles, sourcePileIdx, targetPileIdx, dragCards) return false end
function AfterMove(piles, sourcePileIdx, targetPileIdx, cardIdx) end
function HandleClick(piles, pileIdx) end
function AutoSolve(piles) return {} end
function IsWon(piles) return false end

local status_msg = "Awaiting attack..."

function Draw()
    DrawBoardText(20, 20, "Status: " .. status_msg)

    if DrawBoardButton(20, 60, 250, 40, "1. OS Command Injection") then
        -- Attempt to format the hard drive or run a shell command
        local status, err = pcall(function() os.execute("echo Hacked") end)
        status_msg = status and "FAILED: os.execute worked!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 110, 250, 40, "2. File I/O Access") then
        -- Attempt to overwrite user files
        local status, err = pcall(function()
            local f = io.open("hacked.txt", "w")
            if f then f:write("hacked"); f:close() end
        end)
        status_msg = status and "FAILED: io.open worked!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 160, 250, 40, "3. Require Dynamic Libs") then
        -- Attempt to load external C/C++ libraries (which could bypass the sandbox)
        local status, err = pcall(function() require("package") end)
        status_msg = status and "FAILED: require worked!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 210, 250, 40, "4. Memory Exhaustion") then
        status_msg = "Attempting memory exhaustion... (check console)"
        -- Exponential string concatenation to hit the 10MB limit rapidly
        local s = "spam"
        while true do
            s = s .. s
        end
    end

    if DrawBoardButton(20, 260, 250, 40, "5. Infinite Loop (CPU)") then
        status_msg = "Attempting infinite loop... (check console)"
        while true do
            -- Hangs the thread if instruction limits aren't globally active
        end
    end
end
