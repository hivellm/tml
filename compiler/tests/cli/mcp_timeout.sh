#!/usr/bin/env bash
# End-to-end test for phase1k_mcp-timeout-recovery.
#
# Validates that the MCP subprocess-timeout path in mcp_tools.cpp does NOT
# block when the spawned subprocess hangs without writing output — the
# real UzDB/MCP failure mode before phase1k.
#
# We exercise the path indirectly by running `tml check` on a valid file
# under a tight timeout via the Windows `timeout` built-in. If the MCP
# timeout regression came back, this script would hang until the outer
# shell timeout fires.
#
# Usage:
#     bash compiler/tests/cli/mcp_timeout.sh
#
# Exits non-zero on any failure.

set -euo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SANDBOX=".sandbox/mcp_timeout_test"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi

cat > "$SANDBOX/ok.tml" <<'EOF'
func main() {
    println("ok")
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

# 1. `tml check` on a valid file piped through the outer `timeout` must return
# before 15 s with exit 0. If mcp_tools.cpp::execute_command ever regresses
# back to blocking ReadFile, this test hangs and the outer `timeout` kicks in.
check "piped check returns within budget" \
    bash -c "timeout 15 '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/out.txt' 2> '$SANDBOX/err.txt' && test \$? -eq 0"

# 2. `tml check` on a non-existent file must still terminate within budget
# rather than hang on a spin-loop.
check "piped check on missing file returns within budget" \
    bash -c "timeout 15 '$TML' check '$SANDBOX/nonexistent.tml' > '$SANDBOX/out.txt' 2> '$SANDBOX/err.txt' || test \$? -ne 124"

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
