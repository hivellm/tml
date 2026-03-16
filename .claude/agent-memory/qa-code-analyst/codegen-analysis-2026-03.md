---
name: codegen-analysis-march-2026
description: Comprehensive QA analysis of TML compiler codegen (legacy LLVMIRGen + MIR path), patterns, anti-patterns, and technical debt
type: project
---

# TML Codegen Analysis — March 2026

**Why:** Reference for future QA sessions, improvement planning, and task creation.

**How to apply:** When working on codegen bugs, improvements, or refactoring, consult this file first.

---

## Two Codegen Paths (CRITICAL UNDERSTANDING)

### Primary: MIR → LLVM IR (MirCodegen)
- **Files:** `compiler/src/codegen/mir_codegen.cpp` + `compiler/src/codegen/mir/` (~3075 lines)
- **Used by:** Query-based pipeline (`query_core.cpp:666`) — the DEFAULT for all normal builds
- **Pipeline:** Source → THIR → MIR → `MirCodegen::generate()` → LLVM IR → `.obj`
- **Quality:** MIR is SSA form, so it maps naturally to LLVM IR without manual SSA construction
- **Size:** ~3075 lines across 6 files — lean and manageable

### Legacy (Fallback): AST → LLVM IR (LLVMIRGen)
- **Files:** `compiler/src/codegen/llvm/` — ~49,000 lines across 60+ files
- **Used by:** `--legacy` CLI flag, `compile_ast()` in `LLVMCodegenBackend`
- **Still active:** Test suite compilation via `parallel_build.cpp:647`, `build.cpp:506`, `run_profiled.cpp:184`
- **Key insight:** The library module codegen (emit_module_pure_tml_functions) still uses LLVMIRGen

### KEY: Legacy is not truly "legacy" yet
LLVMIRGen is still the active path for library IR generation in test builds.
The `CodegenLibraryState` capture/restore mechanism bridges the two: lib state
computed once by LLVMIRGen, restored into multiple MirCodegen worker instances.

---

## Critical Architecture Patterns Found

### 1. God Class Anti-Pattern — LLVMIRGen
- **File:** `compiler/include/codegen/llvm/llvm_ir_gen.hpp`
- **Size:** 1804 lines, 475 field/method declarations
- **State count:** 50+ mutable instance fields including:
  - `last_expr_type_`, `last_semantic_type_`, `expected_enum_type_`, `expected_literal_type_`
  - `current_impl_type_`, `current_type_subs_`, `current_where_constraints_`
  - `current_func_`, `current_block_`, `current_loop_*` (multiple fields)
  - `closure_return_alloca_/type_/label_` (inline closure state)
  - `comptime_loop_var_/type_/value_` (compile-time loop state)
- **Risk:** Subtle state leakage bugs (confirmed: `last_semantic_type_` leaked across
  function boundaries — fixed in commit bee67287, but root cause still present)

### 2. `parse_mangled_type_string` Duplication — HIGH DEBT
- **Copies:** 9+ copies across 9 files:
  - `call.cpp`, `method.cpp`, `call_generic_struct.cpp`, `llvm_struct_expr.cpp`
  - `infer.cpp`, `unary.cpp`, `method_static_dispatch.cpp`
  - `impl.cpp`, `core/generic.cpp`, `core/runtime_modules.cpp`
