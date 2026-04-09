#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: tests/run_lexer_tests.sh <piko_binary>" >&2
    exit 2
fi

PIKO_BIN="$1"
CASES_DIR="tests/lexer"

if [ ! -x "$PIKO_BIN" ]; then
    echo "error: binary not executable: $PIKO_BIN" >&2
    exit 1
fi

for src in "$CASES_DIR"/*.piko; do
    expected="${src%.piko}.tokens"
    actual="${src%.piko}.actual"

    if [ ! -f "$expected" ]; then
        echo "error: missing expected token snapshot: $expected" >&2
        exit 1
    fi

    "$PIKO_BIN" --lex "$src" > "$actual"

    if ! diff -u "$expected" "$actual"; then
        echo "lexer snapshot mismatch for: $src" >&2
        rm -f "$actual"
        exit 1
    fi

    rm -f "$actual"
    echo "ok: $src"
done
