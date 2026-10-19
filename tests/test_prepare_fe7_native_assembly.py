#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
SCRIPT = REPO / "scripts" / "prepare_fe7_native_assembly.py"
SPEC = importlib.util.spec_from_file_location("prepare_fe7_native_assembly", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class AssemblyPreparationTest(unittest.TestCase):
    def test_removes_only_adjacent_duplicate_local_label(self):
        source = (
            ".L00000010:\n"
            "    nop\n"
            ".L00000020:\n"
            ".L00000020:\n"
            "    bx lr\n"
        )
        result, removed = MODULE.normalize(source)
        self.assertEqual(removed, 1)
        self.assertEqual(result, ".L00000010:\n    nop\n.L00000020:\n    bx lr\n")

    def test_rejects_non_adjacent_duplicate_label(self):
        with self.assertRaises(ValueError):
            MODULE.normalize(".L00000010:\n    nop\n.L00000010:\n")

    def test_supplied_code_source_needs_one_normalization(self):
        source_path = Path("/mnt/data/fe7src/fe7_us-main/asm/code.s")
        if not source_path.is_file():
            self.skipTest("user-supplied FE7 source archive is not available in this workspace")
        result, removed = MODULE.normalize(source_path.read_text(encoding="utf-8"))
        self.assertEqual(removed, 1)
        self.assertNotIn(".L08053496:\n.L08053496:\n", result)


if __name__ == "__main__":
    unittest.main(verbosity=2)