- Each copy is slightly different (some handle `mutptr_`, `ref_`, `mutref_` prefixes; others don't)
- This function should be in a single shared utility — possibly as `LLVMIRGen::parse_mangled_type()`
  or a free function in a shared header.
- **Fix effort:** Medium (1-4 hours) — consolidate to one canonical impl

### 3. `get_const_llvm_type` Duplication
- **Copies:** 3 copies in `generate.cpp`, `generate_support.cpp`, `runtime_modules.cpp`
- **Fix effort:** Small

### 4. String-Based Type Encoding (Pervasive)
- Type information is encoded as LLVM type strings (`"i32"`, `"%struct.Maybe__I32"`)
  and decoded on demand via string parsing
- 64 string ops (find/substr/starts_with) in generic.cpp alone, 60 in call.cpp
- Type checking at codegen time requires reparsing mangled names — O(n) string work per type resolution
- Correct architecture: carry `types::TypePtr` through the full pipeline (MIR path does this)
- This is the root cause of many `last_semantic_type_` bugs

### 5. Fat Pointer Closure — Runtime Dispatch Branch
- Every call through a function pointer stored in a struct field emits:
  icmp null + branch + phi for thin/fat pointer
- This is 5 extra instructions per closure call
- Rust avoids this by using distinct function pointer types at compile time
- Fix: track fat-ness in type system; emit direct call for known-non-capturing closures

### 6. Closure Heap Allocation — Always malloc
- `closure.cpp:372` — capturing closures always heap-allocate the env struct via `@malloc`
- No stack allocation even for closures with single-owner lifetimes
- Rust uses stack allocation when closure lifetime is bounded

### 7. Alloca Without Alignment — Phase 7.3 Deferred
- 284 `= alloca` instructions emitted without `align` annotation
- LLVM can figure it out but it's technically non-conformant IR
- Task notes this affects 820 sites across 44 files — `optimize-codegen-like-rust` Phase 7.3

### 8. Monomorphization Loop — potential infinite growth
- `generate_pending_instantiations()` in `generic.cpp` loops until stable
- Uses separate pending queues (`pending_func_keys_`, `pending_class_keys_`) to avoid O(n^2) scan
- But `pending_impl_method_instantiations_` is still a full vector processed in batches
- No cycle detection — pathological recursive generics could loop indefinitely

### 9. Error Recovery — Returns "0" Sentinel
- On codegen error: `report_error(...)` + `return "0"` (a valid LLVM integer constant!)
- This means codegen continues producing (possibly invalid) IR after errors
- In 9 places in call.cpp alone
- Better pattern: return `std::optional<std::string>` or use a poisoned value

### 10. `collect_codegen_captures` — Incomplete Visitor
- Only handles ~12 expression types; missing: `StructInitExpr`, `TupleExpr`, `CastExpr`,
  `LoopExpr`, `TryExpr`, nested closures
- Works because library closures fallback to type checker captures
- But user closures skipping type checker (in some paths) may miss captures

---

## IR Quality Status (from optimize-codegen-like-rust task)

Phases 1-5 COMPLETE:
- Maybe[T] compact layout: `{ i32, i32 }` for primitives ✓
- Struct insertvalue (no alloca for constructors) ✓
- Dead declaration elimination (on-demand via catalog) ✓
- Checked arithmetic (`@llvm.sadd.with.overflow`) ✓
- MIR load_store_opt + mem2reg passes ✓

Phase 6 (Exception Handling) INCOMPLETE:
- No `invoke`/`cleanuppad` — panics can't run Drop
- Bare `call` everywhere — no stack unwinding
- No personality function

Phase 7.3 INCOMPLETE:
- Missing `align` on 284 alloca/load/store sites

---

## Key Technical Debt Items (Prioritized)

| Priority | Issue | Location | Effort |
|----------|-------|----------|--------|
| HIGH | parse_mangled_type_string x9 copies | 9 codegen files | Medium |
| HIGH | God class LLVMIRGen (50+ state fields) | llvm_ir_gen.hpp | XL |
| HIGH | String-based type encoding throughout legacy path | all expr/ files | XL |
| MEDIUM | Error returns "0" sentinel (invalid recovery) | call.cpp, method.cpp | Medium |
| MEDIUM | Missing align annotations (284 sites) | all codegen files | Medium |
| MEDIUM | Closure env always heap-allocated | closure.cpp | Medium |
| MEDIUM | Fat ptr branch on every func-ptr call | call.cpp:192-272 | Medium |
| LOW | collect_codegen_captures incomplete visitor | closure.cpp | Small |
| LOW | get_const_llvm_type x3 copies | 3 core/ files | Small |

---

## File Reference
- Legacy codegen entry: `compiler/src/codegen/llvm/llvm_ir_gen.cpp`
- MIR codegen entry: `compiler/src/codegen/mir_codegen.cpp`
- Path selection: `compiler/src/codegen/llvm/llvm_codegen_backend.cpp` (compile_mir vs compile_ast)
- Query entry: `compiler/src/query/query_core.cpp:666` (always uses compile_mir)
- Header (god class): `compiler/include/codegen/llvm/llvm_ir_gen.hpp`
