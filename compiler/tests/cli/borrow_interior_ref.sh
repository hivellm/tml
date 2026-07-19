#!/usr/bin/env bash
# CLI test for phase26e 1.2 — interior-reference lifetime binding (borrow checker).
#
# The NLL borrow checker runs during `build` (NOT `check`, which stops after the
# type checker). These fixtures each hold two conflicting interior references
# into a container across their last uses; the checker must reject them with a
# B009-class "cannot borrow ..." diagnostic (non-zero exit). The positive shapes
# (NLL-released refs, two shared refs, immediate use) live in the @test suite and
# must stay green — see lib/std/tests/collections/list_interior_ref.test.tml.
#
# Usage: bash compiler/tests/cli/borrow_interior_ref.sh
# Exits non-zero on any failure.
set -uo pipefail

TML="${TML:-./build/debug/bin/tml.exe}"
# Fixtures live under scripts/fixtures/ (NOT compiler/tests/) so the suite
# runner never tries to compile these deliberately-failing files.
DIR="scripts/fixtures/borrow"

if [ ! -x "$TML" ]; then
    echo "FAIL: $TML not found (build first)" >&2
    exit 1
fi

pass=0
fail=0

# A negative fixture must FAIL to build AND mention a borrow conflict.
expect_borrow_error() {
    local name="$1"
    local file="$DIR/$name"
    local out
    out="$("$TML" build "$file" 2>&1)"
    local rc=$?
    if [ $rc -eq 0 ]; then
        printf '  FAIL  %s (expected borrow error, compiled cleanly)\n' "$name"
        fail=$((fail + 1))
        return
    fi
    if printf '%s' "$out" | grep -qiE "cannot borrow|borrowed as|more than once"; then
        printf '  PASS  %s (rejected: borrow conflict)\n' "$name"
        pass=$((pass + 1))
    else
        printf '  FAIL  %s (non-zero exit but no borrow diagnostic)\n' "$name"
        printf '        %s\n' "$out" | head -3
        fail=$((fail + 1))
    fi
}

expect_borrow_error "neg_two_mut_ref.tml"
expect_borrow_error "neg_shared_then_mut_ref.tml"
# phase26g 1.4: get_ref held across a `mut this` mutation (push-class
# invalidation) for collections whose get_ref returns `ref T` directly.
expect_borrow_error "neg_get_ref_then_push.tml"
expect_borrow_error "neg_get_ref_then_push_deque.tml"
# phase26g 1.2: the HashMap/BTreeMap variant (get_ref -> Maybe[ref V]) — the
# interior-ref borrow is now propagated through the `when Just(r)` pattern match
# and `let Just(r) = ... else` destructuring, so a mutation held across the ref
# is detected too.
expect_borrow_error "neg_get_ref_then_insert_map.tml"
expect_borrow_error "neg_get_ref_then_insert_btree.tml"

printf '\n[borrow_interior_ref] pass=%d fail=%d\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
