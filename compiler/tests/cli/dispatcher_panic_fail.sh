#!/usr/bin/env bash
# Regression test for phase44a_test-dispatcher-false-pass.
#
# Pins the fix: a panicking `@test` body must be reported `test_fail` (with
# the real panic message) and the run must exit non-zero — in BOTH
# standalone (`--no-suite`, 1 EXE per file — where the false-pass evidence
# was originally observed) and suite-packed (default, multiple files per
# EXE) modes. Before the fix, `generate_entry.cpp` discarded the result of
# `tml_run_test_with_catch` and unconditionally returned 0, so a panicking
# body printed `panic: ...` to stderr and was STILL reported `test_pass`.
#
# Also pins that `@should_panic` still passes: it shares the same
# setjmp/longjmp path (`tml_run_should_panic`) that the fix's runtime
# changes touched (`tml_watchdog_cancel` on every longjmp recovery path,
# the removed `__try/__except` SEH scope in `tml_run_test_seh`).
#
# Usage: bash compiler/tests/cli/dispatcher_panic_fail.sh
# Exits non-zero on any failure.
set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SANDBOX=".sandbox/dispatcher_panic_fail"
FIXTURE="compiler/tests/dispfix"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found (build first)" >&2
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
# Fixture: 3 test files.
#   panics.test.tml       — a plain @test whose body panics; MUST fail.
#   should_panic_ok.test.tml — @should_panic body that panics; MUST pass.
#   sane.test.tml          — a healthy sanity test; MUST pass (proves the
#                            fixture harness itself, and siblings, are
#                            unaffected by the failing test).
# ----------------------------------------------------------------------------
mkdir -p "$FIXTURE"

cat > "$FIXTURE/panics.test.tml" <<'EOF'
use test

// Regression fixture for phase44a: a panicking @test body must be reported
// test_fail (not test_pass), with the real panic message, and the run must
// exit non-zero. Before the fix, the test-entry wrapper discarded the
// tml_run_test_with_catch result and unconditionally returned 0.
@test
func panics_must_fail() -> I32 {
    assert(false, "phase44a regression: forced panic must fail, not pass")
    return 0
}
EOF

cat > "$FIXTURE/should_panic_ok.test.tml" <<'EOF'
use test

// Pins that @should_panic still passes after the phase44a fix: it shares
// the same setjmp/longjmp path (tml_run_should_panic) whose runtime
// changed (tml_watchdog_cancel on every longjmp recovery path).
@test
@should_panic
func should_panic_still_passes() -> I32 {
    assert(false, "expected panic for @should_panic")
    return 0
}
EOF

cat > "$FIXTURE/sane.test.tml" <<'EOF'
use test

// Healthy sibling: must keep passing regardless of the other two files'
// outcome (proves the failing test does not corrupt sibling results).
@test
func sane_test_passes() -> I32 {
    assert_eq(2 + 2, 4, "arithmetic sanity")
    return 0
}
EOF

# ----------------------------------------------------------------------------
# Helper: run `tml test` on the fixture in a given mode, assert on the
# terminal summary/NDJSON-derived JSON report.
#   $1 = mode label (used in log file names)
#   $2 = extra tml test flag ("" for suite-packed default, "--no-suite" for
#        standalone/per-file)
# ----------------------------------------------------------------------------
run_mode() {
    local mode="$1"
    local extra_flag="$2"
    local run_log="$SANDBOX/${mode}_run.log"
    local json_out="$SANDBOX/${mode}.jsonl"

    # shellcheck disable=SC2086
    "$TML" test --suite=compiler/dispfix --no-cache --no-fail-fast --no-color \
        --output=json:"$json_out" $extra_flag > "$run_log" 2>&1
    local rc=$?

    check "[$mode] run exits non-zero (panicking test present)" \
        test "$rc" -ne 0
    check "[$mode] 3 tests discovered" \
        grep -q 'Tests:   3' "$run_log"
    check "[$mode] exactly 2 tests pass (sane + should_panic)" \
        grep -q 'Passed:  2' "$run_log"
    check "[$mode] exactly 1 test fails (the forced panic)" \
        grep -q 'Failed:  1' "$run_log"
    check "[$mode] FAIL line names the panicking test" \
        grep -q 'FAIL.*dispfix/panics' "$run_log"
    check "[$mode] terminal failure detail carries the panic message" \
        grep -q 'phase44a regression: forced panic must fail, not pass' "$run_log"

    # JSON report: written from the parsed NDJSON test_fail/test_pass events
    # (testing_coordinator.cpp: t.error = ev.error). Confirms the raw NDJSON
    # contract, not just the terminal summary.
    check "[$mode] JSON report was written" \
        test -s "$json_out"
    check "[$mode] JSON: panicking test recorded passed:false" \
        grep -q '"name":"panics","passed":false' "$json_out"
    check "[$mode] JSON: panicking test carries the real panic message in error" \
        grep -q '"error":"[^"]*phase44a regression: forced panic must fail, not pass' "$json_out"
    # Assert the exit code is positively non-zero (a panic surfaces as -1), rather
    # than merely "not the string exit_code:0" — the negated form would also pass
    # if the field were missing entirely.
    check "[$mode] JSON: panicking test has non-zero exit_code" \
        bash -c "grep -o '\"name\":\"panics\"[^}]*' '$json_out' | grep -qE '\"exit_code\":(-[0-9]+|[1-9][0-9]*)'"
    check "[$mode] JSON: should_panic test recorded passed:true" \
        grep -q '"name":"should_panic_ok","passed":true' "$json_out"
    check "[$mode] JSON: sane test recorded passed:true" \
        grep -q '"name":"sane","passed":true' "$json_out"
}

# ----------------------------------------------------------------------------
# 1. Suite-packed (default): all 3 files aggregated into 1 EXE.
# ----------------------------------------------------------------------------
run_mode "suite" ""

# ----------------------------------------------------------------------------
# 2. Standalone (--no-suite): 1 EXE per file — the mode where the original
#    false-pass evidence (shared_get_sound.test.tml) was observed.
# ----------------------------------------------------------------------------
run_mode "standalone" "--no-suite"

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
