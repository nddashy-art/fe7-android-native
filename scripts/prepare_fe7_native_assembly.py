#!/usr/bin/env python3
"""Prepare FE7 ARM assembly for the Android build without editing upstream sources.

This removes adjacent duplicate local labels and can omit routines for which a
portable native C++ implementation is supplied.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import sys

LABEL = re.compile(r"^(\.L[0-9A-Fa-f]+):\s*$")


def normalize(source_text: str, omit_symbols: set[str] | None = None) -> tuple[str, int]:
    omit_symbols = omit_symbols or set()
    lines = source_text.splitlines(keepends=True)
    output: list[str] = []
    removed = 0
    omitted: list[str] = []
    skipping: str | None = None
    start_re = re.compile(r"^\s*(?:thumb_func_start|func_start)\s+([A-Za-z_][A-Za-z0-9_]*)\s*$")
    end_re = re.compile(r"^\s*func_end\s+([A-Za-z_][A-Za-z0-9_]*)\s*$")
    for line in lines:
        current = line.rstrip("\r\n")
        if skipping is not None:
            match = end_re.match(current)
            if match and match.group(1) == skipping:
                skipping = None
            continue
        match = start_re.match(current)
        if match and match.group(1) in omit_symbols:
            skipping = match.group(1)
            omitted.append(skipping)
            continue
        previous = output[-1].rstrip("\r\n") if output else ""
        current_match = LABEL.match(current)
        previous_match = LABEL.match(previous)
        if current_match and previous_match and current_match.group(1) == previous_match.group(1):
            removed += 1
            continue
        output.append(line)
    if skipping is not None:
        raise ValueError(f"missing func_end for omitted routine {skipping}")
    missing = sorted(omit_symbols - set(omitted))
    if missing:
        raise ValueError("requested routine(s) not found: " + ", ".join(missing))
    result = "".join(output)
    labels = [match.group(1) for line in result.splitlines() if (match := LABEL.match(line)) is not None]
    duplicates = [name for name, count in Counter(labels).items() if count > 1]
    if duplicates:
        raise ValueError("cannot safely normalize non-adjacent duplicate labels: " + ", ".join(duplicates[:8]))
    return result, removed

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--omit-symbol", action="append", default=[], metavar="SYMBOL")
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    try:
        original = args.source.read_text(encoding="utf-8")
        normalized, removed = normalize(original, set(args.omit_symbol))
        args.destination.parent.mkdir(parents=True, exist_ok=True)
        args.destination.write_text(normalized, encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"assembly normalization failed: {exc}", file=sys.stderr)
        return 2

    omitted_text = ", ".join(args.omit_symbol) if args.omit_symbol else "none"
    print(f"Prepared Android assembler copy: {args.destination} ({removed} duplicate local label(s) removed; omitted: {omitted_text}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
