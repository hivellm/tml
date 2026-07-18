#!/usr/bin/env bash
# CLI test for phase42b_cache-staleness-correctness (F-027..F-030).
#
# Closes four under-invalidation holes that let the toolchain serve stale
# results until a cache dir was manually nuked:
#
#   F-027  A `<source>.tml.ast.bin` sidecar was trusted unconditionally, so a
#          stale cached AST silently overrode an edited source. Now the sidecar
#          is used only when it is at least as new as the source.
#   F-028  The warm daemon result cache keyed only on argv `.tml` mtimes, so
#          editing an IMPORTED module returned the previous diagnostics. Now the
#          transitive source universe (lib tree + argv-sibling dir) is checked.
#   F-029  The daemon's meta preload was call_once/process; project-local
#          modules already self-validate (proven below), so a stale-RESULT
#          divergence needs a lib/ SOURCE edit — out of scope for a CLI test.
#          The defensive fix (lib-change probe that drops GlobalModuleCache) is
#          exercised indirectly; this section is informational.
#   F-030  `tml cache invalidate <src>` deleted nothing (substring-matched
#          hash-named files). Now it removes the source's `.ast.bin` sidecar,
#          its tests.json suite entry + EXE, and its incr entries + ir files,
#          reporting exactly what was removed.
#
# Usage:
#     bash compiler/tests/cli/cache_staleness.sh
#
# Exits non-zero on any failure. The daemon sections are Windows-only (named
# pipe transport); they self-skip elsewhere.

set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SANDBOX=".sandbox/cache_staleness_test"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi
TML_ABS="$(cd "$(dirname "$TML")" && pwd)/$(basename "$TML")"

rm -rf "$SANDBOX"; mkdir -p "$SANDBOX"

pass=0
fail=0
check() {
    local name="$1"; shift
    if "$@"; then
        printf '  PASS  %s\n' "$name"; pass=$((pass + 1))
    else
        printf '  FAIL  %s\n' "$name"; fail=$((fail + 1))
    fi
}

cleanup() {
    ( cd "$SANDBOX/proj" 2>/dev/null && "$TML_ABS" daemon stop >/dev/null 2>&1 ) || true
}
trap cleanup EXIT

is_windows() { case "$(uname -s 2>/dev/null)" in MINGW*|MSYS*|CYGWIN*) return 0;; *) return 1;; esac; }

# ----------------------------------------------------------------------------
# F-027: a stale/invalid .ast.bin sidecar must not override a newer source.
# ----------------------------------------------------------------------------
printf 'func main() -> I64 {\n    return 7\n}\n' > "$SANDBOX/r.tml"
printf 'GARBAGE-NOT-A-VALID-AST\x00\x01\x02' > "$SANDBOX/r.tml.ast.bin"
sleep 1
printf 'func main() -> I64 {\n    return 7\n}\n' > "$SANDBOX/r.tml"   # now newer
TML_DAEMON=0 "$TML" build "$SANDBOX/r.tml" > "$SANDBOX/f027.out" 2>&1 || true
check "F-027 newer source ignores stale .ast.bin sidecar" \
    bash -c "! grep -q 'ast.bin deserialization' '$SANDBOX/f027.out'"

# ----------------------------------------------------------------------------
# F-028 / F-029: warm-daemon staleness on an imported sibling module.
# (Sibling `use` resolves relative to the entry file's directory, so the daemon
#  and client run from inside the project dir.)
# ----------------------------------------------------------------------------
if is_windows; then
    mkdir -p "$SANDBOX/proj"
    printf 'pub func val() -> I64 { return 1 }\n' > "$SANDBOX/proj/helper.tml"
    printf 'use helper::val\n\n@test("t")\nfunc t() {\n    let x: I64 = val()\n    assert(x == 1)\n}\n' > "$SANDBOX/proj/app.tml"

    (
        cd "$SANDBOX/proj" || exit 1
        "$TML_ABS" daemon stop  >/dev/null 2>&1
        "$TML_ABS" daemon start >/dev/null 2>&1
        sleep 1
        TML_DAEMON=1 "$TML_ABS" check app.tml > w1.out 2>&1; echo $? > w1.rc
        sleep 1
        printf 'pub func val() -> Text { return "x" }\n' > helper.tml   # edit imported dep
        TML_DAEMON=1 "$TML_ABS" check app.tml > w2.out 2>&1; echo $? > w2.rc
        # F-029 flavor: touch the argv file to force a result-cache MISS and
        # confirm the warm-daemon recompile re-reads the edited sibling.
        sleep 1
        printf 'use helper::val\n\n\n@test("t")\nfunc t() {\n    let x: I64 = val()\n    assert(x == 1)\n}\n' > app.tml
        TML_DAEMON=1 "$TML_ABS" check app.tml > w3.out 2>&1; echo $? > w3.rc
        "$TML_ABS" daemon stop >/dev/null 2>&1
    )
    w1=$(cat "$SANDBOX/proj/w1.rc" 2>/dev/null || echo 99)
    w2=$(cat "$SANDBOX/proj/w2.rc" 2>/dev/null || echo 99)
    w3=$(cat "$SANDBOX/proj/w3.rc" 2>/dev/null || echo 99)
    check "F-028 warm miss initially clean (w1 exit 0)" test "$w1" -eq 0
    check "F-028 imported-module edit invalidates warm result (w2 exit != 0)" test "$w2" -ne 0
    check "F-029 project module re-read on warm recompile (w3 exit != 0)" test "$w3" -ne 0
else
    echo "  SKIP  F-028/F-029 daemon checks (non-Windows)"
fi

# ----------------------------------------------------------------------------
# F-030: `cache invalidate` removes the .ast.bin sidecar and reports it.
# ----------------------------------------------------------------------------
printf 'func main() -> I64 {\n    return 7\n}\n' > "$SANDBOX/inv.tml"
printf 'AST-BIN-SIDECAR' > "$SANDBOX/inv.tml.ast.bin"
inv_abs="$(cd "$SANDBOX" && pwd)/inv.tml"
TML_DAEMON=0 "$TML" cache invalidate "$inv_abs" > "$SANDBOX/f030.out" 2>&1 || true
check "F-030 cache invalidate deleted the .ast.bin sidecar" \
    bash -c "! test -f '$SANDBOX/inv.tml.ast.bin'"
check "F-030 cache invalidate reported the removal" \
    grep -q '\[ast.bin\] removed' "$SANDBOX/f030.out"

echo
echo "Summary: $pass pass, $fail fail"
rm -rf "$SANDBOX"
exit "$fail"
