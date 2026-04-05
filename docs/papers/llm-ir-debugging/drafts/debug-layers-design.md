# Multi-Layer Debug Output Design

## Overview

When a test fails or a compilation error occurs, the compiler can optionally emit diagnostic information from multiple compilation layers. This gives the LLM (or human) a complete picture of what happened at each stage, eliminating the need to manually invoke `emit-ir`, `emit-mir`, read source files, etc.

## Flag: `--debug-layers`

### Usage

```bash
tml test --debug-layers                    # All layers on failure
tml test --debug-layers=mir,ir             # Only MIR + LLVM IR
tml build file.tml --debug-layers          # For compilation errors
tml check file.tml --debug-layers          # For type errors
```

### MCP Integration

```json
{
  "tool": "test",
  "params": {
    "path": "lib/core/tests/str/basic.test.tml",
    "debug_layers": true
  }
}
```

Or selective:
```json
{
  "params": {
    "debug_layers": ["hir", "mir", "ir"]
  }
}
```

## Layer Output Structure

On failure, each layer emits a scoped diagnostic block. The output is structured so the LLM can identify which layer the bug originates from.

### Example: Test Failure with --debug-layers

```
FAIL: test_str_repeat (lib/core/tests/str/repeat.test.tml:15)
  Expected: "abcabc"
  Got:      ""

=== SOURCE (line 15) ===
  let result = "abc".repeat(2)

=== HIR ===
  %0 = call str::repeat(%self: &str "abc", %n: I64 2) -> Text
  ; type: Text (heap-allocated, drop-on-scope-exit)

=== MIR (basic block) ===
  bb0:
    %1 = const_str "abc"
    %2 = const_i64 2
    %3 = call @str_repeat(%1, %2) -> %Text
    store %3 -> _result
    br bb1

=== LLVM IR (function) ===
  define void @test_str_repeat() {
  entry:
    %0 = call { ptr, i64 } @tml_str_repeat(ptr @.str.abc, i64 3, i64 2)
    ; NOTE: passing length=3, count=2 to C runtime
    ...
  }

=== DIAGNOSIS HINTS ===
  Layer: CODEGEN (MIR → LLVM IR)
  Issue: str_repeat returns empty string
  Possible causes:
    - ABI mismatch between TML call convention and C runtime signature
    - sret vs direct return confusion
    - String pointer not passed correctly
```

## Layer Descriptions

### 1. SOURCE
- **What**: The exact source line(s) that failed
- **When**: Always emitted
- **Size**: 1-5 lines

### 2. HIR (High-level IR)
- **What**: Desugared, type-resolved expression
- **When**: On type errors, behavior dispatch errors, generic instantiation errors
- **Shows**: Resolved types, monomorphized generics, desugared syntax
- **Size**: 5-20 lines (expression scope)
- **LLM value**: Reveals type inference decisions, whether the right overload was selected

### 3. THIR (Typed HIR)
- **What**: Post-trait-resolution, post-coercion IR
- **When**: On method resolution errors, operator dispatch errors
- **Shows**: Resolved trait implementations, inserted coercions
- **Size**: 5-20 lines
- **LLM value**: Reveals whether the correct impl was selected, if coercions are correct

### 4. MIR (Mid-level IR)
- **What**: SSA form with basic blocks, explicit control flow
- **When**: On borrow check errors, lifetime errors, optimization bugs
- **Shows**: Variable lifetimes, drop points, control flow graph
- **Size**: 10-50 lines (function scope)
- **LLM value**: Reveals data flow, where values are live, drop ordering

### 5. LLVM IR
- **What**: Final IR before machine code generation
- **When**: On codegen bugs, runtime crashes, ABI mismatches
- **Shows**: Exact instructions, types, calling conventions
- **Size**: 20-100 lines (function scope)
- **LLM value**: Pattern-matchable, directly comparable to Rust's output, reveals exact ABI

### 6. DIAGNOSIS HINTS (Experimental)
- **What**: Compiler-generated hints about which layer likely contains the bug
- **When**: When the compiler can detect inconsistencies between layers
- **Shows**: Layer name, symptom description, possible causes
- **Size**: 3-10 lines
- **LLM value**: Reduces search space — LLM knows which layer to focus on

## Implementation Plan

### Phase 1: Flag Parsing + LLVM IR on Failure (Simplest)

1. Add `--debug-layers` to CLI argument parser
2. On test failure, re-compile the failing test with `--emit-ir`
3. Extract the function containing the failure
4. Append to test output

### Phase 2: MIR Output on Failure

1. Add MIR printer that can scope to a single function
2. On test failure, emit the MIR for the failing function
3. Requires: mapping test function name → MIR function

### Phase 3: HIR/THIR Output on Failure

1. Add HIR/THIR pretty-printer scoped to expression
2. On type errors, emit the HIR for the failing expression
3. Requires: source location → HIR node mapping

### Phase 4: Diagnosis Hints

1. Implement cross-layer consistency checks
2. Compare HIR types vs MIR types vs LLVM IR types
3. Flag mismatches as hints

## Comparison Protocol

To measure the impact of `--debug-layers`:

### Without (Condition A)
```
LLM sees: "FAIL: test_str_repeat — Expected 'abcabc', Got ''"
LLM does: Read source → emit-ir → Read C runtime → identify ABI mismatch → fix
Tool calls: ~6-10
```

### With (Condition B)
```
LLM sees: Full multi-layer output including LLVM IR showing the ABI mismatch
LLM does: Read LLVM IR in output → identify ABI mismatch → fix
Tool calls: ~2-3
```

### Measurement
- Count tool calls per bug fix
- Count file reads (proxy for "searching")
- Count emit-ir/emit-mir calls (proxy for "already got it from debug-layers")
- Time from first failure to correct fix
