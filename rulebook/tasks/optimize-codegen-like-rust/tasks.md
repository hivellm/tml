# Tasks: Optimize TML Codegen Using Rust as Reference

**Status**: In Progress (75%)

> **NOTE**: This task is a living document. It gets incrementally updated as we discover codegen issues during other work (iterators, closures, generics, etc.). Dedicated execution of these phases will happen later when the compiler is stable. For now, findings from IR comparisons are recorded here for future reference.

## Phase 1: Enum/Maybe Layout Optimization

- [x] 1.1 Specialize `Maybe[T]` layout for primitive types (I32, I64, Bool, F32, F64) to use `{ i32, T }` instead of `{ i32, [1 x i64] }`
- [ ] 1.2 Specialize `Maybe[T]` layout for pointer types (Str, ref T) to use nullable pointer (null = Nothing) with no tag
- [x] 1.3 Specialize `Outcome[T, E]` layout for primitive types similarly
- [x] 1.4 Ensure `when` pattern matching codegen handles both compact and generic layouts
- [x] 1.5 Run full test suite to verify no regressions (10,281 pass, 0 fail)

## Phase 2: Eliminate Redundant Alloca in Constructors

- [x] 2.1 Use `insertvalue` directly for struct construction when all fields are known at init time
- [x] 2.2 Use `extractvalue` directly for struct field reads on SSA values (not via GEP+load)
- [x] 2.3 Eliminate alloca+store+load pattern for function parameters in simple functions
- [x] 2.4 Keep alloca path for mutable locals and complex patterns (address-taken, closures, arrays, generics)
- [x] 2.5 Compare IR output with Rust for `new()`, `next()`, and similar functions

## Phase 3: Dead Declaration Elimination (DONE)

- [x] 3.1 Track which runtime functions are actually referenced during codegen
- [x] 3.2 Only emit `declare` for functions that appear in at least one `call`/`invoke` instruction
- [x] 3.3 Move runtime declarations from hardcoded preamble to on-demand emission (catalog system in runtime.cpp)
- [x] 3.4 Verify no link-time errors from missing declarations

## Phase 4: Checked Arithmetic (Debug Mode)

- [x] 4.1 Add `--checked-math` / `--no-checked-math` compiler flags (enabled at O0, disabled at O1+)
- [x] 4.2 Replace `add nsw` with `@llvm.sadd.with.overflow` + branch to panic for signed integers
- [x] 4.3 Replace `add nuw` with `@llvm.uadd.with.overflow` + branch to panic for unsigned integers
- [x] 4.4 Extend to sub (ssub/usub) and mul (smul/umul) with overflow intrinsics
- [x] 4.5 Emit source location (file:line) in panic message instead of generic string
- [x] 4.6 Works for all integer types (i8, i16, i32, i64 — intrinsic is type-polymorphic)

## Phase 5: Reduce Redundant Loads

- [x] 5.1 Cache struct field loads within the same basic block — handled by MIR load_store_opt pass (MemState tracking)
- [x] 5.2 Use SSA phi nodes instead of alloca+load for loop variables — handled by MIR mem2reg pass
- [x] 5.3 Eliminate dead `load` instructions after `store` to same alloca — handled by MIR load_store_opt + DSE passes
- [x] 5.4 Compare loop IR with Rust's SSA-form loops — MIR passes produce equivalent SSA at O0

## Phase 6: Exception Handling Foundation

- [ ] 6.1 Research Windows SEH vs DWARF unwinding requirements for TML
- [ ] 6.2 Add `invoke` + `cleanuppad` for calls in functions with destructors (Drop behavior)
- [ ] 6.3 Implement cleanup landing pads that call `drop()` for stack-allocated values
- [ ] 6.4 Add `personality` function declaration (`__CxxFrameHandler3` on Windows, `__gxx_personality_v0` on Linux)
- [ ] 6.5 Compare panic propagation IR with Rust's unwind tables

## Phase 7: Naming and Metadata

- [x] 7.1 Add `source_filename` and `target datalayout` to emitted IR modules
- [x] 7.2 Add `!llvm.ident` metadata with TML compiler version
- [ ] 7.3 Add proper `align` annotations to all `alloca`, `load`, `store` instructions (820 sites across 44 files — deferred)
- [x] 7.4 Use `inbounds` GEP consistently — added to all 372 GEP emit sites across 51 codegen files

## Validation

- [ ] Full test suite passes after each phase
- [ ] IR comparison with Rust shows parity for core patterns
- [ ] No regressions in compile time
- [ ] Coverage report maintained or improved
