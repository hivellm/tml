# Recommended Changes: Prioritized Action Plan

## Guiding Principles

1. **Incremental** — Each change must be independently testable
2. **Backward compatible** — No test regressions during migration
3. **High ROI first** — Fixes that eliminate the most recurring bugs first
4. **Aligned with self-hosting** — Changes that also prepare for the TML-in-TML compiler rewrite

## Priority 1: Eliminate i32 Fallbacks (CRITICAL)

**Problem**: 23 sites silently fall back to `i32` when type info is missing
**Impact**: Causes silent data corruption, wrong ABI decisions, mysterious crashes
**Effort**: Low (1-2 days)

### What to Do

1. Add a MIR validation pass that runs after MIR building, before codegen:

```cpp
// compiler/src/mir/mir_validate.cpp
void validate_mir_types(const mir::Module& module) {
    for (const auto& func : module.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                // Every non-void instruction MUST have a type
                if (inst.result != mir::INVALID_VALUE && !inst.type) {
                    TML_LOG_ERROR("mir",
                        "BUG: instruction " << inst.result
                        << " in " << func.name
                        << " has no type annotation");
                    // In debug builds: assert(false)
                    // In release builds: log and continue (for now)
                }
            }
        }
    }
}
```

2. In codegen, replace `make_i32_type()` fallbacks with warnings that identify the source:

```cpp
// Before:
mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();

// After:
mir::MirTypePtr type_ptr = i.result_type;
if (!type_ptr) {
    TML_LOG_WARN("codegen", "Missing type on instruction " << inst.result
                 << " in " << current_func_ << ", falling back to i32");
    type_ptr = mir::make_i32_type();
}
```

3. Fix the top MIR builders to always set types (driven by the warnings)

### Expected Result

Turn 23 silent bugs into 23 visible warnings, then fix them upstream in the MIR builders. Over time, the warnings go to zero and can become hard errors.

## Priority 2: Create ABI Module (HIGH)

**Problem**: Win64 ABI decisions scattered across 10+ sites
**Impact**: Monthly struct-passing bugs, sret mismatches
**Effort**: Medium (3-5 days)

### What to Do

1. Create `compiler/include/codegen/abi.hpp` with `FnABI`, `ArgABI`, `PassMode`
2. Create `compiler/src/codegen/abi.cpp` with `compute_fn_abi()`
3. In `emit_function()`: compute FnABI once, store in function context
4. In `emit_function_declaration()`: use same computation
5. In `emit_call_inst()`: look up callee's FnABI
6. Remove all `starts_with("%struct.")` checks from codegen (replace with `ArgABI.mode`)

### Migration Strategy

- Phase A: Create ABI module, compute FnABI, but DON'T use it yet (just verify it matches current behavior)
- Phase B: Switch `emit_function_declaration()` to use FnABI
- Phase C: Switch `emit_call_inst()` to use FnABI
- Phase D: Remove the string-based aggregate checks

## Priority 3: Introduce CGValue (HIGH)

**Problem**: No value/pointer distinction, 61 side-table lookups
**Impact**: Weekly type mismatch bugs, unnecessary spills
**Effort**: Medium (3-5 days)

### What to Do

1. Create `compiler/include/codegen/cg_value.hpp` with `CGValue`, `CGValueKind`
2. Make `emit_instruction()` populate a `CGValue` for each result
3. Add `cg_values_` map alongside `value_regs_` (transition period)
4. Convert call site argument processing to use CGValue
5. Gradually remove `value_types_` lookups as CGValue takes over
6. Remove `value_types_` map entirely

### CGValue Design

```cpp
struct CGValue {
    std::string reg;
    std::string llvm_type;
    CGValueKind kind;  // Immediate, Address, FatPointer, ZeroSized
    mir::MirTypePtr mir_type;

    // Convenience
    bool needs_spill_for_indirect() const {
        return kind == CGValueKind::Immediate && is_aggregate();
    }

    // Convert to pointer (alloca + store)
    CGValue to_address(MirCodegen& cg) const {
        if (kind == CGValueKind::Address) return *this;
        if (kind == CGValueKind::ZeroSized) return *this;
        std::string alloca = cg.new_temp();
        cg.emitln("    " + alloca + " = alloca " + llvm_type + ", align 8");
        cg.emitln("    store " + llvm_type + " " + reg + ", ptr " + alloca);
        return CGValue{alloca, "ptr", CGValueKind::Address, mir_type};
    }
};
```

## Priority 4: Table-Driven Intrinsics (MEDIUM)

