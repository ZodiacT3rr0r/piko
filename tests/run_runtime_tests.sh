#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: tests/run_runtime_tests.sh <piko_binary>" >&2
    exit 2
fi

PIKO_BIN="$1"
CASES_DIR="tests/runtime"

if [ ! -x "$PIKO_BIN" ]; then
    echo "error: binary not executable: $PIKO_BIN" >&2
    exit 1
fi

for src in "$CASES_DIR"/*.piko; do
    expected="${src%.piko}.out"
    actual="${src%.piko}.actual"

    if [ ! -f "$expected" ]; then
        echo "error: missing expected runtime snapshot: $expected" >&2
        exit 1
    fi

    "$PIKO_BIN" "$src" > "$actual"

    if ! diff -u "$expected" "$actual"; then
        echo "runtime snapshot mismatch for: $src" >&2
        rm -f "$actual"
        exit 1
    fi

    rm -f "$actual"
    echo "ok: $src"
done
