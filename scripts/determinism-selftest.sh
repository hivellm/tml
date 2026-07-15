#!/usr/bin/env bash
# determinism-selftest.sh — verifies the phase25a harness machinery itself.
#
# 1. The adversarial allocator must catch a deliberate double-free
#    deterministically (quarantine message + nonzero exit).
# 2. A well-behaved corpus target must pass with the flags ON.
# 3. The runner must compute pass-rates and enforce --min-pass.
#
# Run after any change to compiler/runtime/memory/mem.c or the scripts.

set -u
cd "$(dirname "$0")/.."

fail() { echo "[selftest] FAIL: $1" >&2; exit 1; }

TML=./build/debug/bin/tml.exe
# Lives under scripts/ (NOT compiler/tests/) so `tml test` discovery never
# picks up a deliberately-crashing program as a suite.
FIXTURE=scripts/fixtures/double_free_probe.tml

# --- 1. deliberate double-free is caught deterministically -------------------
out=$(TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=64 "$TML" run "$FIXTURE" 2>&1)
rc=$?
[ "$rc" -ne 0 ] || fail "double-free probe exited 0 under quarantine"
case "$out" in
    *"detected by TML_ALLOC_QUARANTINE"*) ;;
    *) fail "quarantine did not report the double-free (got: $out)" ;;
esac
echo "[selftest] 1/3 double-free detection: OK (exit $rc)"

# --- 2. well-behaved target passes with flags ON -----------------------------
if [ ! -x build/debug/cache/tests/compiler_determinism_f002_hashmap.exe ]; then
    "$TML" test compiler/tests/determinism >/dev/null 2>&1 \
        || fail "could not compile corpus test exes"
fi
TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=64 \
    ./build/debug/cache/tests/compiler_determinism_f002_hashmap.exe --run-all >/dev/null 2>&1 \
    || fail "well-behaved corpus exe failed under adversarial flags"
echo "[selftest] 2/3 clean run under adversarial flags: OK"

# --- 3. runner enforces --min-pass -------------------------------------------
scripts/determinism.sh --runs 3 --min-pass 100 --label selftest-true -- true >/dev/null \
    || fail "runner rejected an always-passing command"
if scripts/determinism.sh --runs 3 --min-pass 100 --label selftest-false -- false >/dev/null; then
    fail "runner accepted an always-failing command at --min-pass 100"
fi
echo "[selftest] 3/3 runner pass-rate enforcement: OK"

echo "[selftest] all checks passed"
