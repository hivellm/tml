#!/usr/bin/env bash
# determinism-gate.sh — pre-push / CI determinism gate (phase25a).
#
# 1. Refreshes the pure-TML corpus test executables (compile-once; the suite
#    cache handles recompile-on-change, and execution here bypasses the
#    "cached-passed" skip by running the exes directly).
# 2. Runs the corpus under the adversarial allocator, N times per target.
# 3. Compares pass-rates against the recorded baseline
#    (docs/analysis/tml-table-analysis/07-determinism-baseline.md) via the
#    per-target minimum pass-rates encoded below. Exits nonzero on regression.
#
# Usage:
#   scripts/determinism-gate.sh [runs]     # default 30
#
# Rationale: a pass here does NOT prove soundness — it proves we did not get
# WORSE than the recorded baseline. Phase26 raises the floors to 100.

set -u
cd "$(dirname "$0")/.."

RUNS="${1:-30}"

echo "[gate] refreshing determinism corpus executables..."
# --no-suite: the corpus contract below runs ONE exe per repro file
# (compiler_determinism_<name>.exe). Since phase41a `tml test` aggregates
# multiple files per EXE by default, per-file mode must be forced here or the
# corpus would execute stale per-file exes from older sessions.
if ! ./build/debug/bin/tml.exe test compiler/tests/determinism --no-suite >/dev/null 2>&1; then
    echo "[gate] FAIL: corpus test suites do not compile (see 'tml test compiler/tests/determinism --no-suite')" >&2
    exit 1
fi

# Adversarial mode ON for the gate: layout-dependent bugs become deterministic.
export TML_ALLOC_POISON=1
export TML_ALLOC_QUARANTINE=256

# Per-target minimum pass-rate floors (percent), from the recorded baseline.
# A target absent from this table defaults to --min-pass 100.
# Floors below 100 are DOCUMENTED DEBT tracked by phase26 (see baseline doc).
min_pass_for() {
    case "$1" in
        # legacy cc targets with known-bad baselines (phase24 residue):
        essential.c) echo 0 ;;      # 0/100 measured — tracked, phase26 gate raises this
        # 86/100 normal, 98/100 adversarial measured. Floor 80 (not 90) so a
        # single unlucky crash at the pre-push sample size (10 runs) does not
        # false-alarm; the x100 baseline doc is the source of truth.
        c_essential_repro) echo 80 ;;
        *) echo 100 ;;
    esac
}

overall=0
total=0
failed=0

# Iterate corpus entries one at a time so each target gets its own floor.
while IFS= read -r line || [ -n "$line" ]; do
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [ -z "$line" ] && continue
    case "$line" in \#*) continue ;; esac

    name="${line%%:*}"
    cmd="${line#*: }"
    if [ "$name" = "$line" ] || [ "${name//[ $'\t']/}" != "$name" ]; then
        name="$line"
        cmd="$line"
    fi

    floor="$(min_pass_for "$name")"
    total=$((total + 1))
    if ! scripts/determinism.sh --runs "$RUNS" --min-pass "$floor" --label "$name" -- bash -c "$cmd"; then
        echo "[gate] REGRESSION: '$name' fell below its ${floor}% floor" >&2
        failed=$((failed + 1))
        overall=1
    fi
    echo ""
done < scripts/determinism-corpus.txt

echo "[gate] summary: $((total - failed))/${total} targets at or above their baseline floor (runs: ${RUNS}, adversarial: ON)"
exit $overall
