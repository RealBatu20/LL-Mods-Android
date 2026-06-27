-- bedrocklua example script.
--
-- This is the Lua equivalent of a vanilla @minecraft/server JavaScript entry.
-- The behavior pack's manifest.json declares this module with
--   "type": "script", "language": "lua", "entry": "scripts/main.lua"
-- and bedrocklua runs it in an embedded Lua 5.4 VM.

local mc = import("@minecraft/server")
local ui = import("@minecraft/server-ui")

local world = mc.world
local system = mc.system

-- 1. Log + (degraded) broadcast a message at startup.
world.sendMessage("bedrocklua example loaded!")

-- 2. Vector math (offset-free, always works).
local a = mc.Vector.create(1, 2, 3)
local b = mc.Vector.up
local sum = mc.Vector.add(a, b)
world.sendMessage(string.format("vector sum = (%g, %g, %g)", sum.x, sum.y, sum.z))

-- 3. A repeating task every 20 ticks (~1 second) via the scheduler.
local seconds = 0
system.runInterval(function()
    seconds = seconds + 1
    world.sendMessage("heartbeat tick=" .. tostring(system.currentTick) ..
        " seconds=" .. tostring(seconds))
end, 20)

-- 4. A one-shot delayed task.
system.runTimeout(function()
    world.sendMessage("this ran once, 40 ticks after load")
end, 40)

-- 5. Event subscriptions (offset-free; fire from engine or fallback hooks).
world.afterEvents.worldLoad.subscribe(function(ev)
    world.sendMessage("worldLoad event received")
end)

world.afterEvents.playerJoin.subscribe(function(ev)
    world.sendMessage("player joined: " .. tostring(ev.playerName))
end)

-- 6. A cancellable before-event: block any chat message containing "secret".
world.beforeEvents.chatSend.subscribe(function(ev)
    if ev.message and string.find(ev.message, "secret") then
        ev.cancel = true
        world.sendMessage("blocked a message containing 'secret'")
    end
end)

-- 7. Build an ItemStack (value type, offset-free).
local sword = mc.ItemStack("minecraft:diamond_sword", 1)
sword:setLore({ "A bedrocklua-made blade" })
world.sendMessage("created item " .. sword.typeId .. " x" .. tostring(sword.amount))

-- 8. Build a UI form definition (builders work; show() needs verified offsets).
local form = ui.ActionFormData()
    :title("bedrocklua")
    :body("Pick an option")
    :button("Hello")
    :button("World")
world.sendMessage("built ActionFormData with " .. tostring(#form.buttons) .. " buttons")

-- 9. Offset-dependent calls degrade to a catchable error, never a crash.
local ok, err = pcall(function()
    return world.getDimension("minecraft:overworld"):runCommand("say hi")
end)
if not ok then
    world.sendMessage("runCommand unavailable (expected until verified): " .. tostring(err))
end
