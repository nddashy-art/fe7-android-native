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
STARTUP_BIN="${TMPDIR:-/tmp}/fe7-engine-startup-test"
PPU_BIN="${TMPDIR:-/tmp}/fe7-gba-ppu-test"
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
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_bios_compat.cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_port.cpp" \
  "$ROOT/app/src/main/cpp/fe7_engine_startup.cpp" \
  "$ROOT/tests/fe7_engine_startup_test.cpp" -o "$STARTUP_BIN"
"$STARTUP_BIN"
"$CXX" $COMMON -I"$ROOT/app/src/main/cpp" \
  "$ROOT/app/src/main/cpp/gba_memory.cpp" \
  "$ROOT/app/src/main/cpp/gba_ppu.cpp" \
  "$ROOT/tests/gba_ppu_test.cpp" -o "$PPU_BIN"
"$PPU_BIN"
python3 - "$ROOT" <<'PYTEST'
import importlib.util
import tempfile
from pathlib import Path
import sys

root = Path(sys.argv[1])
spec = importlib.util.spec_from_file_location(
    "prepare_fe7_native_source", root / "scripts" / "prepare_fe7_native_source.py"
)
assert spec is not None and spec.loader is not None
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)
source = """void AgbMain(void)
{
    InitGame();
    for (;;)
    {
        RunMainFunc();
        SoftResetIfKeyCombo();
    }
}

void PutBuildInfo(u16 * tm)
{
}
"""
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "main.c"
    path.write_text(source, encoding="utf-8")
    assert adapter.wrap_agb_main(path)
    adapted = path.read_text(encoding="utf-8")
    assert "void Fe7EngineRunFrame(void)" in adapted
    assert "    return;" in adapted
    assert "    RunMainFunc();" in adapted
    assert path.with_name("main.c.fe7native.bak").is_file()
    assert not adapter.wrap_agb_main(path)
    assert path.read_text(encoding="utf-8") == adapted
print("FE7 AgbMain adapter regression test passed")
PYTEST
rm -f "$BIOS_BIN" "$MEMORY_BIN" "$PORT_BIN" "$SERVICES_BIN" "$DMA_BIN" "$IRQ_BIN" "$STARTUP_BIN" "$PPU_BIN"
