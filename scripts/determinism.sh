#!/usr/bin/env bash
# determinism.sh — repro runner for the TML determinism harness (phase25a).
#
# Runs a command N times and reports pass count, fail count, and an
# exit-code histogram. Exits non-zero when the pass-rate falls below the
# --min-pass threshold. Heap-layout-dependent crashes (the phase24 bug
# class) reproduce only in a fraction of runs — this script turns that
# fraction into a measured, gateable number.
#
# Usage:
#   scripts/determinism.sh [options] -- <command> [args...]
#   scripts/determinism.sh [options] --corpus <manifest>
#
# Options:
#   -n, --runs N       Number of runs per command (default: 100)
#   --min-pass PCT     Minimum pass-rate percentage; below it the script
#                      exits 1 (default: 100)
#   --timeout SECS     Per-run timeout via timeout(1) when available
#                      (default: 60; 0 disables). Timed-out runs count as
#                      failures with exit code 124.
#   --label NAME       Display label for single-command mode
#   -h, --help         This message
#
# Corpus manifest format (see scripts/determinism-corpus.txt):
#   - one shell command per line, executed with `bash -c` from the repo root
#   - blank lines and lines starting with '#' are ignored
#   - an optional "name: " prefix before the command sets the display label
#
# Adversarial allocator mode — environment variables are inherited by the
# target commands, so the same corpus measures both modes:
#   scripts/determinism.sh --corpus scripts/determinism-corpus.txt
#   TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=256 \
#       scripts/determinism.sh --corpus scripts/determinism-corpus.txt
#
# TML_ALLOC_POISON=1      fill freed heap blocks with 0xDD (reads of freed
#                         memory see garbage deterministically)
# TML_ALLOC_QUARANTINE=N  freed blocks are held in a FIFO quarantine of N
#                         entries before the real free; double-frees of a
#                         quarantined block abort deterministically
# (implemented in compiler/runtime/memory/mem.c, mem_free)

set -u

RUNS=100
MIN_PASS=100
TIMEOUT_SECS=60
CORPUS=""
LABEL=""
declare -a COMMAND=()

usage() {
    sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        -n|--runs)
            RUNS="$2"; shift 2 ;;
        --min-pass)
            MIN_PASS="$2"; shift 2 ;;
        --timeout)
            TIMEOUT_SECS="$2"; shift 2 ;;
        --corpus)
            CORPUS="$2"; shift 2 ;;
        --label)
            LABEL="$2"; shift 2 ;;
        -h|--help)
            usage; exit 0 ;;
        --)
            shift
            COMMAND=("$@")
            break ;;
        *)
            echo "determinism.sh: unknown option '$1' (use -- before the command)" >&2
            exit 2 ;;
    esac
done

case "$RUNS" in ''|*[!0-9]*) echo "determinism.sh: --runs must be a positive integer" >&2; exit 2 ;; esac
case "$MIN_PASS" in ''|*[!0-9]*) echo "determinism.sh: --min-pass must be an integer percentage" >&2; exit 2 ;; esac
if [ "$RUNS" -lt 1 ]; then
    echo "determinism.sh: --runs must be >= 1" >&2
    exit 2
fi

if [ -z "$CORPUS" ] && [ ${#COMMAND[@]} -eq 0 ]; then
    usage >&2
    exit 2
fi
if [ -n "$CORPUS" ] && [ ${#COMMAND[@]} -gt 0 ]; then
    echo "determinism.sh: use either --corpus or a command, not both" >&2
    exit 2
fi

# timeout(1) is present in Git Bash / coreutils; degrade gracefully without it.
TIMEOUT_BIN=""
if [ "$TIMEOUT_SECS" != "0" ] && command -v timeout >/dev/null 2>&1; then
    TIMEOUT_BIN="timeout"
fi

# run_target <label> <command-string>
# Prints the per-target report and returns 0 when the pass-rate meets
# MIN_PASS. Sets globals LAST_PASS / LAST_FAIL.
run_target() {
    local label="$1"
    local cmd="$2"
    local pass=0
    local fail=0
    local i rc
    # Exit-code histogram: bash 4+ associative array.
    local -A histogram=()

    for ((i = 1; i <= RUNS; i++)); do
        if [ -n "$TIMEOUT_BIN" ]; then
            "$TIMEOUT_BIN" "$TIMEOUT_SECS" bash -c "$cmd" >/dev/null 2>&1
        else
            bash -c "$cmd" >/dev/null 2>&1
        fi
        rc=$?
        histogram[$rc]=$(( ${histogram[$rc]:-0} + 1 ))
        if [ "$rc" -eq 0 ]; then
            pass=$((pass + 1))
        else
            fail=$((fail + 1))
        fi
    done

    local hist_str=""
    local code
    for code in $(printf '%s\n' "${!histogram[@]}" | sort -n); do
        hist_str+="${hist_str:+, }exit ${code} x${histogram[$code]}"
    done

    echo "[determinism] target: ${label}"
    echo "[determinism]   cmd:  ${cmd}"
    echo "[determinism]   runs: ${RUNS}  pass: ${pass}  fail: ${fail}  ($((pass * 100 / RUNS))%)"
    echo "[determinism]   exit codes: ${hist_str}"

    LAST_PASS=$pass
    LAST_FAIL=$fail

    # pass/RUNS >= MIN_PASS/100  <=>  pass*100 >= MIN_PASS*RUNS (integer-safe)
    [ $((pass * 100)) -ge $((MIN_PASS * RUNS)) ]
}

overall=0

if [ -n "$CORPUS" ]; then
    if [ ! -f "$CORPUS" ]; then
        echo "determinism.sh: corpus manifest '$CORPUS' not found" >&2
        exit 2
    fi
    total_targets=0
    failed_targets=0
    while IFS= read -r line || [ -n "$line" ]; do
        # Strip leading/trailing whitespace; skip blanks and comments.
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"
        [ -z "$line" ] && continue
        case "$line" in \#*) continue ;; esac

        # Optional "name: command" prefix (name = no spaces before ':').
        name="${line%%:*}"
        rest="${line#*: }"
        if [ "$name" != "$line" ] && [ "${name//[ $'\t']/}" = "$name" ]; then
            label="$name"
            cmd="$rest"
        else
            label="$line"
            cmd="$line"
        fi

        total_targets=$((total_targets + 1))
        if ! run_target "$label" "$cmd"; then
            failed_targets=$((failed_targets + 1))
            overall=1
        fi
        echo ""
    done < "$CORPUS"
    echo "[determinism] corpus summary: $((total_targets - failed_targets))/${total_targets} targets at >= ${MIN_PASS}% pass-rate (runs per target: ${RUNS})"
else
    # Single-command mode: preserve argument boundaries via printf %q.
    cmd=$(printf '%q ' "${COMMAND[@]}")
    cmd="${cmd% }"
    run_target "${LABEL:-$cmd}" "$cmd" || overall=1
fi

exit $overall
