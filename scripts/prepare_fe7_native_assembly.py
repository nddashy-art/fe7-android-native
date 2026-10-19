#!/usr/bin/env python3
"""Create an Android-assembler-safe copy of FE7 code.s without editing the source tree.

The supplied snapshot contains one adjacent duplicate local label at the same
address. Removing the duplicate declaration does not change any instruction or
symbol address; all other assembly source text is preserved.
"""
from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import sys

LABEL = re.compile(r"^(\.L[0-9A-Fa-f]+):\s*$")


def normalize(source_text: str) -> tuple[str, int]:
    lines = source_text.splitlines(keepends=True)
    output: list[str] = []
    removed = 0
    for line in lines:
        current = line.rstrip("\r\n")
        previous = output[-1].rstrip("\r\n") if output else ""
        current_match = LABEL.match(current)
        previous_match = LABEL.match(previous)
        if current_match and previous_match and current_match.group(1) == previous_match.group(1):
            removed += 1
            continue
        output.append(line)

    result = "".join(output)
    labels = [
        match.group(1)
        for line in result.splitlines()
        if (match := LABEL.match(line)) is not None
    ]
    duplicates = [name for name, count in Counter(labels).items() if count > 1]
    if duplicates:
        raise ValueError(
            "cannot safely normalize non-adjacent duplicate labels: "
            + ", ".join(duplicates[:8])
        )
    if removed == 0:
        # An already normalized copy is valid and remains unchanged.
        return result, 0
    return result, removed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    try:
        original = args.source.read_text(encoding="utf-8")
        normalized, removed = normalize(original)
        args.destination.parent.mkdir(parents=True, exist_ok=True)
        args.destination.write_text(normalized, encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"assembly normalization failed: {exc}", file=sys.stderr)
        return 2

    print(
        f"Prepared Android assembler copy: {args.destination} "
        f"({removed} adjacent duplicate label declaration(s) removed)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
