-- bedrocklua example script.
--
-- This is the Lua equivalent of a vanilla @minecraft/server JavaScript entry.
-- The behavior pack's manifest.json declares this module with
--   "type": "script", "language": "lua", "entry": "scripts/main.lua"
-- and bedrocklua runs it in an embedded Lua 5.4 VM.

-- bedrocklua uses its own module namespace (NOT the vanilla @minecraft/* names).
local mc    = import("@bedrocklua")        -- core API (like @minecraft/server)
local ui    = import("@bedrocklua-ui")     -- forms (like @minecraft/server-ui)
local admin = import("@bedrocklua-admin")  -- server administration
local net   = import("@bedrocklua-net")    -- networking

local world = mc.world
local system = mc.system

-- 0. Module versions.
world.sendMessage(string.format("bedrocklua example loaded! (@bedrocklua v%s, -ui v%s, -net v%s)",
    mc.version, ui.version, net.version))

-- 1. admin.broadcast (offset-free, logs to logcat).
admin.broadcast("hello from @bedrocklua-admin")

-- 1a. net.sha256 works offline; an async fetch is non-blocking.
world.sendMessage("sha256('bedrocklua') = " .. net.sha256("bedrocklua"))
net.fetchAsync("https://raw.githubusercontent.com/rxi/json.lua/master/json.lua", function(res)
    if res.ok then
        world.sendMessage("net.fetchAsync got " .. tostring(#res.body) .. " bytes")
    else
        world.sendMessage("net.fetchAsync failed (offline?): " .. tostring(res.error))
    end
end)

-- 1b. Use a module imported by name via the manifest's bedrocklua.imports
-- (local file source -> always works). The optional URL import "remote_demo"
-- is loaded only if reachable; we pcall it so being offline is harmless.
local greet = require("greet")
world.sendMessage(greet.greeting("world"))

local ok_remote, json = pcall(require, "remote_demo")
if ok_remote and type(json) == "table" then
    world.sendMessage("remote_demo lua module loaded from the internet")
else
    world.sendMessage("remote_demo not loaded (offline/optional) - that's fine")
end

-- 1c. Session-scoped dynamic properties (offset-free, fully working).
world.setDynamicProperty("launchCount", (world.getDynamicProperty("launchCount") or 0) + 1)
world.sendMessage("dynamic property launchCount = " ..
    tostring(world.getDynamicProperty("launchCount")))

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
