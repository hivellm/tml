# Deep Analysis Reviewer Memory

## Key Debugging Insights

### Register Prefix Mapping (MIR vs AST Codegen)
- `%v` prefix -> MIR codegen (`compiler/src/codegen/mir/codegen_helpers.cpp:44`)
- `%t` prefix -> AST/LLVMIRGen codegen (`compiler/src/codegen/llvm/core/llvm_utils.cpp:39`)
- `%ext` / `%trunc` -> coercion registers in MIR binary instruction handler
- `%spill` -> struct-to-ptr spill registers

### Compilation Pipelines
- `emit-ir` tool uses LLVMIRGen (AST codegen, `%t` prefix)
- `build` command uses MIR codegen (THIR->MIR->LLVM, `%v` prefix) when `use_thir=true` (default)
- Test runner uses the `build` pipeline (MIR codegen) via `codegen_unit()`
- To reproduce MIR codegen bugs, use `mcp__tml__build` or `mcp__tml__run`, NOT `mcp__tml__emit-ir`
- **CRITICAL**: `mcp__tml__emit-ir` uses AST codegen. Test runner uses MIR codegen. They produce different results.

### MIR O0 Optimization Pass Bug (2026-03-01) - ACTIVE
- See `mir-optimization-bugs.md` for full analysis
- O0 pipeline at `query_core.cpp:447` applies SimplifyCfgPass, BlockMergePass, MergeReturnsPass
- These passes INCORRECTLY eliminate conditional branches and phi nodes
- Result: functions like `classify()` always return one branch's value, ignoring conditions
- Causes abort() -> __fastfail -> STATUS_STACK_BUFFER_OVERRUN (0xC0000409 = -1073740791)
- Affected tests: implicit_returns.test.tml, default_field_values.test.tml, likely others

### MIR Codegen Linkage Bug in Suite Mode (2026-03-01) - ACTIVE
- `mir_codegen.cpp:784-789`: `main` excluded from internal linkage when `force_internal_linkage=true`
- AST codegen (`func.cpp:583-595`) correctly gives `main` internal linkage in suite mode
- MIR codegen makes `tml_main` external -> duplicate symbols when multiple test files have main()

### MIR Codegen assert_eq Bool Dispatch (2026-03-01) - ACTIVE
- `instructions.cpp:868-877`: assert_eq dispatch handles ptr, i32, i64 but NOT i1 (Bool)
- Bool comparisons call `assert_eq(i64, i64)` with i1 args -> stack corruption

### Coverage System Architecture (2026-02-28)
- See `coverage-gaps.md` for full analysis
- **Source extraction**: `extract_functions()` in `testing_coverage.cpp` uses regex
- **Runtime tracking**: `tml_cover_func()` in `lib/test/runtime/coverage.c`
- **Codegen instrumentation**: `emit_coverage()` in AST codegen only, NOT MIR

### THIR Coercion Bug Pattern (2026-02-28)
- **File**: `compiler/src/thir/thir_lower.cpp`, lines 828-840
- **Bug**: `needs_coercion()` returns IntWidening without checking bit widths

### MIR Constant Optimization
- `ConstInt`/`ConstFloat` NOT emitted as instructions, stored as literals in value_regs_
- `ConstUnit` produces no output at all

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

### MCP Server Stale Binary Gotcha
- `tml_mcp.exe` links compiler libraries statically; can't rebuild while running
