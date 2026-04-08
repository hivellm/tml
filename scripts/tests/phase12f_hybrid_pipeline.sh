#!/usr/bin/env bash
# Integration test for phase12f hybrid pipeline (--stage=lexer:tml).
#
# Verifies:
#   1. `tml build --stage=lexer:tml` compiles hello.tml via TML lexer subprocess.
#   2. The resulting exe runs and prints "hello".
#   3. IR emitted by the hybrid pipeline is bitwise-identical to the pure C++
#      pipeline (THIR + MIR + LLVM IR text).
#
# Usage: scripts/tests/phase12f_hybrid_pipeline.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TML="$ROOT/build/debug/bin/tml.exe"
SRC="$ROOT/.sandbox/hello.tml"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$TML" ]]; then
    echo "tml.exe not built at $TML — run scripts/build.bat first" >&2
    exit 2
fi
if [[ ! -f "$SRC" ]]; then
    echo 'func main() { println("hello") }' > "$SRC"
fi

echo "[1/3] Baseline C++ pipeline"
"$TML" build "$SRC" --out-dir="$TMP/base" --emit-pipeline="$TMP/base/pipe" >/dev/null
BASE_EXE="$TMP/base/hello.exe"
BASE_OUT="$("$BASE_EXE")"
[[ "$BASE_OUT" == "hello" ]] || { echo "baseline exe wrong output: $BASE_OUT" >&2; exit 1; }

echo "[2/3] Hybrid pipeline (--stage=lexer:tml)"
"$TML" build "$SRC" --stage=lexer:tml --out-dir="$TMP/hyb" --emit-pipeline="$TMP/hyb/pipe" >/dev/null
HYB_EXE="$TMP/hyb/hello.exe"
HYB_OUT="$("$HYB_EXE")"
[[ "$HYB_OUT" == "hello" ]] || { echo "hybrid exe wrong output: $HYB_OUT" >&2; exit 1; }

echo "[3/3] IR-diff (THIR + MIR + LLVM IR)"
for name in hello.thir hello.mir.pre hello.mir.post hello.ll; do
    b="$TMP/base/pipe/$name"
    h="$TMP/hyb/pipe/$name"
    [[ -f "$b" && -f "$h" ]] || { echo "missing $name emission" >&2; exit 1; }
    diff -q "$b" "$h" >/dev/null || { echo "$name differs between baseline and hybrid" >&2; diff "$b" "$h" | head; exit 1; }
done

echo "OK — phase12f hybrid pipeline bitwise-identical"
