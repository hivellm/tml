#!/usr/bin/env bash
# End-to-end test for phase0r_compiler-output-pipe-hang.
#
# Validates that `tml check/run` do NOT deadlock when stdout/stderr are
# redirected to a pipe or file — the real hang reported by UzDB/MCP/CI.
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
check "piped valid: exit 0 within ${TIMEOUT}s" \
    bash -c "timeout $TIMEOUT '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/ok.out' 2> '$SANDBOX/ok.err' && test \$? -eq 0"

# 2. `check` on invalid file: non-zero exit, diagnostics on stderr only.
check "piped invalid: diagnostics on stderr" \
    bash -c "
        timeout $TIMEOUT '$TML' check '$SANDBOX/bad.tml' > '$SANDBOX/bad.out' 2> '$SANDBOX/bad.err' || true
        test -s '$SANDBOX/bad.err'
    "

check "piped invalid: stdout is empty" \
    bash -c "! test -s '$SANDBOX/bad.out'"

# 3. Stderr diagnostics should not contain raw ANSI escapes when piped.
check "piped invalid: stderr strips ANSI in non-TTY" \
    bash -c "! grep -q $'\x1b\\[' '$SANDBOX/bad.err'"

# 4. --no-color flag suppresses colors even if stderr happened to be a TTY.
check "--no-color removes escapes" \
    bash -c "
        timeout $TIMEOUT '$TML' check --no-color '$SANDBOX/bad.tml' > '$SANDBOX/bad2.out' 2> '$SANDBOX/bad2.err' || true
        ! grep -q $'\x1b\\[' '$SANDBOX/bad2.err'
    "

# 5. TML_NO_COLOR env var has the same effect.
check "TML_NO_COLOR=1 removes escapes" \
    bash -c "
        TML_NO_COLOR=1 timeout $TIMEOUT '$TML' check '$SANDBOX/bad.tml' > '$SANDBOX/bad3.out' 2> '$SANDBOX/bad3.err' || true
        ! grep -q $'\x1b\\[' '$SANDBOX/bad3.err'
    "

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