**Problem**: 1,357 LOC of if/else string matching
**Impact**: Difficult to add new intrinsics, duplicated type resolution
**Effort**: Medium (2-3 days)

### What to Do

1. Create `IntrinsicKind` enum with all known intrinsics
2. Create lookup table: function name → IntrinsicKind
3. Create shared `resolve_element_type()` helper
4. Create shared `ensure_ptr_value()` helper
5. Rewrite `emit_call_inst()` to dispatch on IntrinsicKind
6. Each intrinsic becomes a separate method: `emit_intrinsic_ptr_read()`, etc.

### Expected Result

`instructions_call.cpp` shrinks from 1,357 to ~800 lines. Adding a new intrinsic requires: (1) add enum value, (2) add table entry, (3) write handler method. No more copying 50-line blocks.

## Priority 5: Typed Emit Helpers (MEDIUM)

**Problem**: Raw string concatenation for IR emission, no validation
**Impact**: Typos and malformed IR caught late at LLVM parse time
**Effort**: Low (1-2 days)

### What to Do

1. Create `IREmitter` class with typed helpers (see 06-IR-GENERATION-STRATEGY.md)
2. Add `IREmitter` as a member of `MirCodegen`
3. Convert emit calls incrementally: start with the most error-prone patterns (load, store, GEP, call)
4. Each helper includes assertions for common mistakes (void loads, empty types, null pointers)

## Priority 6: Unit Type Cleanup (MEDIUM)

**Problem**: 61+ comparisons against `"void"` scattered through codegen
**Impact**: Occasional Unit-type bugs (store void, load void, alloca void)
**Effort**: Low (1 day)

### What to Do

1. In `mir_primitive_to_llvm()`, map `Unit` to `"{}"` instead of `"void"`
2. Use `"void"` ONLY for function return types
3. This eliminates ALL the `if (type_str == "void") { type_str = "{}"; }` patches
4. The distinction: `void` is a LLVM IR return type, `{}` is a LLVM IR value type

### Before (current):
```cpp
// 15+ sites like this:
std::string type_str = mir_type_to_llvm(type_ptr);
if (type_str == "void") {
    type_str = "{}";  // LLVM doesn't allow void values
}
```

### After:
```cpp
// mir_types.cpp — change ONE line
case mir::PrimitiveType::Unit:
    return "{}";  // Unit is zero-sized struct, not void

// Only emit "void" for function return types:
std::string ret = is_unit_return ? "void" : mir_type_to_llvm(func.return_type);
```

## Priority 7: Remove Legacy Codegen (LOW, LONG-TERM)

**Problem**: Two parallel codegen paths (AST→LLVM and MIR→LLVM)
**Impact**: Monthly dual-path divergence bugs
**Effort**: Very high (ongoing)

### What to Do

1. Track which tests still require legacy codegen (`--legacy` flag)
2. Fix MIR codegen to handle those cases
3. When no tests require `--legacy`, remove the AST codegen entirely
4. This removes ~90 files under `compiler/src/codegen/llvm/`

### Current State

The legacy codegen is still used for some complex patterns. Removing it requires the MIR codegen to handle every case the legacy codegen handles. This is an ongoing process.

## Implementation Timeline

| Priority | Change | Days | Can Parallel? |
|----------|--------|------|---------------|
| P1 | Eliminate i32 fallbacks | 1-2 | Yes |
| P2 | Create ABI module | 3-5 | Yes (with P1) |
| P3 | Introduce CGValue | 3-5 | After P1 |
| P4 | Table-driven intrinsics | 2-3 | Yes (with P2, P3) |
| P5 | Typed emit helpers | 1-2 | Yes (anytime) |
| P6 | Unit type cleanup | 1 | After P3 |
| P7 | Remove legacy codegen | Ongoing | After P1-P6 |

**Total for P1-P6**: ~12-18 days of focused work

## Expected Impact

After P1-P6:
- **i32 fallback bugs**: Eliminated (hard errors instead of silent bugs)
- **ABI mismatch bugs**: Eliminated (centralized, tested once)
- **Value/pointer ambiguity bugs**: Eliminated (CGValue tracks kind)
- **Intrinsic dispatch bugs**: Reduced (table-driven, shared helpers)
- **Malformed IR bugs**: Reduced (typed helpers with assertions)
- **Unit type bugs**: Eliminated (consistent `{}` representation)
- **Codebase size**: ~800 lines smaller (net, from intrinsic refactor alone)
- **New feature velocity**: Significantly faster (clear patterns to follow)
