#!/usr/bin/env python3
"""Apply the first FE7-to-native memory-header adapter.

Usage: python3 scripts/prepare_fe7_native_source.py /path/to/fe7_us-main
Keeps .fe7native.bak copies and is idempotent. This changes register/pointer
macros only; it does not port ARM7TDMI assembly or build the game engine.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> bool:
    source = path.read_text(encoding="utf-8")
    if new in source:
        return False
    if old not in source:
        raise RuntimeError(f"expected source marker missing in {path}: {old!r}")
    backup = path.with_name(path.name + ".fe7native.bak")
    if not backup.exists():
        shutil.copy2(path, backup)
    path.write_text(source.replace(old, new, 1), encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path, help="extracted FE7 decompilation root")
    args = parser.parse_args()
    root = args.source_root.resolve()
    inc = root / "include"
    targets = [inc / "gbaio.h", inc / "gbavideo.h", inc / "hardware.h"]
    missing = [str(path) for path in targets if not path.is_file()]
    if missing:
        parser.error("source root is missing expected files: " + ", ".join(missing))

    repo_root = Path(__file__).resolve().parents[1]
    public_header = repo_root / "app" / "src" / "main" / "cpp" / "gba_port.h"
    destination_header = inc / "gba_port.h"
    if not public_header.is_file():
        raise RuntimeError(f"cannot locate the native C API header: {public_header}")
    if destination_header.exists():
        if destination_header.read_bytes() != public_header.read_bytes():
            raise RuntimeError(f"refusing to overwrite existing header: {destination_header}")
    else:
        shutil.copy2(public_header, destination_header)

    changed = 0
    changed += replace_once(
        inc / "gbaio.h",
        '#include "prelude.h"\n\n#define REG(type, addr) (*((type volatile *)(addr)))',
        '''#include "prelude.h"
#ifdef FE7_NATIVE_PORT
#include "gba_port.h"
#endif

#ifdef FE7_NATIVE_PORT
#define REG(type, addr) (*((volatile type *)fe7_gba_io_pointer((uint32_t)(addr), sizeof(type))))
#else
#define REG(type, addr) (*((type volatile *)(addr)))
#endif''',
    )
    changed += replace_once(
        inc / "gbaio.h",
        '#define INTR_VECTOR (*(void **)0x03007FFC)',
        '''#ifdef FE7_NATIVE_PORT
#define INTR_VECTOR (*fe7_gba_interrupt_vector_slot())
#else
#define INTR_VECTOR (*(void **)0x03007FFC)
#endif''',
    )
    changed += replace_once(
        inc / "gbavideo.h",
        '#include "prelude.h"\n\n#define VRAM_ADDR 0x06000000\n#define VRAM_SIZE 0x18000\n#define VRAM ((void *)VRAM_ADDR)\n\n#define PLTT_ADDR 0x05000000\n#define PLTT_SIZE 0x400\n#define PLTT ((void *)PLTT_ADDR)\n\n#define OAM_ADDR 0x07000000\n#define OAM_SIZE 0x400\n#define OAM ((void *)OAM_ADDR)',
        '''#include "prelude.h"
#ifdef FE7_NATIVE_PORT
#include "gba_port.h"
#endif

#define VRAM_ADDR 0x06000000
#define VRAM_SIZE 0x18000
#ifdef FE7_NATIVE_PORT
#define VRAM FE7_GBA_VRAM_PTR
#else
#define VRAM ((void *)VRAM_ADDR)
#endif

#define PLTT_ADDR 0x05000000
#define PLTT_SIZE 0x400
#ifdef FE7_NATIVE_PORT
#define PLTT FE7_GBA_PLTT_PTR
#else
#define PLTT ((void *)PLTT_ADDR)
#endif

#define OAM_ADDR 0x07000000
#define OAM_SIZE 0x400
#ifdef FE7_NATIVE_PORT
#define OAM FE7_GBA_OAM_PTR
#else
#define OAM ((void *)OAM_ADDR)
#endif''',
    )
    changed += replace_once(
        inc / "hardware.h",
        '#include "prelude.h"\n\n// TODO: move to gbasomething\n#define VRAM (void *)0x06000000',
        '''#include "prelude.h"
#ifdef FE7_NATIVE_PORT
#include "gba_port.h"
#endif

// TODO: move to gbasomething
#ifdef FE7_NATIVE_PORT
#define VRAM FE7_GBA_VRAM_PTR
#else
#define VRAM (void *)0x06000000
#endif''',
    )
    print(f"Native memory-header adapter applied to {root} ({changed} edits).")
    print("Backups use the .fe7native.bak suffix. This is not a game-engine build or a ROM boot test.")
    print("Compile with FE7_NATIVE_PORT defined and include app/src/main/cpp for the native API.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
