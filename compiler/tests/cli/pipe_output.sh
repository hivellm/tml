#!/usr/bin/env bash
# End-to-end test for phase0r_compiler-output-pipe-hang + phase1j_diagnostics-to-stderr.
#
# Validates:
#   - `tml check/run/build` do NOT deadlock when stdout/stderr are redirected
#   - Diagnostics go to stderr, never to stdout
#   - `--no-color` / `TML_NO_COLOR` strip ANSI escapes in non-TTY mode
#
# Usage:
#     bash compiler/tests/cli/pipe_output.sh
#
# Exits non-zero on any failure.

set -euo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
TIMEOUT="${TIMEOUT:-10}"
SANDBOX=".sandbox/pipe_output_test"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found or not executable" >&2
    exit 1
fi

# Valid source.
cat > "$SANDBOX/ok.tml" <<'EOF'
func main() {
    println("hi")
}
EOF

# Source with a type error — must produce diagnostics on stderr within TIMEOUT.
cat > "$SANDBOX/bad.tml" <<'EOF'
func main() {
    let x: I32 = "not a number"
}
EOF

# Source with a parse error.
cat > "$SANDBOX/syntax.tml" <<'EOF'
let main = func() { println("hello") }
EOF

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

# 1. `check` on valid file, stdout piped to file. Must exit 0 within TIMEOUT.
check "check valid: exit 0 within ${TIMEOUT}s" \
    bash -c "timeout $TIMEOUT '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/ok.out' 2> '$SANDBOX/ok.err' && test \$? -eq 0"

# 2. `check` on type-error file: non-zero exit, diagnostics on stderr only.
check "check invalid: diagnostics on stderr" \
    bash -c "
        timeout $TIMEOUT '$TML' check '$SANDBOX/bad.tml' > '$SANDBOX/bad.out' 2> '$SANDBOX/bad.err' || true
        test -s '$SANDBOX/bad.err'
    "

check "check invalid: stdout is empty" \
    bash -c "! test -s '$SANDBOX/bad.out'"

# 3. `check` on syntax-error file: parse error on stderr only.
check "check parse error: stderr has P-code, stdout empty" \
    bash -c "
        timeout $TIMEOUT '$TML' check '$SANDBOX/syntax.tml' > '$SANDBOX/syn.out' 2> '$SANDBOX/syn.err' || true
        grep -q 'error\[P' '$SANDBOX/syn.err' && ! test -s '$SANDBOX/syn.out'
    "

# 4. Stderr diagnostics should not contain raw ANSI escapes when piped.
check "piped invalid: stderr strips ANSI in non-TTY" \
    bash -c "! grep -q $'\x1b\\[' '$SANDBOX/bad.err'"

# 5. --no-color flag suppresses colors even if stderr happened to be a TTY.
check "--no-color removes escapes" \
    bash -c "
        timeout $TIMEOUT '$TML' check --no-color '$SANDBOX/bad.tml' > '$SANDBOX/bad2.out' 2> '$SANDBOX/bad2.err' || true
        ! grep -q $'\x1b\\[' '$SANDBOX/bad2.err'
    "

# 6. TML_NO_COLOR env var has the same effect.
check "TML_NO_COLOR=1 removes escapes" \
    bash -c "
        TML_NO_COLOR=1 timeout $TIMEOUT '$TML' check '$SANDBOX/bad.tml' > '$SANDBOX/bad3.out' 2> '$SANDBOX/bad3.err' || true
        ! grep -q $'\x1b\\[' '$SANDBOX/bad3.err'
    "

# 7. `build` on an invalid file: errors on stderr, stdout empty.
check "build invalid: errors on stderr only" \
    bash -c "
        timeout $TIMEOUT '$TML' build '$SANDBOX/bad.tml' > '$SANDBOX/bld.out' 2> '$SANDBOX/bld.err' || true
        test -s '$SANDBOX/bld.err' && ! test -s '$SANDBOX/bld.out'
    "

# 8. `run` on an invalid file: errors on stderr, stdout empty.
check "run invalid: errors on stderr only" \
    bash -c "
        timeout $TIMEOUT '$TML' run '$SANDBOX/bad.tml' > '$SANDBOX/run.out' 2> '$SANDBOX/run.err' || true
        test -s '$SANDBOX/run.err' && ! test -s '$SANDBOX/run.out'
    "

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
