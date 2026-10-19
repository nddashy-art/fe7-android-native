#!/usr/bin/env python3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PATCHER = REPO / "scripts" / "prepare_fe7_native_source.py"

class PrepareSourceAdapterTest(unittest.TestCase):
    def test_idempotent_and_preserves_backups(self):
        with tempfile.TemporaryDirectory(prefix="fe7-native-source-test-") as temp:
            root = Path(temp) / "fe7_us-main"
            include, source = root / "include", root / "src"
            include.mkdir(parents=True)
            source.mkdir()
            (include / "gbaio.h").write_text(
                '#ifndef GBAIO_H\n#define GBAIO_H\n#include "prelude.h"\n\n'
                '#define REG(type, addr) (*((type volatile *)(addr)))\n'
                '#define INTR_VECTOR (*(void **)0x03007FFC)\n'
                '#define DmaSet(dma_num, src, dst, control) ((void)0)\n#endif\n', encoding="utf-8")
            (include / "gbavideo.h").write_text(
                '#ifndef GBAVIDEO_H\n#define GBAVIDEO_H\n#include "prelude.h"\n\n'
                '#define VRAM_ADDR 0x06000000\n#define VRAM_SIZE 0x18000\n'
                '#define VRAM ((void *)VRAM_ADDR)\n\n#define PLTT_ADDR 0x05000000\n'
                '#define PLTT_SIZE 0x400\n#define PLTT ((void *)PLTT_ADDR)\n\n'
                '#define OAM_ADDR 0x07000000\n#define OAM_SIZE 0x400\n'
                '#define OAM ((void *)OAM_ADDR)\n#endif\n', encoding="utf-8")
            (include / "hardware.h").write_text(
                '#ifndef HARDWARE_H\n#define HARDWARE_H\n#include "prelude.h"\n\n'
                '// TODO: move to gbasomething\n#define VRAM (void *)0x06000000\n#endif\n', encoding="utf-8")
            original_sram = (
                'u32 (*COMMON(VerifySramFast) VerifySramFast)(void const * src, void * dst, u32 size) = NULL;\n'
                'void SetSramFastFunc(void)\n{\n    u16 * src;\n'
                '    VerifySramFast = (void *)((uptr)VerifySramFastBuf + 1);\n\n'
                '    REG_WAITCNT = (REG_WAITCNT & ~3) | 3;\n}\n')
            (source / "gbasram.c").write_text(original_sram, encoding="utf-8")
            original_interrupt = (
                '#include "prelude.h"\n#include "gbaio.h"\n#include "gbasvc.h"\n\n'
                'extern void IntrMain(void);\nvoid DummyIntFunc(void);\n'
                'extern void (*IntrFuncTable[])(void);\nextern u32 IntrMainRam[0x200];\n'
                'void IrqInit(void) { IntrFuncTable[0] = DummyIntFunc; }\n'
                'void DummyIntFunc(void) {}\n'
                'void SetIrqFunc(int num, void (*func)(void)) { IntrFuncTable[num] = func; }\n'
            )
            (source / "interrupt.c").write_text(original_interrupt, encoding="utf-8")
            original_main = (
                'void AgbMain(void)\\n{\\n    InitGame();\\n    for (;;)\\n'
                '    {\\n        RunMainFunc();\\n        SoftResetIfKeyCombo();\\n'
                '    }\\n}\\n\\nvoid PutBuildInfo(u16 * tm)\\n{\\n}\\n'
            )
            (source / "main.c").write_text(original_main, encoding="utf-8")
            original_gbaio = (include / "gbaio.h").read_bytes()
            original_sram_bytes = (source / "gbasram.c").read_bytes()
            original_interrupt_bytes = (source / "interrupt.c").read_bytes()
            original_main_bytes = (source / "main.c").read_bytes()

            first = subprocess.run([sys.executable, str(PATCHER), str(root)],
                                   check=True, capture_output=True, text=True)
            second = subprocess.run([sys.executable, str(PATCHER), str(root)],
                                    check=True, capture_output=True, text=True)

            patched = (include / "gbaio.h").read_text(encoding="utf-8")
            self.assertIn("fe7_gba_io_pointer", patched)
            self.assertIn("fe7_gba_interrupt_vector_slot", patched)
            self.assertIn("fe7_gba_dma_set((dma_num)", patched)
            video = (include / "gbavideo.h").read_text(encoding="utf-8")
            self.assertIn("FE7_GBA_VRAM_PTR", video)
            self.assertIn("FE7_GBA_PLTT_PTR", video)
            self.assertIn("FE7_GBA_OAM_PTR", video)
            sram_patched = (source / "gbasram.c").read_text(encoding="utf-8")
            self.assertIn("ReadSramFast = (void (*)(void const *, void *, u32))ReadSramFastRom;", sram_patched)
            self.assertIn("#else\n    u16 * src;", sram_patched)
            self.assertEqual((include / "gbaio.h.fe7native.bak").read_bytes(), original_gbaio)
            self.assertEqual((source / "gbasram.c.fe7native.bak").read_bytes(), original_sram_bytes)
            interrupt_patched = (source / "interrupt.c").read_text(encoding="utf-8")
            self.assertIn("fe7_gba_dispatch_pending_irqs", interrupt_patched)
            self.assertIn("#else\nextern void IntrMain(void);", interrupt_patched)
            self.assertEqual((source / "interrupt.c.fe7native.bak").read_bytes(), original_interrupt_bytes)
            main_patched = (source / "main.c").read_text(encoding="utf-8")
            self.assertIn("void Fe7EngineRunFrame(void)", main_patched)
            self.assertIn("#ifdef FE7_NATIVE_PORT", main_patched)
            self.assertEqual((source / "main.c.fe7native.bak").read_bytes(), original_main_bytes)
            self.assertIn("(10 edits)", first.stdout)
            self.assertIn("(0 edits)", second.stdout)
            self.assertTrue((include / "gba_port.h").is_file())

if __name__ == "__main__":
    unittest.main(verbosity=2)
