#!/usr/bin/env python3
"""Extract real, verified engine signatures from a target libminecraftpe.so and
merge them into src/sig/signatures.json.

This is the correct way to "find" bedrocklua's signatures: instead of guessing
mangled names, it reads them out of the actual binary. libminecraftpe.so for
Android exports a large dynamic symbol table, so most of the functions bedrocklua
hooks resolve directly by symbol; anything still missing afterwards needs a byte
pattern (see docs/SIGNATURES.md).

Usage:
    scripts/extract_signatures.py <libminecraftpe.so> [version] [--json PATH] [--write]

Without --write it prints what it found (dry run). With --write it merges the
discovered symbols into src/sig/signatures.json as verified candidates.

Requires: llvm-nm (from the Android NDK) or a binutils nm with --demangle, plus
python3. Pass an NDK nm via the NM environment variable if it isn't on PATH.
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

# logical name -> list of matchers; a matcher is a list of substrings that must
# ALL appear in the demangled symbol. The first matcher that yields hits wins.
# Distinguishing parameter types are included so overloaded methods (handle,
# sendNetworkPacket, ...) resolve to the intended overload.
TARGETS = {
    "Level::tick": [["Level::tick("]],
    "ServerLevel::onLevelLoaded": [["Level::onLevelLoaded("], ["ServerLevel::onLevelLoaded("]],
    "ServerNetworkHandler::handleTextPacket": [["ServerNetworkHandler::handle(", "TextPacket"]],
    "TextPacket::createChat": [["TextPacket::createChat("]],
    "ServerPlayer::sendNetworkPacket": [
        ["ServerPlayer::sendNetworkPacket("],
        ["Player::sendNetworkPacket("],
    ],
    "MinecraftCommands::executeCommand": [["MinecraftCommands::executeCommand("]],
    "ResourcePackManager::setStack": [["ResourcePackManager::setStack("]],
    "Level::getActivePlayerCount": [["Level::getActivePlayerCount("]],
    "Actor::getNameTag": [["Actor::getNameTag("]],
    "Actor::getPosition": [["Actor::getPosition("]],
    "Player::getInventory": [
        ["Player::getInventory("],
        ["Player::getSupplies("],
        ["Player::getSuppliesContainer("],
    ],
    "ScriptModuleMinecraft::registerBindings": [
        ["ScriptModuleMinecraft", "Bindings"],
        ["ScriptModule", "InitializeModule"],
    ],
}

FUNCTION_TYPES = set("TtWw")


def find_nm() -> str:
    for cand in (os.environ.get("NM"), "llvm-nm", "nm"):
        if cand and shutil.which(cand):
            return cand
    sys.exit("error: no nm/llvm-nm found. Set NM=/path/to/llvm-nm (from the NDK).")


def dump_symbols(nm: str, so: str):
    """Returns a list of (type, mangled, demangled) for defined function symbols.

    nm -D and nm -DC iterate the dynamic symbol table in the same order, so the
    two outputs zip line-for-line to pair each mangled name with its demangling.
    """
    def run(args):
        out = subprocess.run([nm, *args, so], capture_output=True, text=True)
        if out.returncode != 0:
            sys.exit(f"error: {nm} {' '.join(args)} failed:\n{out.stderr}")
        return out.stdout.splitlines()

    mangled = run(["-D", "--defined-only"])
    demangled = run(["-DC", "--defined-only"])
    if len(mangled) != len(demangled):
        # Fall back to parsing independently if the line counts diverge.
        sys.exit("error: mangled/demangled symbol counts differ; cannot pair them")

    syms = []
    for m_line, d_line in zip(mangled, demangled):
        parts = m_line.split(maxsplit=2)
        if len(parts) < 3:
            continue
        _addr, sym_type, m_name = parts[0], parts[1], parts[2]
        if sym_type not in FUNCTION_TYPES:
            continue
        d_parts = d_line.split(maxsplit=2)
        d_name = d_parts[2] if len(d_parts) >= 3 else m_name
        syms.append((sym_type, m_name, d_name))
    return syms


def match_targets(syms):
    """logical name -> ordered list of mangled symbols (best/shortest first)."""
    results = {}
    for logical, matchers in TARGETS.items():
        hits = []
        for matcher in matchers:
            for _t, mangled, demangled in syms:
                if all(s in demangled for s in matcher):
                    hits.append((mangled, demangled))
            if hits:
                break  # first matcher that produced results wins
        # Prefer the shortest demangling (fewest/most-specific args), dedupe.
        hits.sort(key=lambda h: len(h[1]))
        seen, ordered = set(), []
        for mangled, demangled in hits:
            if mangled not in seen:
                seen.add(mangled)
                ordered.append((mangled, demangled))
        results[logical] = ordered
    return results


def merge(json_path: Path, results, version: str, write: bool):
    data = json.loads(json_path.read_text())
    sigs = data.setdefault("signatures", {})

    resolved = unresolved = added = confirmed = 0
    for logical, hits in results.items():
        entry = sigs.setdefault(logical, {"candidates": []})
        if not hits:
            unresolved += 1
            print(f"  [MISS] {logical}: no exported symbol matched (needs a byte pattern)")
            continue
        resolved += 1
        if len(hits) > 1:
            print(f"  [MULTI] {logical}: {len(hits)} matches; keeping all, prune if needed")
        for mangled, demangled in hits:
            existing = next((c for c in entry["candidates"]
                             if c.get("symbol") == mangled), None)
            if existing is not None:
                # The symbol we already shipped as a guess is real on this build:
                # confirm it (and pin the exact version).
                if not existing.get("verified"):
                    confirmed += 1
                existing["verified"] = True
                existing["version"] = version
                print(f"  [CONFIRM] {logical} -> {mangled}")
            else:
                # New, binary-verified symbol; try it first.
                entry["candidates"].insert(
                    0, {"symbol": mangled, "version": version, "verified": True})
                added += 1
                print(f"  [ADD]  {logical} -> {mangled}")
            print(f"           ({demangled})")

    print(f"\nResolved {resolved}/{len(results)} logical names "
          f"({unresolved} need byte patterns); {added} added, {confirmed} confirmed.")
    if write:
        json_path.write_text(json.dumps(data, indent=2) + "\n")
        print(f"Wrote {json_path}")
    else:
        print("Dry run (pass --write to update signatures.json).")


def main():
    args = [a for a in sys.argv[1:] if a not in ("--write",)]
    write = "--write" in sys.argv
    json_path = Path(__file__).resolve().parent.parent / "src" / "sig" / "signatures.json"
    if "--json" in args:
        i = args.index("--json")
        json_path = Path(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    so = args[0]
    version = args[1] if len(args) > 1 else "unknown"
    if not Path(so).is_file():
        sys.exit(f"error: not a file: {so}")

    nm = find_nm()
    print(f"Dumping symbols from {so} with {nm} ...")
    syms = dump_symbols(nm, so)
    print(f"Read {len(syms)} defined function symbols.\nMatching {len(TARGETS)} targets:")
    results = match_targets(syms)
    merge(json_path, results, version, write)


if __name__ == "__main__":
    main()
