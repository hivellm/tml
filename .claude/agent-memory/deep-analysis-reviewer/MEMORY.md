# Deep Analysis Reviewer Memory

## Recurring bug patterns
- [Heap-borrow-drop pattern](heap-borrow-drop-pattern.md) — non-deterministic crashes in cc/* from `Heap[T]` aliasing; flat List + offset table or `Shared[T]` are the structural fixes
- [Shared.get aliasing](shared-get-aliasing.md) — `Shared[T].get()` returns T by bitwise copy; nested Shared fields aliased without refcount bumps. Use-after-free on drop. phase24k-discovered residual bug class after phase24g Shared migration. Naive .duplicate() workarounds REGRESS — needs structural fix in Shared::get or codegen.

## Key Debugging Insights

### Type Checker: Hardcoded Builtin Types Method List (2026-04-06) - PARTIALLY FIXED
- `expr_call_method_types.cpp`: Hardcoded method lists for Maybe[T], Outcome[T,E], etc.
- Methods NOT in the list silently return `make_unit()` (line 1360 of expr_call_method.cpp)
- Fixed: Added 15+ missing Maybe methods (as_ref, as_mut, inspect, take, zip, etc.)
- Fixed: Added 6+ missing Outcome methods (inspect, inspect_err, as_ref, as_mut, etc.)
- REMAINING: `MaybeIter::next()` needs behavior impl lookup for custom types

### AST Codegen: Function Pointer Field Calls in Generic Impls (2026-04-06) - FIXED
- Section 17 of method.cpp: `this.f(args)` where `f` is struct field with func type
- Bug: `current_type_subs_["F"]` mapped to `Fn` (NamedType alias), NOT actual FuncType
- Fix: Extract concrete types from `Self/This` substitution's type_args
- Also added module registry fallback for struct lookup
- Recovered: ALL iterator adapters (Map, Filter, Inspect, Scan, etc.)

### Register Prefix Mapping (MIR vs AST Codegen)
- `%v` prefix -> MIR codegen (`compiler/src/codegen/mir/codegen_helpers.cpp:44`)
- `%t` prefix -> AST/LLVMIRGen codegen (`compiler/src/codegen/llvm/core/llvm_utils.cpp:39`)
- `%ext` / `%trunc` -> coercion registers in MIR binary instruction handler
- `%spill` -> struct-to-ptr spill registers

### Compilation Pipelines
- `tml build --emit-ir` (default query path): STILL uses MIR codegen! `--emit-ir` only skips MIR in `--legacy` mode
- `tml build --emit-ir --legacy`: uses AST codegen (LLVMIRGen, `%t` prefix)
- `tml build` (default query path): uses MIR codegen via `codegen_unit()` when no imports/generics
- Test runner uses `codegen_unit()` -> same MIR codegen path as `tml build`
- MIR codegen registers: `%v` (value_regs), `%t` (new_temp), `%insert`/`%ext`/`%trunc` (coercion)
- **CRITICAL**: Both `tml build` and test runner use the SAME codegen path (MIR via codegen_unit)

### THIR->MIR Builder: Missing Loop Phi Nodes (2026-03-02) - ACTIVE (CRITICAL)
- See `thir-loop-phi-bug.md` for full analysis
- `ThirMirBuilder::build_loop()` at `thir_mir_builder.cpp:876-901` has NO phi node creation
- Compare `HirMirBuilder::build_loop()` at `hir_expr.cpp:778-920` which does it correctly
- Loop condition is emitted with pre-loop variable values (always initial values)
- Body mutations update variable map but condition was already emitted
- Cascade: ConstantFolding folds `lt 0, 10` to `true` -> SimplifyCfg removes exit -> infinite loop
- `use_thir = true` by default (`common.hpp:151`), so ALL non-legacy builds affected
- Affects: volatile.test.tml, bounds_check_elim.test.tml, ALL tests with loops via THIR path
- Same issue in `build_while()` (line 903) and `build_for()` (line 928)

### MIR O0 Optimization Pass Bug (2026-03-01) - ACTIVE
- See `mir-optimization-bugs.md` for full analysis
- O0 pipeline at `query_core.cpp:447` applies SimplifyCfgPass, BlockMergePass, MergeReturnsPass
- These passes INCORRECTLY eliminate conditional branches and phi nodes
- Result: functions like `classify()` always return one branch's value, ignoring conditions
- Causes abort() -> __fastfail -> STATUS_STACK_BUFFER_OVERRUN (0xC0000409 = -1073740791)
- Affected tests: implicit_returns.test.tml, likely others with conditional logic
- NOTE: default_field_values.test.tml is NOT affected by this bug; it has a separate root cause (missing default field materialization)

### MIR Codegen Linkage Bug in Suite Mode (2026-03-01) - ACTIVE
- `mir_codegen.cpp:784-789`: `main` excluded from internal linkage when `force_internal_linkage=true`
- AST codegen (`func.cpp:583-595`) correctly gives `main` internal linkage in suite mode
- MIR codegen makes `tml_main` external -> duplicate symbols when multiple test files have main()

### MIR Codegen assert_eq Bool Dispatch (2026-03-01) - FIXED
- `instructions.cpp:873-883`: Bool (i1) dispatch now zero-extends to i32 and calls assert_eq_i32
- BUT: uses `emit()` (no newline) for zext, producing multi-instruction lines in IR text
- This is cosmetic only -- LLVM parser is token-based, handles it fine

### MIR Codegen: Default Field Values Not Materialized (2026-03-02) - ACTIVE
- When struct literal omits fields with defaults (e.g., `Config { name: "x" }`), MIR never inserts defaults
- Root cause: entire pipeline from HIR -> THIR -> MIR -> codegen just iterates user-provided fields
- `hir_builder_expr.cpp:670-673`: HIR passes through parser's fields without inserting defaults
- `thir_lower.cpp:391`: THIR passes through HIR's fields without inserting defaults
- `thir_mir_builder_expr.cpp:28-32`: MIR builder iterates s.fields, no default injection
- `hir_expr_control.cpp:178-182`: HirMirBuilder also iterates s.fields, no default injection
- `instructions_misc.cpp:318`: MIR codegen emits insertvalue only for provided fields
- Result: uninitialized struct fields are `undef` in LLVM IR
- `undef` causes assert_eq failures (abort) -> exit -1073740791 (STATUS_STACK_BUFFER_OVERRUN)
- Standalone exe may pass by coincidence (LLVM can fold undef comparisons)
- Test runner DLL consistently crashes due to different memory layout
- AST codegen handles defaults correctly at `llvm_struct_expr.cpp:789-859` (second pass)
- Fix location: HIR builder or THIR->MIR builder should inject defaults from struct declaration
- Affects: default_field_values.test.tml, any test with struct default field values

### Coverage System Architecture (2026-02-28)
- See `coverage-gaps.md` for full analysis
- **Source extraction**: `extract_functions()` in `testing_coverage.cpp` uses regex
- **Runtime tracking**: `tml_cover_func()` in `lib/test/runtime/coverage.c`
- **Codegen instrumentation**: `emit_coverage()` in AST codegen only, NOT MIR

### THIR Coercion Bug Pattern (2026-02-28)
- **File**: `compiler/src/thir/thir_lower.cpp`, lines 828-840
- **Bug**: `needs_coercion()` returns IntWidening without checking bit widths

### Unit Type "alloca void" Codegen Bug (2026-03-07) - FIXED
- Root cause: `mir_primitive_to_llvm(Unit)` returns `"void"` but void is only valid as return type
- Fix: Added void->`"{}"` guards at 8 emission sites across MIR codegen + AST codegen `for_data=true`
- Files modified: `instructions.cpp` (load/store/alloca/call/tuple), `mir_codegen.cpp` (decl/def params),
  `instructions_method.cpp` (method args/receiver), `instructions_misc.cpp` (phi/ConstUnit/TupleInit),
  `terminators.cpp` (ret), `llvm_ir_gen_stmt.cpp` (let), `call_user.cpp` (call params)
- Test suite: 1379/1380 passed, no regressions

### Maybe[Unit] GEP Payload Bug (2026-03-07) - FIXED
- Root cause: `when.cpp` GEP extracted payload at `i32 0, i32 1` but `Maybe[Unit] = { i32 }` has no index 1
- Fix: `when.cpp:617-657` — detect Unit payload type, skip GEP, use `null` as dummy payload_ptr
- Handles Maybe and Outcome variants, checks via `types::PrimitiveKind::Unit`

### MIR Constant Optimization
- `ConstInt`/`ConstFloat` NOT emitted as instructions, stored as literals in value_regs_
- `ConstUnit` now maps to `zeroinitializer` with type `"{}"` (fixed 2026-03-07)

### Suite Prefix Naming Bug (2026-02-28) - FIXED
- See previous notes for details

### AST Codegen: impl Method Call ABI Mismatch (2026-03-02) - ACTIVE
- `method_impl.cpp:585`: `this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr"`
- ALL first args to impl methods are passed as `ptr` for non-primitive types
- But functions like `into_inner(slot: ManuallyDrop[T])` define `slot` as struct by VALUE
- Definition side (`impl.cpp:215-226`) correctly detects non-this params via name check
- Call side (`method_impl.cpp:584-592`) does NOT check -- always passes ptr
- Affects: core/mem tests (ManuallyDrop, MaybeUninit, etc.)

### AST Codegen: Primitive ref this Type Bug (2026-03-02) - ACTIVE
- `impl.cpp:237-246`: For `this: ref This` on primitive types, checks `is_mut_this`
- `is_mut_this` only checks IdentPattern `is_mut` flag, NOT if TYPE is RefType
- `this: ref I32` gets `this_type = "i32"` but body generates `load i32, ptr %this`
- Affects: compiler/behaviors/toowned_assoc test

### MIR Codegen: Union Types Not Supported (2026-03-02) - ACTIVE
- HIR builder (`hir_builder.cpp:160-190`) has NO branch for `parser::UnionDecl`
- THIR lowering also ignores UnionDecl
- MIR has no `MirUnionType` -- only `MirStructType`
- Result: union type declarations never emitted in LLVM IR
- `instructions_misc.cpp:225` hardcodes `%struct.` prefix for all StructInitInst
- Affects: compiler/misc/union_basic test

### AST Codegen: Lazy Library Generic Static Functions (2026-03-02) - ACTIVE
- `runtime_modules.cpp:1356`: UNRESOLVED reference warning for lazy library defs
- `TypeId::of[T]()` is a generic STATIC method needing per-type instantiation
- Lazy library system doesn't generate instantiations for generic statics
- Affects: core/any tests

### Codegen Path Decision (query_core.cpp:520-610)
- `has_tml_imports_needing_codegen`: ANY module in registry with pure_tml_functions
- `has_local_generics`: local generic structs/enums/impls, or generic enum usage
- MIR path: NEITHER condition true
- AST path: EITHER condition true
- Test files importing std lib modules (core::mem, etc.) -> AST codegen
- Test files with no imports and no generics -> MIR codegen

### MIR Codegen: Missing GEP Zero Index for Array Access (2026-03-02) - ACTIVE
- `instructions.cpp:198-203`: GEP for array indexing emits ONE index instead of TWO
- Correct: `getelementptr inbounds [N x T], ptr %base, i32 0, i32 <index>`
- Actual:  `getelementptr inbounds [N x T], ptr %base, i32 <index>`
- Missing leading `i32 0` causes GEP to step by sizeof(entire array) instead of sizeof(element)
- `arr[1]` on `[5 x i32]` advances 20 bytes instead of 4 bytes -> out-of-bounds read
- `arr[0]` happens to work (0 * anything = 0)
- Root cause: `thir_mir_builder.cpp:748` sets `gep.indices = {idx}` (only user index, no zero prefix)
- Codegen at `instructions.cpp:200-202` emits indices verbatim without checking if base is array
- AST codegen (`llvm_utils.cpp`) correctly emits two indices for array GEP
- Affects: fixed_array.test.tml, types_encoding.test.tml, likely ALL [T; N] tests

### MIR Codegen: Array Mutation via Spill Not Propagated (2026-03-02) - ACTIVE
- `var bytes: [U8; 4] = [0,0,0,0]; bytes[0] = x; return (bytes, 1)`
- MIR spills array to alloca for GEP store, but tuple construction uses ORIGINAL SSA value
- Store goes to `%arr_spill`, but tuple packs original `%v5` (all zeros)
- Mutations to `var` arrays via index assignment are silently lost
- Affects: encode_utf8/encode_utf16 in types_encoding.test.tml

### AST Codegen: current_ret_type_ Override in Let Statement (2026-03-15) - FIXED
- `llvm_ir_gen_stmt.cpp:900-906`: `let x: StructType = expr` temporarily sets `current_ret_type_ = var_type`
- If `expr` contains `return Err(e)` inside a `when` arm, the return uses the WRONG type
- `Err(e)` constructor falls back to `types::make_i32()` for uninferable `T` (call.cpp:1184-1186)
- Produces `Outcome__I32__LayoutError` instead of correct `Outcome__tuple_Layout_I64__LayoutError`
- Fix: Added `func_ret_type_` field (never overridden by let hints) to `llvm_ir_gen.hpp:389`
- `gen_return` restores `current_ret_type_` to `func_ret_type_` before evaluating return expression
- Set `func_ret_type_` in all 10 function-entry points: func.cpp(2), impl.cpp(2), closure.cpp, class_codegen_generic.cpp(3), dyn.cpp, generate.cpp
- Affected: core/alloc suite (6 files), any code with `return Err(e)` inside `when` in a `let` init

### Coverage Linker: /EXPORT on Executable (2026-03-15) - FIXED
- `object_compiler.cpp:854`: `/EXPORT:__llvm_profile_write_file` was added for ALL output types
- For executables (test system), this forced linker to resolve symbol even when nothing references it
- LLVM backend compiles IR without `-fprofile-instr-generate`, so objects don't reference profile symbols
- Fix: moved `/EXPORT` inside `OutputType::DynamicLib` check, only when profile runtime is found

### Coverage Mode: once_lock_get_or_init Crash (2026-03-15) - ACTIVE (coverage artifact)
- `lib/std/tests/sync/once_lock_get_or_init.test.tml` passes without coverage, crashes with
- Assertion: `get_or_init should return 42` — closure return value corrupted by coverage instrumentation
- TML coverage adds `call void @tml_cover_func(ptr ...)` at function entry
- Likely ABI fragility in closure/reference codegen exposed by changed stack layout

### Incremental Cache Gotcha
- `incr.bin` cache keys by source hash + options, NOT by compiler binary hash
- After rebuilding the compiler, old cached IR is still served as "GREEN"
- Must rename/delete `build/debug/.incr-cache/incr.bin` after compiler changes
- `mcp__tml__cache_invalidate` only invalidates test suite cache, NOT incr.bin

### MCP Server Stale Binary Gotcha
- `tml_mcp.exe` links compiler libraries statically; can't rebuild while running
