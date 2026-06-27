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

## Importing external Lua modules (bring your own API)

When the built-in `@minecraft/server` surface isn't enough, a pack can import
extra Lua modules — from a URL or a local file — and `require()`/`import()` them
by name. Declare them in a `bedrocklua.imports` block:

```json
{
  "modules": [ { "type": "script", "language": "lua", "entry": "scripts/main.lua" } ],
  "bedrocklua": {
    "imports": [
      { "name": "json",  "source": "https://example.com/json.lua",
        "sha256": "<hex>", "optional": false },
      { "name": "mylib", "source": "scripts/lib/mylib.lua" }
    ]
  }
}
```

| field | meaning |
| --- | --- |
| `name` | the key you `require(name)` / `import(name)` with |
| `source` | `http(s)://` URL, `file://` URL, absolute path, or a path relative to the pack (also accepts `url`) |
| `sha256` | optional lowercase hex; the fetched bytes are verified against it |
| `optional` | when `true`, a failed import is skipped with a warning instead of erroring |

Notes:
- URL modules are fetched through the game process's network stack (Android
  Java/TLS) and **cached** under the mod data dir, so later loads work offline; a
  failed fetch falls back to the cached copy.
- A non-optional import that fails still registers, but `require()`-ing it raises
  a catchable Lua error naming the failure — wrap risky imports in `pcall`.
- The imported module's `return` value is what `require` gives you, exactly like
  standard Lua modules.

```lua
local mylib = require("mylib")       -- or import("mylib")
local json  = require("json")
```

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
| `world.setDynamicProperty(id, v)` / `getDynamicProperty(id)` | **Works** — session-scoped in-memory store (offset-free). `getDynamicPropertyIds()`, `clearDynamicProperties()` too. |
| `world.getTimeOfDay()` / `setTimeOfDay()` / `getDay()` | Offset-dependent; degrade cleanly. |

### Constants

`@minecraft/server` ships offset-free constant tables: `GameMode`,
`MinecraftDimensionTypes`, `EntityComponentTypes`, `ItemComponentTypes`,
`BlockComponentTypes`, `EquipmentSlot`, `EntityDamageCause`, `DisplaySlotId`,
plus `TicksPerSecond` (20) and `TicksPerDay` (24000).

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
