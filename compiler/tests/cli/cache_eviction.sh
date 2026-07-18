#!/usr/bin/env bash
# CLI test for phase42c_cache-eviction-and-lazy-meta (F-031).
#
# Verifies the LRU cache evictor wired into every test/build/run teardown
# (`cli::enforce_cache_caps` -> `evict_dir_to_cap`, cmd_cache.cpp):
#
#   1. Suite-EXE cache is bounded to its cap (default 512 MB, overridable via
#      TML_CACHE_TESTS_CAP_MB). When over cap, unreferenced/oldest EXEs are
#      evicted first; the just-run (referenced) suite EXE survives.
#   2. Eviction removes real files, not just JSON fields (the F-031 defect).
#
# NOTE: eviction targets the SHARED content-addressed cache; evicted EXEs are
# self-healing (regenerated on next use), so this test is safe to re-run but
# will force other cached suites to recompile once. Dummy payloads are given an
# OLD mtime so they are the first to be evicted.
#
# Usage: bash compiler/tests/cli/cache_eviction.sh
# Exits non-zero on any failure.

set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
CACHE_TESTS="build/debug/cache/tests"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found (build first)" >&2
    exit 1
fi
if [ ! -d "$CACHE_TESTS" ]; then
    mkdir -p "$CACHE_TESTS"
fi

pass=0
fail=0
check() {
    local name="$1"; shift
    if "$@"; then printf '  PASS  %s\n' "$name"; pass=$((pass + 1))
    else printf '  FAIL  %s\n' "$name"; fail=$((fail + 1)); fi
}

echo "== phase42c F-031: cache eviction =="

# --- 1. Seed oversized dummy EXEs with an OLD mtime (evicted first) ----------
DUMMY_PREFIX="$CACHE_TESTS/zzz_evicttest_"
for i in $(seq 1 20); do
    # 5 MB each -> 100 MB of unreferenced, oldest cache pressure.
    head -c 5242880 /dev/zero > "${DUMMY_PREFIX}${i}.exe" 2>/dev/null
    touch -t 200001010000 "${DUMMY_PREFIX}${i}.exe" 2>/dev/null
done
dummies_before=$(ls ${DUMMY_PREFIX}*.exe 2>/dev/null | wc -l)
check "seeded 20 dummy EXEs" test "$dummies_before" -eq 20

# --- 2. Run a small clean suite with a tiny cap to force eviction -----------
# Teardown (incr_test_run_end -> enforce_cache_caps) evicts EXEs down to the cap.
TML_CACHE_TESTS_CAP_MB=20 "$TML" test --suite=core/hash > .sandbox/cache_eviction_run.log 2>&1
run_rc=$?
check "suite run succeeded" test "$run_rc" -eq 0

# --- 3. The evictor logged an eviction below the 20 MB cap ------------------
check "evictor fired + logged before->after" \
    grep -qE "\[evict\] .*tests.*->" .sandbox/cache_eviction_run.log

# --- 4. Oldest dummy orphans were evicted (files deleted, not just JSON) ----
dummies_after=$(ls ${DUMMY_PREFIX}*.exe 2>/dev/null | wc -l)
check "dummy orphan EXEs deleted from disk" test "$dummies_after" -lt "$dummies_before"

# --- 5. EXE cache is now under the 20 MB cap --------------------------------
exe_bytes=$(cat ${CACHE_TESTS}/*.exe 2>/dev/null | wc -c)
cap_bytes=$((20 * 1024 * 1024))
# Allow the just-compiled referenced suite EXE(s) headroom: assert well under
# the pre-eviction ~100 MB+ pressure, i.e. the cap was actually enforced.
check "suite-EXE cache bounded near cap" test "$exe_bytes" -le $((cap_bytes + 20 * 1024 * 1024))

# --- cleanup any surviving dummies -----------------------------------------
rm -f ${DUMMY_PREFIX}*.exe 2>/dev/null

echo ""
echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]
