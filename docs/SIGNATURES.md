# Resolving signatures for a Minecraft version

bedrocklua keeps every engine target in
[`src/sig/signatures.json`](../src/sig/signatures.json). Each logical name lists
candidate resolvers (mangled symbol names and/or byte patterns). At startup the
first candidate that resolves against the loaded `libminecraftpe.so` wins.

```json
"Level::tick": {
  "candidates": [
    { "symbol": "_ZN5Level4tickEv", "version": "1.21.x" },
    { "pattern": "FF 83 00 D1 ?? ?? ?? ??", "version": "1.21.40" }
  ]
}
```

`symbol` is passed to the symbol resolver; `pattern` is a byte signature (`?` /
`??` = any byte). Both are tried via `pl::signature::resolveSignature`.

## Why this is version-keyed instead of hardcoded

The symbols shipped in `signatures.json` are **candidates** derived from
LeviLamina-style naming. They have not been verified against every build. A
candidate that does not resolve is not a build error — the binding that needs it
raises a catchable Lua error and everything else keeps working.

## Verifying / adding a build

1. Pull the `libminecraftpe.so` for your target version + ABI out of the APK
   (`lib/arm64-v8a/libminecraftpe.so`).
2. Load it in a disassembler (IDA / Ghidra / Binary Ninja) or dump symbols:
   ```sh
   $ANDROID_NDK/toolchains/llvm/prebuilt/*/bin/llvm-nm -C libminecraftpe.so | grep -i 'Level::tick'
   ```
3. For each logical name in `signatures.json`, confirm the mangled symbol exists
   (exported) or derive a stable byte pattern from the function prologue if it is
   stripped/local.
4. Update the `candidates` list, tagging each with its `version`.
5. Re-package (`packaging/package.sh`) — `signatures.json` is shipped alongside
   the `.so` and read from the mod directory at runtime, so you can iterate
   without recompiling.

## Field offsets (not in signatures.json)

A few hooks need struct field offsets, not function addresses:

- `src/hook/ChatHooks.cpp` — `kTextPacketAuthorOffset`, `kTextPacketMessageOffset`.
  Left at `0` (disabled, pass-through) by default. Set them to the verified byte
  offsets of the `TextPacket` author/message `std::string` fields to enable chat
  events.

Offsets are kept in code (not the JSON) because they are read with the
ABI-aware helper in `src/util/AbiString.hpp` and must match the exact struct
layout of the target build.

## Checklist before marking VERIFIED

- [ ] Symbol/pattern resolves at runtime (check the `[sig]` lines in logcat).
- [ ] The hook installs (`[hook] … installed` line).
- [ ] The detour's argument signature matches the real function (ARM64 AAPCS).
- [ ] No crash on world load over several minutes.
