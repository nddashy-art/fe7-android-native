#!/usr/bin/env python3
"""Compile and link the first ROM-independent slice of FE7's own C source."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

UNITS = (
    "util", "text", "face", "unit", "bm", "map",
    "sprite", "icon", "msg", "debug_text", "banim_sprite",
)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} /path/to/fe7_us-main", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    include = root / "include"
    source_dir = root / "src"
    if not (include / "prelude.h").is_file() or not source_dir.is_dir():
        print(f"Expected include/prelude.h and src/ under: {root}", file=sys.stderr)
        return 2

    compiler = os.environ.get("CC", "cc")
    if shutil.which(compiler) is None:
        print(f"C compiler not found: {compiler}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="fe7-native-data-slice-") as tmp:
        output = Path(tmp)
        objects: list[Path] = []
        for unit in UNITS:
            source = source_dir / f"{unit}.c"
            if not source.is_file():
                print(f"Missing FE7 translation unit: {source}", file=sys.stderr)
                return 2
            obj = output / f"{unit}.o"
            command = [
                compiler, "-std=gnu11", "-fPIC", "-fno-common",
                "-Wall", "-Wextra", "-DFE7_NATIVE_PORT=1",
                f"-I{include}", "-c", str(source), "-o", str(obj),
            ]
            subprocess.run(command, check=True)
            objects.append(obj)

        shared = output / "libfe7-native-data-slice.so"
        subprocess.run(
            [compiler, "-shared", *(str(obj) for obj in objects), "-o", str(shared)],
            check=True,
        )
        if not shared.is_file() or shared.stat().st_size == 0:
            print("Shared-object link did not produce output.", file=sys.stderr)
            return 1

    print(f"FE7 native data-slice smoke test passed: compiled and linked {len(UNITS)} source translation units.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Native source compile/link failed (exit {exc.returncode}).", file=sys.stderr)
        raise SystemExit(exc.returncode or 1)
