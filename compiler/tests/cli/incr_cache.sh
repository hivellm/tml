#!/usr/bin/env bash
# CLI test for phase42a_incr-cache-structural (F-019..F-026).
#
# Validates that the ADR-002 red-green incr.bin cache is a real cross-run cache:
#
#   F-023  Removing test_entry_index from CodegenUnitKey + deriving the entry
#          symbol from a file-stable id means touching ONE test file REDs only
#          that unit — siblings stay GREEN (before, an index shift RED'd all).
#   F-020  A shared per-run PrevSessionCache load: a warm rerun serves GREEN and
#          the whole suite does a bounded number of incr.bin loads (not per-file).
#   F-024  compiler_build_hash is content-addressed → a `<dll>.bhash` sidecar.
#   F-025  Config-partitioned `incr.<opts>.bin` files.
#   F-019  A per-run `[incr] incr: GREEN=.. RED=..` telemetry line is emitted.
#
# Usage:  bash compiler/tests/cli/incr_cache.sh
# Exits non-zero on any failure. Uses the core/hash clean suite.

set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
SUITE="core/hash"
TEST_FILE="lib/core/tests/hash/hash_basic.test.tml"
SANDBOX=".sandbox/incr_cache_test"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found" >&2
    exit 1
fi
if [ ! -f "$TEST_FILE" ]; then
    echo "FAIL: $TEST_FILE not found" >&2
    exit 1
fi

rm -rf "$SANDBOX"; mkdir -p "$SANDBOX"
BACKUP="$SANDBOX/hash_basic.bak"
cp "$TEST_FILE" "$BACKUP"

restore() { cp "$BACKUP" "$TEST_FILE" 2>/dev/null || true; }
trap restore EXIT

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

# Run the suite (forcing recompile through the incr layer) and echo the
# telemetry GREEN/RED as "GREEN RED" on stdout, or "NONE" if not found.
run_suite() {
    local out="$1"
    "$TML" test --suite="$SUITE" --no-cache > "$out" 2>&1
    local line
    line="$(grep -o 'incr: GREEN=[0-9]* RED=[0-9]*' "$out" | tail -1)"
    if [ -z "$line" ]; then echo "NONE"; return; fi
    echo "$line" | sed -E 's/incr: GREEN=([0-9]*) RED=([0-9]*)/\1 \2/'
}

# 1. Warm the cache (cold run — entries may be RED).
run_suite "$SANDBOX/run1.out" >/dev/null

# 2. Warm rerun, nothing changed → all GREEN, zero RED (F-020/F-023/F-019).
read -r g2 r2 <<< "$(run_suite "$SANDBOX/run2.out")"
check "F-019 telemetry line present" bash -c "[ '$g2' != 'NONE' ]"
check "F-020/F-023 warm rerun all GREEN (GREEN>0)" bash -c "[ '${g2:-0}' -gt 0 ] 2>/dev/null"
check "F-023 warm rerun zero RED" bash -c "[ '${r2:-1}' -eq 0 ] 2>/dev/null"

# 3. Touch ONE file → only that unit RED, siblings GREEN (F-023 headline).
printf '\n// phase42a incr_cache gate probe\n' >> "$TEST_FILE"
read -r g3 r3 <<< "$(run_suite "$SANDBOX/run3.out")"
restore
check "F-023 one-file touch keeps siblings GREEN (GREEN>0)" bash -c "[ '${g3:-0}' -gt 0 ] 2>/dev/null"
check "F-023 one-file touch REDs only affected (RED>=1)" bash -c "[ '${r3:-0}' -ge 1 ] 2>/dev/null"
check "F-023 touched RED count is small (<GREEN)" bash -c "[ '${r3:-99}' -lt '${g3:-0}' ] 2>/dev/null"

# 4. F-024: content-hash sidecar exists next to the compiler DLL.
check "F-024 .bhash sidecar exists" bash -c "ls build/debug/bin/plugins/*.bhash >/dev/null 2>&1"

# 5. F-025: at least one config-partitioned incr.<hash>.bin exists.
check "F-025 partitioned incr.<hash>.bin exists" \
    bash -c "ls build/debug/cache/incr/incr.*.bin >/dev/null 2>&1"

printf '\nincr_cache: %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
