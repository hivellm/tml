#!/bin/bash
# Differential test: verify all type system modules type-check without errors.
# This runs `tml check` on each module and verifies zero errors.
# Phase 14c/14d — diagnostic-level differential testing.

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

echo "=== Phase 14c+14d Differential Check: Type System Modules ==="
echo ""

# Core inference engine (phase 14c)
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

# Behavior dispatch (phase 14d)
check_module "compiler-tml/src/types/behaviors/common.tml"
check_module "compiler-tml/src/types/behaviors/registry.tml"
check_module "compiler-tml/src/types/behaviors/solver.tml"
check_module "compiler-tml/src/types/behaviors/dispatch.tml"
check_module "compiler-tml/src/types/coercion.tml"

# Pipeline integration
check_module "compiler-tml/src/types/pipeline.tml"

# Module system (phase 14b)
check_module "compiler-tml/src/types/module.tml"
check_module "compiler-tml/src/types/imports.tml"

# HIR (phase 15a)
check_module "compiler-tml/src/hir/common.tml"
check_module "compiler-tml/src/hir/pattern.tml"
check_module "compiler-tml/src/hir/stmt.tml"
check_module "compiler-tml/src/hir/expr.tml"
check_module "compiler-tml/src/hir/module.tml"
check_module "compiler-tml/src/hir/builder.tml"
check_module "compiler-tml/src/hir/lower_expr.tml"
check_module "compiler-tml/src/hir/monomorph.tml"
check_module "compiler-tml/src/hir/printer.tml"

# THIR (phase 15b)
check_module "compiler-tml/src/thir/common.tml"
check_module "compiler-tml/src/thir/pattern.tml"
check_module "compiler-tml/src/thir/stmt.tml"
check_module "compiler-tml/src/thir/expr.tml"
check_module "compiler-tml/src/thir/module.tml"
check_module "compiler-tml/src/thir/lower.tml"
check_module "compiler-tml/src/thir/exhaustiveness.tml"

# MIR (phase 15c)
check_module "compiler-tml/src/mir/common.tml"
check_module "compiler-tml/src/mir/types.tml"
check_module "compiler-tml/src/mir/inst.tml"
check_module "compiler-tml/src/mir/block.tml"
check_module "compiler-tml/src/mir/module.tml"
check_module "compiler-tml/src/mir/builder/core.tml"
check_module "compiler-tml/src/mir/printer.tml"

# Test files
check_module "compiler-tml/tests/thir/thir_types.test.tml"
check_module "compiler-tml/tests/hir/hir_types.test.tml"
check_module "compiler-tml/tests/mir/mir_types.test.tml"
check_module "compiler-tml/tests/types/unify_basic.test.tml"
check_module "compiler-tml/tests/types/unify_primitives.test.tml"
check_module "compiler-tml/tests/types/infer_differential.test.tml"
check_module "compiler-tml/tests/types/differential_check.test.tml"
check_module "compiler-tml/tests/types/behavior_dispatch.test.tml"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    echo "DIFFERENTIAL CHECK FAILED"
    exit 1
fi

echo "ALL MODULES TYPE-CHECK SUCCESSFULLY"
exit 0
