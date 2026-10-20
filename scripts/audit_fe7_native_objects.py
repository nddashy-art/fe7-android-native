#!/usr/bin/env python3
"""Audit FE7 object symbols and relocations, grouped by Android ABI.

This is an object-level inventory, not proof that the engine links or boots.
Undefined symbols are resolved against other audited objects from the same ABI
only; Android/system libraries and fe7runtime are outside this object set.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
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


def symbols_from_nm(output: str) -> set[str]:
    symbols: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        if fields:
            symbol = fields[-1]
            if symbol and symbol not in {"U", "u"}:
                symbols.add(symbol)
    return symbols


def abi_for(obj: Path) -> str:
    for abi in ("arm64-v8a", "armeabi-v7a", "x86_64", "x86"):
        if abi in obj.parts:
            return abi
    return "unknown-abi"


def symbol_category(symbol: str) -> str:
    if symbol.startswith("fe7_gba_"):
        return "GBA bridge/runtime"
    if symbol.startswith(("__", "_call_via_")):
        return "compiler/runtime support"
    if symbol.startswith("m4a") or symbol.startswith(("Sound", "MPlay", "ply_")):
        return "audio engine"
    if symbol.startswith("g") and len(symbol) > 1 and symbol[1].isupper():
        return "global game data"
    if symbol.startswith("func_"):
        return "unnamed/game-address function"
    return "game/API function or data"


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
        "Undefined symbols are resolved against definitions in the audited object set",
        "for the same Android ABI only. Symbols provided by fe7runtime, Android/system",
        "libraries, ROM-backed data, or un-audited objects may still need separate work.",
        "Relocation totals are diagnostic and do not alone determine link safety.",
        "",
        f"Objects audited: {len(args.objects)}",
        "",
    ]
    undefined_totals: Counter[str] = Counter()
    relocation_totals: Counter[str] = Counter()
    undefined_by_abi: dict[str, Counter[str]] = defaultdict(Counter)
    defined_by_abi: dict[str, set[str]] = defaultdict(set)
    object_rows: list[tuple[Path, str, set[str], Counter[str]]] = []

    for obj in sorted(args.objects, key=lambda p: str(p)):
        abi = abi_for(obj)
        undefined = symbols_from_nm(run_tool([args.nm, "-u", str(obj)]))
        defined = symbols_from_nm(run_tool([args.nm, "-g", "--defined-only", str(obj)]))
        reloc_output = run_tool([args.readelf, "-rW", str(obj)])
        relocations: Counter[str] = Counter()
        for line in reloc_output.splitlines():
            fields = line.split()
            if len(fields) >= 3 and fields[0].startswith("0") and fields[2].startswith("R_"):
                relocations[fields[2]] += 1

        undefined_totals.update(undefined)
        relocation_totals.update(relocations)
        undefined_by_abi[abi].update(undefined)
        defined_by_abi[abi].update(defined)
        object_rows.append((obj, abi, undefined, relocations))

    for obj, abi, undefined, relocations in object_rows:
        lines.extend([
            f"## {obj}",
            "",
            f"ABI: {abi}",
            "",
            f"Undefined symbols ({len(undefined)} unique):",
        ])
        lines.extend([f"- {symbol}" for symbol in sorted(undefined)] or ["- None"])
        lines.extend(["", "Relocation types:"])
        lines.extend(
            [f"- {kind}: {count}" for kind, count in sorted(relocations.items())]
            or ["- None"]
        )
        lines.append("")

    lines.extend(["## Cross-object unresolved candidates by ABI", ""])
    lines.append(
        "These symbols are undefined in at least one audited object and have no global "
        "definition in the audited objects for that same ABI. They may still be supplied "
        "by fe7runtime, Android/system libraries, or omitted source/data objects."
    )
    lines.append("")
    unresolved_total = 0
    for abi in sorted(undefined_by_abi):
        unresolved = Counter({
            symbol: count
            for symbol, count in undefined_by_abi[abi].items()
            if symbol not in defined_by_abi[abi]
        })
        unresolved_total += len(unresolved)
        lines.extend([f"### {abi}", ""])
        if not unresolved:
            lines.append("- None")
        else:
            by_category: dict[str, Counter[str]] = defaultdict(Counter)
            for symbol, count in unresolved.items():
                by_category[symbol_category(symbol)][symbol] = count
            for category in sorted(by_category):
                lines.extend([f"**{category}**", ""])
                lines.extend(
                    f"- {symbol}: {count}"
                    for symbol, count in by_category[category].most_common()
                )
                lines.append("")

    lines.extend([
        "## Aggregate per-object undefined references",
        "",
        "These counts include symbols later defined by another audited object and are not",
        "the same as the unresolved-candidate section above.",
        "",
    ])
    lines.extend(
        f"- {symbol}: {count}"
        for symbol, count in undefined_totals.most_common()
    )
    lines.extend(["", "## Aggregate relocation types", ""])
    lines.extend(
        f"- {kind}: {count}"
        for kind, count in sorted(relocation_totals.items())
    )
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(
        f"Wrote {args.output}: {len(args.objects)} objects, "
        f"{len(undefined_totals)} distinct undefined symbols, "
        f"{sum(relocation_totals.values())} relocations, "
        f"{unresolved_total} ABI-specific unresolved candidates."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as exc:
        print(f"FE7 object audit failed: {exc}", file=sys.stderr)
        raise SystemExit(2)
