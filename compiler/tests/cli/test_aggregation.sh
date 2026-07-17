#!/usr/bin/env bash
# End-to-end test for phase41a_test-aggregation-default (F-005).
#
# Validates the aggregated-suite default of `tml test`:
#   1. Default grouping is aggregated (multiple test files per EXE).
#   2. --no-suite forces the per-file grouping (1 EXE per file).
#   3. --coverage forces the per-file grouping even without --no-suite.
#   4. Compile-failure isolation inside an aggregated suite: the offending
#      file is reported as a failed test + compile error, sibling files still
#      compile into the aggregated EXE and run.
#   5. Crash/timeout isolation: a test that kills the aggregated subprocess
#      (per-test watchdog, exit 99) is attributed as the offender, and the
#      never-started sibling files are re-run individually — no silent loss.
#   6. Per-test result parity: aggregated default and --no-suite produce the
#      same pass/fail/compile-error counts for the same fixture.
#
# Usage:
#     bash compiler/tests/cli/test_aggregation.sh
#
# Exits non-zero on any failure.

set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SANDBOX=".sandbox/test_aggregation"
FIXTURE="compiler/tests/aggfix"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi

pass=0
fail=0

check() {
    local name="$1"; shift
    if "$@"; then
        printf '  PASS  %s\n' "$name"
        pass=$((pass + 1))
    else
        printf '  FAIL  %s\n' "$name"
        fail=$((fail + 1))
    fi
}

cleanup() {
    rm -rf "$FIXTURE"
}
trap cleanup EXIT

# ----------------------------------------------------------------------------
# 1-3. Grouping defaults via --list-suites (compiler/borrow = 12 files < 25)
# ----------------------------------------------------------------------------
"$TML" test --suite=compiler/borrow --list-suites --no-color > "$SANDBOX/ls_default.log" 2>&1
"$TML" test --suite=compiler/borrow --list-suites --no-color --no-suite > "$SANDBOX/ls_nosuite.log" 2>&1
"$TML" test --suite=compiler/borrow --list-suites --no-color --coverage > "$SANDBOX/ls_coverage.log" 2>&1

n_default=$(grep -c 'compiler/borrow/' "$SANDBOX/ls_default.log")
n_nosuite=$(grep -c 'compiler/borrow/' "$SANDBOX/ls_nosuite.log")
n_coverage=$(grep -c 'compiler/borrow/' "$SANDBOX/ls_coverage.log")

check "default groups compiler/borrow into 1 aggregated suite (got $n_default)" \
    test "$n_default" -eq 1
check "default aggregated suite contains all 12 files" \
    grep -q '(12 tests)' "$SANDBOX/ls_default.log"
check "--no-suite lists one suite per file (got $n_nosuite)" \
    test "$n_nosuite" -eq 12
check "--coverage forces per-file grouping (got $n_coverage)" \
    test "$n_coverage" -eq 12

# ----------------------------------------------------------------------------
# Fixture: 4 test files in one aggregated suite
#   aa_hang     — infinite loop; the in-EXE per-test watchdog (100 ms) kills
#                 the subprocess with exit 99 (runs FIRST: sorted order)
#   bad_compile — type error; per-file SKIP inside the aggregated compile
#   mm_ok/zz_ok — healthy tests that must survive both failures above
# ----------------------------------------------------------------------------
mkdir -p "$FIXTURE"

cat > "$FIXTURE/aa_hang.test.tml" <<'EOF'
use test

@test
func hangs_forever() -> I32 {
    var i: I64 = 0
    loop (i >= 0) {
        i = i + 1
        if i > 1000000 {
            i = 0
        }
    }
    return 0
}
EOF

cat > "$FIXTURE/bad_compile.test.tml" <<'EOF'
use test

@test
func does_not_typecheck() -> I32 {
    let x: I32 = "this is not an integer"
    return 0
}
EOF

cat > "$FIXTURE/mm_ok.test.tml" <<'EOF'
use test

@test
func survives_sibling_crash() -> I32 {
    assert_eq(2 + 2, 4, "arithmetic works")
    return 0
}
EOF

cat > "$FIXTURE/zz_ok.test.tml" <<'EOF'
use test

@test
func also_survives() -> I32 {
    assert_eq(6 * 7, 42, "arithmetic works")
    return 0
}
EOF

# ----------------------------------------------------------------------------
# 4-5. Aggregated run: offender isolation + sibling survival
# ----------------------------------------------------------------------------
"$TML" test --suite=compiler/aggfix --no-cache --no-fail-fast --no-color \
    > "$SANDBOX/agg_run.log" 2>&1
agg_rc=$?

check "aggregated fixture run exits non-zero" test "$agg_rc" -ne 0
check "aggregated: 4 tests discovered" grep -q 'Tests:   4' "$SANDBOX/agg_run.log"
check "aggregated: both healthy siblings pass (Passed: 2)" \
    grep -q 'Passed:  2' "$SANDBOX/agg_run.log"
check "aggregated: hang + compile error fail (Failed: 2)" \
    grep -q 'Failed:  2' "$SANDBOX/agg_run.log"
check "aggregated: exactly 1 compile error" \
    grep -q 'Compile errors: 1' "$SANDBOX/agg_run.log"
check "aggregated: compile-failed file isolated as SKIP" \
    grep -q 'COMPILE SKIP.*bad_compile' "$SANDBOX/agg_run.log"
check "aggregated: hanging test attributed as TIMEOUT offender" \
    bash -c "grep -A2 'FAIL compiler/aggfix/aa_hang' '$SANDBOX/agg_run.log' | grep -q 'TIMEOUT'"
check "aggregated: no sibling reported as NOT RUN (re-run fallback)" \
    bash -c "! grep -q 'NOT RUN' '$SANDBOX/agg_run.log'"

# ----------------------------------------------------------------------------
# 6. Per-file mode parity on the same fixture
# ----------------------------------------------------------------------------
"$TML" test --suite=compiler/aggfix --no-cache --no-fail-fast --no-color --no-suite \
    > "$SANDBOX/perfile_run.log" 2>&1
perfile_rc=$?

check "per-file fixture run exits non-zero" test "$perfile_rc" -ne 0
check "per-file: both healthy siblings pass (Passed: 2)" \
    grep -q 'Passed:  2' "$SANDBOX/perfile_run.log"
check "per-file: same failure count (Failed: 2)" \
    grep -q 'Failed:  2' "$SANDBOX/perfile_run.log"
check "per-file: same compile error count" \
    grep -q 'Compile errors: 1' "$SANDBOX/perfile_run.log"

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
