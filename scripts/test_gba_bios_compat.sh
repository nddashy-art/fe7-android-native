#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CXX="${CXX:-c++}"
BIOS_BIN="${TMPDIR:-/tmp}/fe7-gba-bios-compat-test"
MEMORY_BIN="${TMPDIR:-/tmp}/fe7-gba-memory-test"
PORT_BIN="${TMPDIR:-/tmp}/fe7-gba-port-test"
SERVICES_BIN="${TMPDIR:-/tmp}/fe7-gba-bios-services-test"
DMA_BIN="${TMPDIR:-/tmp}/fe7-gba-dma-test"
IRQ_BIN="${TMPDIR:-/tmp}/fe7-gba-irq-test"
COMMON="-std=c++17 -Wall -Wextra -Werror"

"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/tests/gba_bios_compat_test.cpp" -o "$BIOS_BIN"
"$BIOS_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/tests/gba_memory_test.cpp" -o "$MEMORY_BIN"
"$MEMORY_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_port.cpp" \
  "$ROOT/tests/gba_port_test.cpp" -o "$PORT_BIN"
"$PORT_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_port.cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_services.cpp" \
  "$ROOT/app/src/main/cpp/gba_dma.cpp" \
  "$ROOT/app/src/main/cpp/gba_irq.cpp" \
  "$ROOT/tests/gba_bios_services_test.cpp" -o "$SERVICES_BIN"
"$SERVICES_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_port.cpp" \
  "$ROOT/app/src/main/cpp/gba_dma.cpp" \
  "$ROOT/tests/gba_dma_test.cpp" -o "$DMA_BIN"
"$DMA_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_port.cpp" \
  "$ROOT/app/src/main/cpp/gba_irq.cpp" \
  "$ROOT/tests/gba_irq_test.cpp" -o "$IRQ_BIN"
"$IRQ_BIN"
rm -f "$BIOS_BIN" "$MEMORY_BIN" "$PORT_BIN" "$SERVICES_BIN" "$DMA_BIN" "$IRQ_BIN"
