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
Android. bedrocklua is **self-contained**: it links the
[GlossHook](https://github.com/XMDS/GlossHook) inline-hooking library statically
and resolves engine symbols at runtime, so it needs no separately built
`libpreloader.so`. The mod's constructor fires when LeviLauncher loads the
`.so`, waits for `libminecraftpe.so`, then starts the runtime.

## Supported versions

Declared for **Minecraft Bedrock 1.21.* – 1.26.*** (plus a `1.*` forward
catch-all in `manifest.json`). The core runtime — the Lua VM, behavior-pack
`language:"lua"` loading, scheduler, events, `Vector`/`ItemStack` — is
**version-independent** and runs on any of them. Engine-offset features resolve
per-version from the candidate table and degrade gracefully where a symbol does
not match the running build, so a broad version declaration is safe.

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

## Bring your own Lua API (manifest imports)

When the built-in surface isn't enough, a pack can import extra Lua modules from
a **URL** or a **local file** and `require()`/`import()` them by name, via a
`bedrocklua.imports` block in `manifest.json`:

```json
"bedrocklua": {
  "imports": [
    { "name": "json",  "source": "https://example.com/json.lua", "sha256": "<hex>" },
    { "name": "mylib", "source": "scripts/lib/mylib.lua" }
  ]
}
```

URL modules are fetched through the game process (Android Java/TLS), cached for
offline use, and optionally `sha256`-verified. See
[docs/WRITING_LUA_PACKS.md](docs/WRITING_LUA_PACKS.md#importing-external-lua-modules-bring-your-own-api).

## Releases

The [Release workflow](.github/workflows/release.yml) builds and publishes
`libbedrocklua.so` (+ `bedrocklua.levipack` and `.sha256` checksums) to a rolling
prerelease on each push, and to a versioned release on `v*` tags.

## Build

Requirements: Android NDK r26b, CMake ≥ 3.18, network access (deps are fetched).

```sh
# 1. Vendor the GlossHook backend (header + prebuilt arm64 static lib)
bash scripts/fetch_deps.sh

# 2. Configure + build
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
  main.cpp                 constructor entry (waits for libminecraftpe.so)
  mod/                     lifecycle + fallback ticker, dladdr mod dir
  lua/                     LuaState (sol2), LuaScriptHost (scheduler), LuaEventBus
  pack/                    manifest parsing + behavior-pack discovery
  binding/                 @minecraft/server + @minecraft/server-ui mirror
  hook/                    HookManager (GlossHook) + Level/Chat/PackStack + seam
  sig/                     version-keyed signatures + /proc/self/maps scanner
  nbt/                     Bedrock LittleEndian + Network (VarInt/ZigZag) NBT
  util/                    logging, Result, ABI string reader
scripts/fetch_deps.sh      vendors GlossHook into third_party/glosshook
third_party/glosshook/     GlossHook header + prebuilt arm64 static lib (fetched)
examples/lua-behavior-pack Demo pack
docs/                      API + signatures + verification
packaging/package.sh       .levipack builder
```

## License & compliance

bedrocklua is a legitimate, EULA-respecting modding tool: it adds a scripting
runtime, mirrors the documented Script API, and contains no cheats, exploits, or
anti-cheat circumvention. Use it within Minecraft's EULA and platform policies.
