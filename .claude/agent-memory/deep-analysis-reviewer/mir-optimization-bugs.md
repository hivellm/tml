# MIR Optimization Pass Bugs (2026-03-01)

## Summary

The MIR optimization passes at O0 level incorrectly eliminate conditional branches and
phi nodes, causing miscompiled code that crashes at runtime.

## Bug 1: SimplifyCfg / BlockMerge Destroying Conditional Logic

### Symptom
Functions with if/else/elif control flow have their branches eliminated. The generated
LLVM IR contains unconditional branches where conditional branches should exist.

### Example: classify() from implicit_returns.test.tml

MIR (before optimization):
```
func classify(%0 x: i32) -> i32 {
entry0:
    %3 = lt %0, %2          ; x < 0
    br %3, bb1, bb2          ; conditional branch
if.then1:
    %6 = neg %5              ; -1
    br bb3
if.else2:
    %8 = eq %0, %7           ; x == 0
    br %8, bb4, bb5           ; inner conditional
if.merge3:
    %14 = phi [%6, bb1], [%13, bb6]
    return %14
```

Generated LLVM IR (after O0 optimization + codegen):
```llvm
define i32 @"classify"(i32 %x) {
entry0:
    %v2 = icmp slt i32 %x, 0
    br i1 %v2, label %if.then1, label %if.else2
if.then1:
    br label %if.merge3
if.else2:
    br label %if.merge3     ; WRONG: should go to inner if
if.merge3:
    ret i32 -1              ; WRONG: always returns -1, phi is gone
}
```

### Root Cause
One of the O0 passes (likely SimplifyCfgPass or BlockMergePass) incorrectly determined
that the inner conditional branches are dead code and eliminated them along with their
phi contributions. The phi in if.merge3 was reduced to a constant -1.

### Files
- Pass pipeline: `compiler/src/mir/mir_pass.cpp:475-514` (O0 pipeline)
- Passes to investigate: SimplifyCfgPass, BlockMergePass, MergeReturnsPass, DCE
- Codegen: `compiler/src/codegen/mir_codegen.cpp`

## Bug 2: MIR Codegen tml_main Linkage in Suite Mode

### Symptom
When multiple test files with `func main()` are compiled into one suite, they all
produce `@tml_main` with external linkage, causing duplicate symbol linker errors.

### Root Cause
`mir_codegen.cpp:784`: `is_test_entry` check marks `main` as NOT needing internal
linkage. The rename to `tml_main` happens at line 828, but the linkage was already
decided at line 788.

AST codegen at `func.cpp:583-595` does NOT exclude `main` from internal linkage.

### Fix
Change `mir_codegen.cpp:784` to NOT exclude `main` from internal linkage when
force_internal_linkage is true. The wrapper function `tml_test_N` is the only
function that needs external linkage.

## Bug 3: assert_eq Bool Type Mismatch

### Symptom
`assert_eq` called with Bool (i1) args dispatches to `assert_eq(i64, i64)` instead
of a Bool-specific variant. Calling a function expecting 64-bit args with 1-bit args
causes stack corruption.

### Root Cause
`instructions.cpp:868-877`: The dispatch logic handles ptr -> assert_eq_str,
i32 -> assert_eq_i32, but has no case for i1 (Bool). Falls through to i64 default.

### Fix
Add `assert_eq_bool` variant or extend i32 to handle i1 by zero-extending.

## Impact
- Tests that compile via MIR codegen (no TML imports, no generics) are affected
- Tests that use AST codegen (has TML imports) are NOT affected
- Exit code -1073740791 = 0xC0000409 = STATUS_STACK_BUFFER_OVERRUN
  (This is what Windows abort()/__fastfail produces)
