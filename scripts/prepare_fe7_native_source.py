#!/usr/bin/env python3
"""Safely apply the first FE7-to-native memory/BIOS compatibility layer.

Usage: python3 scripts/prepare_fe7_native_source.py /path/to/fe7_us-main
It keeps .fe7native.bak copies and is idempotent. It does not port the GBA
assembly or claim that the whole engine is ready to link or run.
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
def wrap_dma_macro(path: Path) -> bool:
    """Swap FE7's register-programming DMA macro for the host software DMA hook."""
    source = path.read_text(encoding="utf-8")
    if "fe7_gba_dma_set((dma_num)" in source:
        return False
    lines = source.splitlines(keepends=True)
    for start, line in enumerate(lines):
        if line.startswith("#define DmaSet(dma_num, src, dst, control)"):
            end = start
            while lines[end].rstrip().endswith(chr(92)):
                end += 1
                if end >= len(lines):
                    raise RuntimeError(f"unterminated DmaSet macro in {path}")
            original = "".join(lines[start:end + 1])
            replacement = (
                "#ifdef FE7_NATIVE_PORT\n"
                "#define DmaSet(dma_num, src, dst, control) fe7_gba_dma_set((dma_num), (const void *)(src), (void *)(dst), (u32)(control))\n"
                "#else\n" + original +
                "#endif // FE7_NATIVE_PORT\n"
            )
            updated = "".join(lines[:start]) + replacement + "".join(lines[end + 1:])
            backup = path.with_name(path.name + ".fe7native.bak")
            if not backup.exists():
                shutil.copy2(path, backup)
            path.write_text(updated, encoding="utf-8")
            return True
    raise RuntimeError(f"could not locate DmaSet macro in {path}")


def wrap_interrupt_source(path: Path) -> bool:
    """Replace ARM7TDMI IntrMain assembly assumptions with a C IRQ dispatch path."""
    source = path.read_text(encoding="utf-8")
    if "fe7_gba_dispatch_pending_irqs" in source:
        return False
    marker = "extern void IntrMain(void);"
    if marker not in source:
        raise RuntimeError(f"could not locate IntrMain declaration in {path}")
    native = """#ifdef FE7_NATIVE_PORT
#include "gba_port.h"

void (*IntrFuncTable[14])(void);
u32 IntrMainRam[0x200];
void DummyIntFunc(void);

void IntrMain(void)
{
    fe7_gba_dispatch_pending_irqs();
}

void DummyIntFunc(void)
{
}

void IrqInit(void)
{
    int i;
    for (i = 0; i < 14; ++i)
    {
        IntrFuncTable[i] = DummyIntFunc;
        fe7_gba_set_irq_handler((unsigned)i, DummyIntFunc);
    }
    REG_IME = 0;
    REG_IE = 0;
    fe7_gba_irq_clear(0x3FFF);
    INTR_VECTOR = IntrMainRam;
}

void SetIrqFunc(int num, void (*func)(void))
{
    if (num < 0 || num >= 14)
        return;
    if (func == NULL)
        func = DummyIntFunc;
    IntrFuncTable[num] = func;
    fe7_gba_set_irq_handler((unsigned)num, func);
}
#else
"""
    updated = source.replace(marker, native + marker, 1) + "\n#endif // FE7_NATIVE_PORT\n"
    backup = path.with_name(path.name + ".fe7native.bak")
    if not backup.exists():
        shutil.copy2(path, backup)
    path.write_text(updated, encoding="utf-8")
    return True



def wrap_agb_main(path: Path) -> bool:
    """Split FE7's one-time AgbMain initialization from its infinite main loop."""
    source = path.read_text(encoding="utf-8")
    if "void Fe7EngineRunFrame(void)" in source:
        return False
    old_loop = """    for (;;)
    {
        RunMainFunc();
        SoftResetIfKeyCombo();
    }
}

void PutBuildInfo"""
    new_loop = """#ifdef FE7_NATIVE_PORT
    // The Android host owns frame pacing and calls Fe7EngineRunFrame().
    return;
#else
    for (;;)
    {
        RunMainFunc();
        SoftResetIfKeyCombo();
    }
#endif
}

#ifdef FE7_NATIVE_PORT
void Fe7EngineRunFrame(void)
{
    RunMainFunc();
    SoftResetIfKeyCombo();
}
#endif

void PutBuildInfo"""
    if old_loop not in source:
        raise RuntimeError(f"could not locate AgbMain main loop in {path}")
    backup = path.with_name(path.name + ".fe7native.bak")
    if not backup.exists():
        shutil.copy2(path, backup)
    path.write_text(source.replace(old_loop, new_loop, 1), encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path, help="extracted FE7 decompilation root")
    args = parser.parse_args()
    root = args.source_root.resolve()
    inc = root / "include"
    targets = [inc / "gbaio.h", inc / "gbavideo.h", inc / "hardware.h", root / "src" / "gbasram.c", root / "src" / "main.c"]
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
#endif''')
    changed += replace_once(
        inc / "gbaio.h",
        '#define INTR_VECTOR (*(void **)0x03007FFC)',
        '''#ifdef FE7_NATIVE_PORT
#define INTR_VECTOR (*fe7_gba_interrupt_vector_slot())
#else
#define INTR_VECTOR (*(void **)0x03007FFC)
#endif''')
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
#endif''')
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
#endif''')
    sram_source = root / "src" / "gbasram.c"
    changed += replace_once(
        sram_source,
        "u32 (*COMMON(VerifySramFast) VerifySramFast)(void const * src, void * dst, u32 size) = NULL;",
        """#ifdef FE7_NATIVE_PORT
uptr (*COMMON(VerifySramFast) VerifySramFast)(void const * src, void * dst, u32 size) = NULL;
#else
u32 (*COMMON(VerifySramFast) VerifySramFast)(void const * src, void * dst, u32 size) = NULL;
#endif""")
    changed += replace_once(
        sram_source,
        "void SetSramFastFunc(void)\n{\n    u16 * src;",
        """void SetSramFastFunc(void)
{
#ifdef FE7_NATIVE_PORT
    // ARM7TDMI machine-code blobs copied into writable/executable RAM cannot be
    // called on Android; native builds use the equivalent C implementations.
    ReadSramFast = (void (*)(void const *, void *, u32))ReadSramFastRom;
    VerifySramFast = (uptr (*)(void const *, void *, u32))VerifySramFastRom;
#else
    u16 * src;""")
    changed += replace_once(
        sram_source,
        "    VerifySramFast = (void *)((uptr)VerifySramFastBuf + 1);\n\n    REG_WAITCNT = (REG_WAITCNT & ~3) | 3;\n}",
        """    VerifySramFast = (void *)((uptr)VerifySramFastBuf + 1);

    REG_WAITCNT = (REG_WAITCNT & ~3) | 3;
#endif
}""")
    changed += wrap_dma_macro(inc / "gbaio.h")
    changed += wrap_interrupt_source(root / "src" / "interrupt.c")
    changed += wrap_agb_main(root / "src" / "main.c")
    print(f"Native memory-header adapter applied to {root} ({changed} edits).")
    print("Backups use the .fe7native.bak suffix. This is not a game-engine build or ROM boot test.")
    print("AgbMain is split into initialization plus Fe7EngineRunFrame() for host-owned pacing.")
    print("Compile with FE7_NATIVE_PORT defined and include app/src/main/cpp for the native API.")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
