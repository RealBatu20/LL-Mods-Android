# Resolving signatures for a Minecraft version

bedrocklua targets **Minecraft Bedrock 1.21 – 1.26** (and beyond). It keeps every
engine target in [`src/sig/signatures.json`](../src/sig/signatures.json). Each
logical name lists candidate resolvers (mangled symbol names and/or byte
patterns) — list candidates from multiple versions and the first that resolves
against the loaded `libminecraftpe.so` wins, so one table serves the whole range.

```json
"Level::tick": {
  "candidates": [
    { "symbol": "_ZN5Level4tickEv", "version": "1.21.x" },
    { "pattern": "FF 83 00 D1 ?? ?? ?? ??", "version": "1.21.40" }
  ]
}
```

`symbol` candidates resolve through GlossHook's `GlossSymbol` against the loaded
`libminecraftpe.so`; `pattern` candidates (`?` / `??` = any byte) are scanned
over the module's executable mappings by `src/sig/ModuleScanner.cpp` (via
`/proc/self/maps`). Byte patterns are the reliable path for stripped Bedrock
internals that export no dynamic symbol.

## Why this is version-keyed instead of hardcoded

The symbols shipped in `signatures.json` are **candidates** derived from
LeviLamina-style naming. They have not been verified against every build. A
candidate that does not resolve is not a build error — the binding that needs it
raises a catchable Lua error and everything else keeps working.

## Finding the real signatures (automated)

The symbols here are *candidates* until checked against a real binary. The fastest
correct way to populate verified entries is to extract them straight from your
target `libminecraftpe.so` with the bundled tool — no disassembler needed,
because libminecraftpe.so for Android exports a large dynamic symbol table.

1. Pull `libminecraftpe.so` (arm64-v8a) out of the Minecraft APK
   (`lib/arm64-v8a/libminecraftpe.so`).
2. Run the extractor (uses the NDK's `llvm-nm`; set `NM=` if it isn't on PATH):
   ```sh
   # dry run - shows what it found
   python3 scripts/extract_signatures.py /path/to/libminecraftpe.so 1.21.50

   # merge verified symbols into src/sig/signatures.json
   python3 scripts/extract_signatures.py /path/to/libminecraftpe.so 1.21.50 --write
   ```
   For each logical name it reports `[ADD]` (new symbol found), `[CONFIRM]` (a
   shipped candidate is real on this build — flips `"verified": true` and pins the
   version), or `[MISS]` (no exported symbol — that one needs a byte pattern).
3. Re-package (`packaging/package.sh`) — `signatures.json` is shipped alongside
   the `.so` and read from the mod directory at runtime, so you can iterate
   without recompiling.

For a `[MISS]`, derive a stable byte pattern from the function prologue in a
disassembler (IDA / Ghidra / Binary Ninja) and add it as a `pattern` candidate;
these are scanned at runtime by `src/sig/ModuleScanner.cpp`.

> The repository ships **unverified candidates** only: this environment has no
> `libminecraftpe.so`, so the maintainers cannot mark them verified — that is the
> one step that requires your target binary. Run the extractor (or paste an
> `llvm-nm -DC libminecraftpe.so` dump) to produce verified entries.

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
