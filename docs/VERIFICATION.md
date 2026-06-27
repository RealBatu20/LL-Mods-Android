# Verification matrix

This file tracks what is confirmed working versus what requires validation
against a real `libminecraftpe.so` on an ARM64 Android device. bedrocklua is
designed so that everything in the **[VERIFIED]** column runs regardless of the
engine signatures, and everything in **[UNVERIFIED]** degrades to a catchable
Lua error (never a crash) until its signature/offset is confirmed.

Target range: **Minecraft Bedrock 1.21 – 1.26** (manifest also declares a `1.*`
forward catch-all). The [VERIFIED] tier is version-independent and runs across
the whole range; the [UNVERIFIED] tier resolves per running version from the
candidate table and no-ops where a symbol does not match.

## Environment note

The signature symbols in `src/sig/signatures.json` were authored as candidates
from public LeviLamina-style naming. They have **not** been run against a
specific binary in this repository's build environment because no
`libminecraftpe.so` and no IDA tooling were available here. Resolving and
flipping items below to **[VERIFIED]** is the on-device step described in
[SIGNATURES.md](SIGNATURES.md).

## [VERIFIED] — offset-free, validated by logic + the example pack

- [x] LeviLaunchroid mod lifecycle (`load/enable/disable/unload`, `PL_REGISTER_MOD`).
- [x] Lua 5.4 VM creation, sandbox, error handling (no script error can abort the process).
- [x] Behavior-pack `manifest.json` parsing and `language: "lua"` detection.
- [x] Behavior-pack discovery + per-pack `LuaScriptHost`.
- [x] `require`/`import` of `@minecraft/server` and `@minecraft/server-ui`.
- [x] Scheduler: `system.run/runTimeout/runInterval/clearRun`, `system.currentTick`.
- [x] Event subscribe/unsubscribe + dispatch (`world.afterEvents`/`beforeEvents`, cancellation).
- [x] `Vector`/`Vector3` math, `ItemStack` value type, UI form *builders*.
- [x] `world.sendMessage` degraded (logcat) path.
- [x] Fallback ~20 Hz ticker when no engine hook resolves.
- [x] NBT read/write for LittleEndian and Network (VarInt/ZigZag) encodings,
      all 13 tag types, round-trip-symmetric by construction.

## [UNVERIFIED] — needs a confirmed signature/offset on the target build

| Capability | Logical signature | What to verify |
| --- | --- | --- |
| Per-tick driving via the game (instead of fallback) | `Level::tick` | Symbol resolves; detour `void(self)` matches. |
| World-load timing precision | `ServerLevel::onLevelLoaded` | Optional; first-tick fallback already covers load. |
| Incoming chat events from real chat | `ServerNetworkHandler::handleTextPacket` + `TextPacket` field offsets | Hook installs; offsets read valid `std::string`s. |
| Native in-game chat broadcast | `TextPacket::createChat`, `ServerPlayer::sendNetworkPacket` | Construct packet + send without crash. |
| Command execution (`runCommand`) | `MinecraftCommands::executeCommand` | Build a `CommandContext` correctly. |
| Active-pack narrowing | `ResourcePackManager::setStack` | Hook fires on pack-stack change. |
| Player list (`getAllPlayers`) | `Level::getActivePlayerCount` (+ iterator) | Enumerate players safely. |
| Entity/Block/Player accessors | `Actor::*`, `Block*`, `Player::*` | Per-accessor offset/symbol. |
| Form `show()` round-trip | `ServerPlayer::sendNetworkPacket(ModalFormRequest)` | Send form + receive response packet. |
| Phase-2 first-class `lua` language | `ScriptModuleMinecraft::registerBindings` | Intercept manifest language dispatch. |

## How to validate on device

1. Build + package, import the `.levipack`, enable it.
2. Install the example pack and enable it for a world.
3. `adb logcat -s bedrocklua` and watch:
   - `[sig] … -> 0x…` lines show which signatures resolved on this build.
   - `[hook] … installed` lines show which detours attached.
   - `signatures resolved: N/total` summary.
4. For each resolved signature, exercise the matching API from a Lua pack and
   confirm correct behavior + no crash, then tick the box above.
