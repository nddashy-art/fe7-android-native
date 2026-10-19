#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 /path/to/fe7_us-main" >&2
    exit 2
fi

SOURCE_ROOT=$(CDPATH= cd -- "$1" && pwd)
INCLUDE_DIR="$SOURCE_ROOT/include"
SOURCE_DIR="$SOURCE_ROOT/src"
CC="${CC:-cc}"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/fe7-native-data-slice.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

if [ ! -f "$INCLUDE_DIR/prelude.h" ] || [ ! -d "$SOURCE_DIR" ]; then
    echo "Expected include/prelude.h and src/ in: $SOURCE_ROOT" >&2
    exit 2
fi

UNITS="util text face unit bm map sprite icon msg debug_text banim_sprite"
count=0
for unit in $UNITS; do
    source="$SOURCE_DIR/$unit.c"
    if [ ! -f "$source" ]; then
        echo "Missing FE7 translation unit: $source" >&2
        exit 2
    fi
    "$CC" -std=gnu11 -fPIC -fno-common -Wall -Wextra \
        -I"$INCLUDE_DIR" -DFE7_NATIVE_PORT=1 \
        -c "$source" -o "$TMP_DIR/$unit.o"
    count=$((count + 1))
done

# This catches duplicate definitions and object-format/link issues beyond syntax-only tests.
"$CC" -shared "$TMP_DIR"/*.o -o "$TMP_DIR/libfe7-native-data-slice.so"
echo "FE7 native data-slice smoke test passed: compiled and linked $count source translation units."
