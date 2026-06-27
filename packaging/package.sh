#!/usr/bin/env bash
#
# Builds a distributable bedrocklua.levipack (a zip) from a compiled
# libbedrocklua.so. A .levipack is what LeviLaunchroid imports.
#
# Usage: packaging/package.sh path/to/libbedrocklua.so [output_dir]
set -euo pipefail

SO_PATH="${1:?usage: package.sh <libbedrocklua.so> [output_dir]}"
OUT_DIR="${2:-dist}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ ! -f "$SO_PATH" ]]; then
  echo "error: shared library not found: $SO_PATH" >&2
  exit 1
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

# Mod payload: manifest + native lib + the runtime-editable signature table.
cp "$ROOT/manifest.json" "$STAGE/manifest.json"
cp "$SO_PATH" "$STAGE/$(basename "$SO_PATH")"
cp "$ROOT/src/sig/signatures.json" "$STAGE/signatures.json"

mkdir -p "$OUT_DIR"
OUT_PACK_ABS="$(cd "$OUT_DIR" && pwd)/bedrocklua.levipack"
rm -f "$OUT_PACK_ABS"

# .levipack is a zip archive; zip from inside the stage so paths are flat.
( cd "$STAGE" && zip -r -X "$OUT_PACK_ABS" . )

echo "packed: $OUT_PACK_ABS"
unzip -l "$OUT_PACK_ABS"
