#!/usr/bin/env bash
# End-to-end test for phase40a_mcp-daemon-warm-path (F-016/F-017).
#
# Validates the warm compile-daemon routing that the MCP tools rely on:
#   1. TML_DAEMON=1 with no daemon running falls back to direct compilation
#      (correct result, no error).
#   2. With a daemon running, the warm path returns output identical to the
#      cold path, and a cache-hit is clearly faster than a cold start.
#   3. Hard-killing the daemon mid-session degrades transparently (fallback).
#   4. Rebuilding the compiler DLL under a live daemon (simulated by touching
#      its mtime) must NOT surface a spurious failure: the client detects the
#      daemon's restart notice and falls through to direct compilation.
#   5. The MCP server auto-starts the daemon on the first daemon-eligible
#      tool call (fire-and-forget) and still returns a correct result.
#
# Usage:
#     bash compiler/tests/cli/daemon_warm_path.sh
#
# Exits non-zero on any failure. Windows-only (the daemon pipe transport is
# currently implemented for Windows named pipes).

set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
TML_MCP="${TML_MCP:-./build/debug/bin/tml_mcp.exe}"
DLL="./build/debug/bin/plugins/tml_compiler.dll"
SANDBOX=".sandbox/daemon_warm_path_test"
mkdir -p "$SANDBOX"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi

cat > "$SANDBOX/ok.tml" <<'EOF'
func add(a: I64, b: I64) -> I64 {
    return a + b
}

func main() {
    let x = add(2, 3)
    println("{x}")
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

now_ms() {
    date +%s%3N
}

daemon_pid() {
    # "TML daemon: running (PID 12345, responsive)" -> 12345
    "$TML" daemon status 2>/dev/null | sed -n 's/.*PID \([0-9]*\).*/\1/p'
}

# Preserve the compiler DLL mtime across the staleness test (the test-EXE
# cache is keyed on it). Restored on every exit path.
touch -r "$DLL" "$SANDBOX/dll_mtime_ref"
cleanup() {
    touch -r "$SANDBOX/dll_mtime_ref" "$DLL" 2>/dev/null || true
    "$TML" daemon stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

# ----------------------------------------------------------------------------
# 0. Start from a clean slate
# ----------------------------------------------------------------------------
"$TML" daemon stop >/dev/null 2>&1 || true
sleep 1

# ----------------------------------------------------------------------------
# 1. Fallback: TML_DAEMON=1 with no daemon running -> direct compile, exit 0
# ----------------------------------------------------------------------------
check "fallback when daemon absent (exit 0)" \
    bash -c "TML_DAEMON=1 '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/fallback.out' 2>&1"

# ----------------------------------------------------------------------------
# 2. Warm path parity + speed
# ----------------------------------------------------------------------------
"$TML" check "$SANDBOX/ok.tml" > "$SANDBOX/cold.out" 2>&1
cold_rc=$?

check "daemon starts" bash -c "'$TML' daemon start > /dev/null 2>&1 && sleep 1 && '$TML' daemon status 2>/dev/null | grep -q 'running'"

# First warm request is a cache-miss (fills the daemon result cache)
TML_DAEMON=1 "$TML" check "$SANDBOX/ok.tml" > "$SANDBOX/warm1.out" 2>&1
# Second warm request is a cache-hit
t0=$(now_ms)
TML_DAEMON=1 "$TML" check "$SANDBOX/ok.tml" > "$SANDBOX/warm2.out" 2>&1
warm_rc=$?
t1=$(now_ms)
warm_ms=$((t1 - t0))

check "warm exit code matches cold exit code" test "$warm_rc" -eq "$cold_rc"
check "warm output identical to cold output" diff -q "$SANDBOX/cold.out" "$SANDBOX/warm2.out"
# Cold is ~450 ms; a cache-hit is ~7 ms. 250 ms splits them with wide margin.
check "warm cache-hit is fast (<250 ms, got ${warm_ms} ms)" test "$warm_ms" -lt 250

# ----------------------------------------------------------------------------
# 3. Hard-kill mid-session -> transparent fallback
# ----------------------------------------------------------------------------
pid=$(daemon_pid)
check "daemon PID discoverable" test -n "$pid"
if [ -n "$pid" ]; then
    cmd //c "taskkill /F /PID $pid" >/dev/null 2>&1
    sleep 1
fi
check "check still succeeds after daemon killed (fallback)" \
    bash -c "TML_DAEMON=1 '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/killed.out' 2>&1"
check "killed-daemon output identical to cold output" diff -q "$SANDBOX/cold.out" "$SANDBOX/killed.out"

# ----------------------------------------------------------------------------
# 4. DLL staleness under a live daemon -> restart notice falls through, exit 0
# ----------------------------------------------------------------------------
"$TML" daemon start > /dev/null 2>&1
sleep 1
# Warm it so the daemon is mid-session when the "rebuild" happens
TML_DAEMON=1 "$TML" check "$SANDBOX/ok.tml" > /dev/null 2>&1
touch "$DLL"   # simulate a compiler rebuild (mtime restored by cleanup trap)
check "stale-DLL request falls through to direct compile (exit 0)" \
    bash -c "TML_DAEMON=1 '$TML' check '$SANDBOX/ok.tml' > '$SANDBOX/stale.out' 2>&1"
check "stale-DLL output identical to cold output" diff -q "$SANDBOX/cold.out" "$SANDBOX/stale.out"
touch -r "$SANDBOX/dll_mtime_ref" "$DLL"
# The daemon must have exited after detecting the stale DLL
sleep 1
check "daemon exited after detecting stale DLL" \
    bash -c "! ('$TML' daemon status 2>/dev/null | grep -q 'responsive')"

# ----------------------------------------------------------------------------
# 5. MCP server auto-starts the daemon on a daemon-eligible tool call
# ----------------------------------------------------------------------------
if [ -x "$TML_MCP" ]; then
    "$TML" daemon stop >/dev/null 2>&1 || true
    sleep 1
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"daemon-warm-path-test","version":"1.0"}}}'
        printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized"}'
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"check\",\"arguments\":{\"file\":\"$SANDBOX/ok.tml\"}}}"
    } | timeout 60 "$TML_MCP" > "$SANDBOX/mcp.out" 2> "$SANDBOX/mcp.err"
    check "MCP check returns correct result" grep -q "Type check passed" "$SANDBOX/mcp.out"
    # Auto-start is fire-and-forget: poll up to 15 s for the daemon to appear
    started=1
    for _ in $(seq 1 15); do
        if "$TML" daemon status 2>/dev/null | grep -q 'responsive'; then
            started=0
            break
        fi
        sleep 1
    done
    check "MCP auto-started the compile daemon" test "$started" -eq 0
else
    echo "  SKIP  MCP auto-start test ($TML_MCP not found)"
fi

echo
echo "Summary: $pass pass, $fail fail"
exit "$fail"
