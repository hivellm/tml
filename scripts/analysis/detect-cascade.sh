#!/bin/bash
# Detect nested when/Maybe cascade patterns in TML files
#
# Finds files where "Nothing => {}" appears, indicating Maybe cascade
# unwrapping that should use let-else or ?. instead.
#
# Usage: ./scripts/detect-cascade.sh [path...]

set -e
PATHS="${@:-lib/ docs/papers/llm-ir-debugging/scripts/}"

echo "=== Cascade Detector ==="
echo ""
echo "Scanning for Nothing => {} patterns (cascade unwrapping)..."
echo ""

# Find all .tml files with Nothing => {} and count occurrences
grep -rn "Nothing => {}" $PATHS --include="*.tml" 2>/dev/null | \
    cut -d: -f1 | sort | uniq -c | sort -rn | \
    while read count file; do
        # Classify severity
        if [ "$count" -ge 10 ]; then
            sev="HIGH  "
        elif [ "$count" -ge 5 ]; then
            sev="MEDIUM"
        else
            sev="LOW   "
        fi
        printf "  [%s] %3d occurrences  %s\n" "$sev" "$count" "$file"
    done

echo ""
echo "--- Top cascade locations (3+ Nothing in same function) ---"
echo ""

# Find specific lines where cascades are deepest
for f in $(grep -rl "Nothing => {}" $PATHS --include="*.tml" 2>/dev/null); do
    # Find functions with 3+ Nothing => {}
    awk '
    /^func / || /pub func / { fname=$0; ncount=0; start=NR }
    /Nothing => \{\}/ { ncount++ }
    /^}$/ {
        if (ncount >= 3) {
            # Extract function name
            match(fname, /func ([a-zA-Z_][a-zA-Z0-9_]*)/, m)
            printf "  %s:%d  func %s()  — %d cascades\n", FILENAME, start, m[1], ncount
        }
        ncount=0
    }
    ' "$f" 2>/dev/null
done

echo ""
echo "--- Summary ---"
total=$(grep -rc "Nothing => {}" $PATHS --include="*.tml" 2>/dev/null | awk -F: '{s+=$2} END {print s}')
files=$(grep -rl "Nothing => {}" $PATHS --include="*.tml" 2>/dev/null | wc -l)
echo "Total Nothing=>{}: $total across $files files"
echo ""
echo "Fix: replace nested when with let-else:"
echo "  let Just(x) = expr else { continue }"
