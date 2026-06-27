# bedrocklua

A Lua scripting API for **Minecraft Bedrock on Android**, packaged as a
[LeviLaunchroid](https://levilaunchroid.levimc.org) native mod
(`libbedrocklua.so`).

It lets a behavior pack declare a **Lua** script module in `manifest.json` the
same way the vanilla Script API declares JavaScript — just `"language": "lua"`
instead of `"language": "javascript"`:

```json
{
  "modules": [
    {
      "type": "script",
      "language": "lua",
      "uuid": "…",
      "version": [1, 0, 0],
      "entry": "scripts/main.lua"
    }
  ]
}
```

bedrocklua discovers such packs, runs their entry script in an embedded
**Lua 5.4** VM (via [sol2](https://github.com/ThePhd/sol2)), and exposes a broad
`@minecraft/server`-style API to the script.

This is a ground-up re-implementation of the concept behind the archived
[`Imrglop/bedrock.lua`](https://github.com/Imrglop/bedrock.lua) (which targeted
Windows 10 Edition with MinHook); none of that Windows code is portable to
Android, so bedrocklua is built on LeviLaunchroid's `preloader-android` (whose
GlossHook backend provides `pl::hook`) for ARM64.

## What works today vs. what needs a verified binary

The runtime splits cleanly into two tiers:

| Tier | Status | Examples |
| --- | --- | --- |
| **Offset-free** | Fully working | `system.run/runInterval/runTimeout`, `world.afterEvents`/`beforeEvents`, `Vector`, `ItemStack`, UI form *builders*, `world.sendMessage` (logs to logcat) |
| **Offset-dependent** | Resolves at runtime; degrades cleanly if unresolved | entity/block/inventory reads & writes, native chat broadcast, command execution, form `show()` |

Engine-internal hook targets live in [`src/sig/signatures.json`](src/sig/signatures.json)
as **version-keyed candidates**. At startup each candidate is tried against the
loaded `libminecraftpe.so`; whatever resolves is used, and anything that does
not is reported in logcat and turned into a *catchable Lua error* if a script
calls it — never a crash. See [docs/VERIFICATION.md](docs/VERIFICATION.md) for
the verified/unverified matrix and [docs/SIGNATURES.md](docs/SIGNATURES.md) for
how to fill them in for a specific Minecraft version.

If **no** engine hook resolves, bedrocklua starts a ~20 Hz fallback ticker so the
scheduler and events still run — meaning the example pack is exercisable even
before any offset is verified.

## Build

Requirements: Android NDK r26b, CMake ≥ 3.18, network access (deps are fetched).
The hooking backend (GlossHook) ships inside `preloader-android`, so there is no
separate native library to provide.

```sh
# Configure + build
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 3. Package a .levipack
packaging/package.sh build/libbedrocklua.so
```

The GitHub Actions workflow [`.github/workflows/build.yml`](.github/workflows/build.yml)
does all of the above and uploads `libbedrocklua.so` + `bedrocklua.levipack`.

## Install & test

1. Import `bedrocklua.levipack` in LeviLaunchroid and enable it.
2. Copy `examples/lua-behavior-pack` into your world's behavior packs (or
   `development_behavior_packs`) and enable it for the world.
3. Launch the world and watch logcat: `adb logcat -s bedrocklua`.

You should see the example's startup messages, per-second heartbeats, and the
expected "unavailable" message for the offset-dependent `runCommand` call.

## Writing Lua packs

See [docs/WRITING_LUA_PACKS.md](docs/WRITING_LUA_PACKS.md) for the full API
surface. Minimal example:

```lua
local mc = import("@minecraft/server")
mc.world.sendMessage("hello from lua")
mc.system.runInterval(function()
    mc.world.sendMessage("tick " .. mc.system.currentTick)
end, 20)
```

## Project layout

```
src/
  main.cpp                 PL_REGISTER_MOD entry
  mod/                     lifecycle + fallback ticker
  lua/                     LuaState (sol2), LuaScriptHost (scheduler), LuaEventBus
  pack/                    manifest parsing + behavior-pack discovery
  binding/                 @minecraft/server + @minecraft/server-ui mirror
  hook/                    Level/Chat/PackStack hooks + phase-2 script-engine seam
  sig/                     version-keyed signature table
  nbt/                     Bedrock LittleEndian + Network (VarInt/ZigZag) NBT
  util/                    logging, Result, ABI string reader
examples/lua-behavior-pack Demo pack
docs/                      API + signatures + verification
packaging/package.sh       .levipack builder
```

## License & compliance

bedrocklua is a legitimate, EULA-respecting modding tool: it adds a scripting
runtime, mirrors the documented Script API, and contains no cheats, exploits, or
anti-cheat circumvention. Use it within Minecraft's EULA and platform policies.
