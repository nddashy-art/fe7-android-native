#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CXX="${CXX:-c++}"
BIN="${TMPDIR:-/tmp}/fe7-gba-bios-compat-test"
"$CXX" -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/tests/gba_bios_compat_test.cpp" -o "$BIN"
"$BIN"
rm -f "$BIN"
