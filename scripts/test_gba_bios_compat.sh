#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CXX="${CXX:-c++}"
BIOS_BIN="${TMPDIR:-/tmp}/fe7-gba-bios-compat-test"
MEMORY_BIN="${TMPDIR:-/tmp}/fe7-gba-memory-test"
COMMON="-std=c++17 -Wall -Wextra -Werror"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/tests/gba_bios_compat_test.cpp" -o "$BIOS_BIN"
"$BIOS_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/tests/gba_memory_test.cpp" -o "$MEMORY_BIN"
"$MEMORY_BIN"
rm -f "$BIOS_BIN" "$MEMORY_BIN"
