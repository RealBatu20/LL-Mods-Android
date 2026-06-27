# Writing Lua behavior packs for bedrocklua

A bedrocklua pack is an ordinary Minecraft behavior pack whose `manifest.json`
declares a script module with `"language": "lua"`. The entry file runs in a
Lua 5.4 sandbox with a `@minecraft/server`-style API.

## manifest.json

```json
{
  "format_version": 2,
  "header": {
    "name": "My Lua Pack",
    "uuid": "<unique-uuid>",
    "version": [1, 0, 0],
    "min_engine_version": [1, 21, 0]
  },
  "modules": [
    {
      "type": "script",
      "language": "lua",
      "uuid": "<unique-uuid>",
      "version": [1, 0, 0],
      "entry": "scripts/main.lua"
    }
  ],
  "dependencies": [
    { "module_name": "@minecraft/server", "version": "1.0.0" },
    { "module_name": "@minecraft/server-ui", "version": "1.0.0" }
  ]
}
```

Only the `language` value differs from a vanilla JavaScript pack.

## Importing the API

```lua
local mc = import("@minecraft/server")        -- or require("@minecraft/server")
local ui = import("@minecraft/server-ui")
```

`require()` also works because the modules are registered in `package.preload`.
Splitting your pack into multiple `.lua` files works via `require("other")`
(the pack's `scripts/` folder is on `package.path`).

## Sandbox

The VM opens `base`, `string`, `table`, `math`, `coroutine`, `utf8`, `os`
(time helpers only) and `package`. `io`, `os.execute/exit/remove`, `dofile`,
`loadfile`, and `package.loadlib` are removed.

## `system` (scheduler) — fully working

| API | Description |
| --- | --- |
| `system.run(fn)` | Run `fn` on the next tick. Returns a handle. |
| `system.runTimeout(fn, ticks)` | Run `fn` once after `ticks` ticks. |
| `system.runInterval(fn, ticks)` | Run `fn` every `ticks` ticks. |
| `system.clearRun(handle)` | Cancel a scheduled run/interval. |
| `system.currentTick` | Current tick number (property). |

20 ticks ≈ 1 second.

## `world` — events working; accessors degrade

| API | Status |
| --- | --- |
| `world.sendMessage(msg)` | Logs to logcat (native chat broadcast pending offset verification). `msg` is a string or a `{ rawtext = { { text=".." } } }` table. |
| `world.afterEvents.<name>.subscribe(fn)` / `unsubscribe(fn)` | Working. Any event name is valid. |
| `world.beforeEvents.<name>.subscribe(fn)` | Working. Set `ev.cancel = true` to cancel. |
| `world.getAllPlayers()` / `getPlayers()` | Returns `{}` until the player-list accessor is verified. |
| `world.getDimension(id)` | Returns a dimension handle; its engine ops raise a catchable error until verified. |

### Events delivered today

- `world.afterEvents.worldLoad` / `worldInitialize`
- `world.afterEvents.playerJoin` / `playerLeave` (payload: `playerName`, `playerId`)
- `world.afterEvents.chatSend` (payload: `sender.name`, `message`)
- `world.beforeEvents.chatSend` (cancellable; payload: `sender`, `message`, `cancel`)

Subscribing to any other name is allowed and will fire once the relevant hook is
wired.

## `Vector` / `Vector3` — fully working

`create(x,y,z)`, `add`, `subtract`, `scale`, `dot`, `length`, `distance`,
`normalize`, and constants `up/down/forward/back/left/right/zero/one`. Vectors
are plain `{x=,y=,z=}` tables.

## `ItemStack` — fully working (value type)

```lua
local item = mc.ItemStack("minecraft:diamond_sword", 1)
item:setAmount(1)
item:setLore({ "line 1", "line 2" })
local copy = item:clone()
```

## `@minecraft/server-ui` — builders work, `show()` degrades

```lua
local form = ui.ActionFormData():title("T"):body("B"):button("OK")
-- form.buttons is populated; form:show(player) raises a catchable error
```

`ActionFormData`, `MessageFormData`, and `ModalFormData` (with
`textField/toggle/slider/dropdown`) all build their definition tables.

## Offset-dependent APIs

Entity/Block/Player accessors, command execution, and form `show()` raise a
catchable Lua error of the form:

```
<api> unavailable: engine signature '<name>' is unresolved on this build
```

Wrap them in `pcall` and they will never crash the game. They start working once
the corresponding signature is verified — see
[VERIFICATION.md](VERIFICATION.md).
