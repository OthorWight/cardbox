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

    if DrawBoardButton(20, 310, 250, 40, "6. Debug Library Access") then
        -- The debug library can bypass almost all sandbox protections, inspect memory, and alter execution
        local status, err = pcall(function() return debug.getregistry() end)
        status_msg = status and "FAILED: debug library accessible!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 360, 250, 40, "7. Dynamic Code Eval") then
        -- load/loadstring can evaluate arbitrary code. If bytecode is allowed, it can crash the VM.
        local status, err = pcall(function()
            local func = loadstring or load
            return func("return 'hacked'")()
        end)
        status_msg = status and "FAILED: load/loadstring worked!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 410, 250, 40, "8. Global Env Access") then
        -- Accessing _G allows modifying the global state outside the sandbox
        local status, err = pcall(function()
            local env = _G or getfenv(0)
            return type(env.DrawBoardButton)
        end)
        status_msg = status and "FAILED: Global env accessible!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(20, 460, 250, 40, "9. Metatable Poisoning") then
        -- Modifying fundamental metatables can break host application logic globally
        local status, err = pcall(function()
            getmetatable("").__tostring = function() return "poison" end
        end)
        status_msg = status and "FAILED: String metatable poisoned!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(290, 60, 250, 40, "10. Stack Overflow") then
        -- Deep recursion crashes the C stack if the engine hasn't limited max C stack depth
        local status, err = pcall(function()
            local function recurse() return recurse() + 1 end
            return recurse()
        end)
        status_msg = status and "FAILED: Stack overflow didn't crash cleanly!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(290, 110, 250, 40, "11. Coroutine Escape") then
        -- Coroutines get their own thread state. C++ instruction hooks often fail to carry over to them!
        local status, err = pcall(function()
            local co = coroutine.create(function() while true do end end)
            coroutine.resume(co)
        end)
        status_msg = status and "FAILED: Coroutine infinite loop escaped!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(290, 160, 250, 40, "12. Bytecode Execution") then
        -- If load() allows binary chunks, attackers can execute malformed bytecode to segfault the C++ VM
        local status, err = pcall(function()
            local f = (loadstring or load)("\27Lua")
            if f then f() end
        end)
        status_msg = status and "FAILED: Accepted raw bytecode!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(290, 210, 250, 40, "13. App Exit / OS Panic") then
        -- os.exit immediately terminates the host C++ application
        local status, err = pcall(function() os.exit(1) end)
        status_msg = status and "FAILED: os.exit is available!" or ("SAFE: " .. tostring(err))
    end

    if DrawBoardButton(290, 260, 250, 40, "14. string.rep OOM") then
        -- Extremely fast way to exhaust memory bypassing standard infinite loop checks
        local status, err = pcall(function()
            -- Attempt to allocate a string spanning hundreds of megabytes
            local s = string.rep("HACK", 1024 * 1024 * 256) 
            return #s
        end)
        -- If this succeeds without Lua throwing an Out Of Memory error, your allocator limits are missing
        status_msg = status and "FAILED: Huge string allocated!" or ("SAFE: " .. tostring(err))
    end
end
