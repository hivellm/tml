# Proposal: Add C++ Unit Tests for Untested Compiler Components

## Status: PROPOSED

## Why

The TML compiler has 364 C++ source files but only 35 unit test files, covering approximately 41% of the codebase at file level. The most critical gaps are in the MIR optimization passes (47/49 files untested), LLVM expression codegen (32/33 untested), codegen core (16/17 untested), and derive codegen (11/11 untested). These components directly affect compiled code quality — bugs in optimization passes cause silent miscompilation, and bugs in expression codegen affect >90% of programs. While 736 TML integration tests (8,659 assertions) provide indirect coverage, C++ unit tests are essential for fast regression detection, better fault isolation, and testing internal invariants that integration tests cannot reach.

## What Changes

### 1. MIR Optimization Pass Tests (47 files, HIGHEST priority)

Add C++ unit tests for all 49 MIR passes under `compiler/src/mir/passes/`. Each test should construct a small MIR function, apply the pass, and verify the transformation. Critical passes first:

- **Constant folding/propagation** — verify constant expressions are simplified
- **Dead code elimination** (DCE, ADCE) — verify unreachable code is removed
- **Inlining** — verify small functions are inlined correctly
- **Mem2reg** — verify stack allocas are promoted to SSA registers
- **Copy propagation** — verify redundant copies are eliminated
- **SROA** — verify aggregate allocas are decomposed
- **Loop optimizations** (LICM, unroll, rotate) — verify loop transformations
- **GVN / CSE** — verify redundant computations are eliminated
- **CFG simplification** — verify empty/unreachable blocks are merged
- **Bounds check elimination** — verify provably-safe checks are removed

### 2. LLVM Expression Codegen Tests (32 files, HIGHEST priority)

Add C++ unit tests for expression codegen under `compiler/src/codegen/llvm/expr/`. Tests should verify correct LLVM IR generation for each expression type:

- **Binary/unary ops** — arithmetic, logical, comparison IR
- **Method calls** — static dispatch, dynamic dispatch, generic instantiation
- **Closures** — capture codegen, environment struct layout
- **Casts** — integer widening/narrowing, float conversion
- **Struct/tuple/enum** — field access, construction, pattern matching IR
- **Try expressions** — error propagation codegen
- **Collection literals** — array, slice initialization

### 3. Codegen Core Tests (16 files, HIGH priority)

Add tests for `compiler/src/codegen/llvm/core/`:

- **Type layout** (`types.cpp`, `types_resolve.cpp`) — struct sizes, alignment, enum discriminants
- **Generic instantiation** (`generic.cpp`) — monomorphization correctness
- **Drop glue** (`drop.cpp`) — destructor codegen, drop order
- **Dynamic dispatch** (`dyn.cpp`) — vtable layout, trait object codegen
- **Debug info** (`debug_info.cpp`) — DWARF metadata generation
- **Optimization passes** (`optimization_passes.cpp`) — LLVM pass pipeline configuration

### 4. Derive Codegen Tests (11 files, MEDIUM priority)

Add tests for `compiler/src/codegen/llvm/derive/`:

- **PartialEq/PartialOrd** — field-by-field comparison IR
- **Debug/Display** — format string generation
- **Duplicate (Clone)** — deep copy codegen
- **Hash** — hash combining logic
- **Default** — zero-initialization codegen
- **Serialize/Deserialize** — serialization IR

### 5. Control Flow Codegen Tests (4 files, MEDIUM priority)

Add tests for `compiler/src/codegen/llvm/control/`:

- **If** — branch codegen, phi nodes
- **Loop** — loop header, back-edge, break/continue
- **When (match)** — switch codegen, exhaustiveness
- **Return** — return value codegen, cleanup

### 6. Declaration Codegen Tests (4 files, MEDIUM priority)

Add tests for `compiler/src/codegen/llvm/decl/`:

- **Struct** — struct type definition, field layout
- **Enum** — tagged union layout, discriminant
- **Func** — function signature, calling convention
- **Impl** — method attachment, self parameter

### 7. CLI Infrastructure Tests (39 files, LOW priority)

Add tests for CLI components that can be unit-tested:

- **Linter** (6 files) — rule matching, diagnostic output
- **Tester** (20 files) — test discovery, suite execution logic
- **Commands** (13 files) — argument parsing, option validation

### 8. Support Component Tests (16 files, LOW priority)

- **Doc generator** (6 files) — doc extraction, HTML generation
- **MCP server** (6 files) — JSON-RPC handling, tool dispatch
- **Preprocessor** (1 file) — macro expansion, conditional evaluation
- **THIR** (3 files) — type-checked HIR lowering

## Impact

- Affected specs: None (test-only changes)
- Affected code: `compiler/tests/` (new test files only)
- Breaking change: NO
- User benefit: Higher compiler reliability, faster regression detection, fewer silent miscompilation bugs

## Dependencies

- Existing test framework in `compiler/tests/` (Google Test based)
- MIR builder API for constructing test inputs
- LLVM IR verification for codegen tests

## Success Criteria

- All 49 MIR passes have at least one dedicated unit test
- All 33 expression codegen files have at least one dedicated unit test
- All 17 codegen core files have at least one dedicated unit test
- All 11 derive files have at least one dedicated unit test
- Zero test regressions in existing suite
- C++ unit test file coverage increases from 35 to 150+ files
- File-level coverage increases from ~41% to >85%

## Risks

- **Test construction complexity**: MIR pass tests require building valid MIR programs programmatically. Mitigation: Create shared test helpers for common MIR patterns.
- **LLVM API dependency**: Codegen tests depend on LLVM C API availability. Mitigation: Use LLVM's in-memory module verification rather than executing generated code.
- **Maintenance burden**: More tests means more maintenance. Mitigation: Focus on invariant-based tests (e.g., "pass does not increase instruction count") rather than fragile output-matching tests.
- **Build time increase**: 150+ new test files will increase build time. Mitigation: Tests are compiled into the existing `tml_tests.exe` binary which already handles parallel execution.
