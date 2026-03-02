# THIR->MIR Builder: Missing Loop Phi Nodes (2026-03-02) - ACTIVE

## Root Cause
`ThirMirBuilder::build_loop()` at `compiler/src/mir/thir_mir_builder.cpp:876-901` does NOT
create phi nodes for loop-carried variables. The function simply:
1. Emits branch to header
2. Builds condition using ORIGINAL variable values (pre-loop SSA defs)
3. Builds body (which calls set_variable but too late -- condition already emitted)
4. Emits back-edge branch without completing any phi nodes

Compare to `HirMirBuilder::build_loop()` at `compiler/src/mir/builder/hir_expr.cpp:778-920`
which correctly:
1. Saves pre-loop variables
2. Creates phi nodes for ALL pre-loop vars in the header block
3. Updates variable map to use phi results BEFORE building condition
4. After building body, completes phi back-edges with updated body values

## Cascade of Destruction (O0 pipeline)
1. Pre-opt MIR: condition uses `%1 = const i32 0` forever (no phi to select updated value)
2. InstSimplify: `sum + 0 = sum`, `0 + 1 = 1` -> body computations simplified away
3. ConstantFolding: `lt 0, 10` -> `const bool true` (condition always true)
4. SimplifyCfg: `br true, body, exit` -> eliminates exit block
5. BlockMerge: merges body into header (body is trivial after simplification)
6. DCE: removes all dead constants
7. Result: `loop.header: br loop.header` (infinite loop)

## Affected Functions
- `ThirMirBuilder::build_loop()` - `thir_mir_builder.cpp:876`
- `ThirMirBuilder::build_while()` - `thir_mir_builder.cpp:903` (same issue)
- `ThirMirBuilder::build_for()` - `thir_mir_builder.cpp:928` (likely same issue)

## Affected Tests
ANY test with `loop (cond) {}` or `while (cond) {}` compiled via THIR path (default)
- volatile.test.tml
- bounds_check_elim.test.tml
- ALL loop-based tests using the default (non-legacy) pipeline

## Key Facts
- `CompilerOptions::use_thir = true` by default (`compiler/include/common.hpp:151`)
- `--legacy` flag switches to HIR path which works correctly
- AST codegen (legacy path) uses alloca/load/store pattern, avoids SSA phi issues
- MIR codegen requires phi nodes for correct SSA at loop boundaries

## Fix Location
`ThirMirBuilder::build_loop()` in `compiler/src/mir/thir_mir_builder.cpp`
Must add logic mirroring `HirMirBuilder::build_loop()`:
1. Save pre-loop variables
2. Create phi nodes in header for all pre-loop vars
3. Update variable map to phi results before building condition
4. After body, complete phi back-edges with updated values
Same fix needed for `build_while()` and `build_for()`.
