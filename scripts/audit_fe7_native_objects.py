#!/usr/bin/env python3
"""Audit undefined symbols and relocations in compiled FE7 native-port objects.

This produces evidence for native-link integration; it does not claim that
objects are safe to link or that AgbMain can execute on Android.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import subprocess
import sys


def run_tool(command: list[str]) -> str:
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stderr.strip()}"
        )
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", required=True, help="path to llvm-nm")
    parser.add_argument("--readelf", required=True, help="path to llvm-readelf")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("objects", nargs="+", type=Path)
    args = parser.parse_args()

    missing = [str(path) for path in args.objects if not path.is_file()]
    if missing:
        parser.error("object file(s) not found: " + ", ".join(missing))

    lines = [
        "# FE7 native object link audit",
        "",
        "This report inventories undefined symbols and relocations in compile-probe",
        "objects. Undefined symbols may be satisfied by other objects; relocation",
        "types/targets must be reviewed before attempting an Android shared-library link.",
        "",
        f"Objects audited: {len(args.objects)}",
        "",
    ]
    undefined_totals: Counter[str] = Counter()
    relocation_totals: Counter[str] = Counter()
    failures: list[str] = []

    for obj in sorted(args.objects, key=lambda p: str(p)):
        nm_output = run_tool([args.nm, "-u", str(obj)])
        undefined = sorted({
            line.split()[-1]
            for line in nm_output.splitlines()
            if line.split()
        })
        reloc_output = run_tool([args.readelf, "-rW", str(obj)])
        relocations: Counter[str] = Counter()
        for line in reloc_output.splitlines():
            fields = line.split()
            if len(fields) >= 3 and fields[0].startswith("0") and fields[2].startswith("R_"):
                relocations[fields[2]] += 1

        undefined_totals.update(undefined)
        relocation_totals.update(relocations)
        lines.extend([f"## {obj}", "", f"Undefined symbols ({len(undefined)} unique):"])
        lines.extend([f"- `{symbol}`" for symbol in undefined] or ["- None"])
        lines.extend(["", "Relocation types:"])
        lines.extend(
            [f"- `{kind}`: {count}" for kind, count in sorted(relocations.items())]
            or ["- None"]
        )
        lines.append("")

    lines.extend([
        "## Aggregate unresolved symbol references",
        "",
        "Counts below are per-object references, not proof that a symbol is globally unresolved.",
        "",
    ])
    lines.extend(
        f"- `{symbol}`: {count}"
        for symbol, count in undefined_totals.most_common()
    )
    lines.extend(["", "## Aggregate relocation types", ""])
    lines.extend(
        f"- `{kind}`: {count}"
        for kind, count in sorted(relocation_totals.items())
    )
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(
        f"Wrote {args.output}: {len(args.objects)} objects, "
        f"{len(undefined_totals)} distinct undefined symbols, "
        f"{sum(relocation_totals.values())} relocations."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as exc:
        print(f"FE7 object audit failed: {exc}", file=sys.stderr)
        raise SystemExit(2)
