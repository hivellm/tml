#!/bin/bash
# Differential test: verify all inference modules type-check without errors.
# This runs `tml check` on each module and verifies zero errors.
# Phase 14c — item 6.1 (type-check level differential testing).

set -e

TML="./build/debug/bin/tml.exe"
PASS=0
FAIL=0

check_module() {
    local file="$1"
    if $TML check "$file" 2>&1; then
        echo "  PASS: $file"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $file"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Phase 14c Differential Check: Inference Modules ==="
echo ""

# Core inference engine
check_module "compiler-tml/src/types/infer/common.tml"
check_module "compiler-tml/src/types/infer/unify.tml"

# Expression checker
check_module "compiler-tml/src/types/checker/check_expr.tml"

# Call resolution
check_module "compiler-tml/src/types/checker/check_call.tml"

# Statement checker
check_module "compiler-tml/src/types/checker/check_stmt.tml"

# Pattern checker
check_module "compiler-tml/src/types/checker/check_pattern.tml"

# Error types
check_module "compiler-tml/src/types/checker/errors.tml"

# Foundation types (from phase14a)
check_module "compiler-tml/src/types/ty.tml"
check_module "compiler-tml/src/types/env.tml"
check_module "compiler-tml/src/types/builtins.tml"
check_module "compiler-tml/src/types/register.tml"

# Test files
check_module "compiler-tml/tests/types/unify_basic.test.tml"
check_module "compiler-tml/tests/types/unify_primitives.test.tml"
check_module "compiler-tml/tests/types/infer_differential.test.tml"
check_module "compiler-tml/tests/types/differential_check.test.tml"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    echo "DIFFERENTIAL CHECK FAILED"
    exit 1
fi

echo "ALL MODULES TYPE-CHECK SUCCESSFULLY"
exit 0
