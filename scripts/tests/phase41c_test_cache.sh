#!/usr/bin/env bash
# Integration test for phase41c — test result cache revival + content-aware
# invalidation (F-014), plus the incremental-cache / import-scan cleanups
# (F-010/F-013) that must remain result-transparent.
#
# Verifies, end-to-end against the real tml.exe:
#   1. Second-run reuse: a clean suite compiled once is fully cached on the
#      second run ("is cached (passed, skipping)"), with identical pass counts.
#   2. Content-aware invalidation (the F-014 headline): `touch`-ing the compiler
#      DLL (mtime changes, bytes identical) does NOT wipe the cache — the rerun
#      stays cached instead of logging "invalidating all cached EXEs". The old
#      mtime:size key invalidated here and forced a full recompile.
#   3. --no-cache preserves siblings: running one suite with --no-cache keeps
#      every other suite's cached entry (the save no longer overwrites an
#      unloaded map down to just the suite it ran).
#   4. Result transparency (F-010/F-013): a suite importing a std module
#      (std/json) links and passes identically cold vs cached.
#
# Usage: scripts/tests/phase41c_test_cache.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TML="$ROOT/build/debug/bin/tml.exe"
CACHE_JSON="$ROOT/build/debug/cache/tests.json"
DLL="$ROOT/build/debug/bin/plugins/tml_compiler.dll"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$TML" ]]; then
    echo "tml.exe not built at $TML — run scripts/build.bat first" >&2
    exit 2
fi
[[ -f "$DLL" ]] || DLL="$ROOT/build/debug/bin/tml.exe" # fall back to exe fingerprint

fail() { echo "FAIL: $*" >&2; exit 1; }
pass_count() { grep -oE "Passed:[^0-9]*[0-9]+" "$1" | grep -oE "[0-9]+" | tail -1; }

run() { # run <suite> <logfile> [extra args...]
    local suite="$1" log="$2"; shift 2
    ( cd "$ROOT" && "$TML" test --suite="$suite" "$@" ) > "$log" 2>&1 || true
}

echo "== phase41c cache integration test =="

# ---------------------------------------------------------------------------
# 1. Second-run reuse (core/hash) + pass-count parity.
# ---------------------------------------------------------------------------
run core/hash "$TMP/hash1.log"
run core/hash "$TMP/hash2.log"
p1="$(pass_count "$TMP/hash1.log")"; p2="$(pass_count "$TMP/hash2.log")"
[[ -n "$p1" && "$p1" == "$p2" ]] || fail "core/hash pass-count divergence: run1=$p1 run2=$p2"
grep -q "is cached (passed, skipping)" "$TMP/hash2.log" \
    || fail "core/hash second run did not hit the result cache"
echo "  [1] second-run reuse OK (core/hash $p1/$p1 cached on run 2)"

# ---------------------------------------------------------------------------
# 2. Content-aware invalidation: touch the DLL (mtime++ , bytes identical).
#    The cache must SURVIVE (this is the whole point of F-014).
# ---------------------------------------------------------------------------
touch "$DLL"
run compiler/borrow "$TMP/borrow_seed.log"   # seed an entry with the current hash
run compiler/borrow "$TMP/borrow_cached.log"
grep -q "is cached (passed, skipping)" "$TMP/borrow_cached.log" \
    || fail "compiler/borrow not cached before the touch test (setup failed)"
touch "$DLL"
run compiler/borrow "$TMP/borrow_after_touch.log"
if grep -q "invalidating all cached EXEs" "$TMP/borrow_after_touch.log"; then
    fail "touching the DLL still wiped the cache (content-aware hash regressed)"
fi
grep -q "is cached (passed, skipping)" "$TMP/borrow_after_touch.log" \
    || fail "compiler/borrow recompiled after a no-op DLL touch (F-014 regressed)"
echo "  [2] content-aware invalidation OK (touch did not wipe the cache)"

# ---------------------------------------------------------------------------
# 3. --no-cache preserves sibling entries.
# ---------------------------------------------------------------------------
# Ensure both suites are cached first.
run core/hash "$TMP/sib_seed_hash.log"
before="$(grep -c '"all_passed"' "$CACHE_JSON" || echo 0)"
run compiler/borrow "$TMP/nocache.log" --no-cache
after="$(grep -c '"all_passed"' "$CACHE_JSON" || echo 0)"
[[ "$after" -ge "$before" ]] \
    || fail "--no-cache shrank the cache: before=$before after=$after (siblings wiped)"
run core/hash "$TMP/sib_check.log"
grep -q "is cached (passed, skipping)" "$TMP/sib_check.log" \
    || fail "sibling core/hash lost its cache entry after a --no-cache run"
echo "  [3] --no-cache preserves siblings OK (entries $before -> $after, sibling cached)"

# ---------------------------------------------------------------------------
# 4. Result transparency for a std-import suite (F-010/F-013 threaded scan).
# ---------------------------------------------------------------------------
run std/json "$TMP/json1.log" --no-cache
run std/json "$TMP/json2.log"
j1="$(pass_count "$TMP/json1.log")"; j2="$(pass_count "$TMP/json2.log")"
[[ -n "$j1" && "$j1" == "$j2" ]] || fail "std/json pass-count divergence: cold=$j1 cached=$j2"
grep -q "is cached (passed, skipping)" "$TMP/json2.log" \
    || fail "std/json second run did not hit the result cache"
echo "  [4] std-import suite transparent OK (std/json $j1/$j1 identical cold vs cached)"

echo "== phase41c cache integration test: ALL PASS =="
