# Codegen Debugger Memory

## Index
- [generic-method-fold-fix.md](generic-method-fold-fix.md) - Iterator::fold[B] method-level generic monomorphization: 3-bug chain (2026-03-20, FIXED)
- [fnptr-literal-coercion-fix.md](fnptr-literal-coercion-fix.md) - Function pointer call integer literal i32→i64 coercion (2026-03-20, FIXED)
- [ptr-read-write-struct-fix.md](ptr-read-write-struct-fix.md) - ptr_read/ptr_write multi-field struct: 4-bug chain (type checker + HIR + MIR + codegen) (2026-03-20, FIXED)
- [struct-field-mutation-fix.md](struct-field-mutation-fix.md) - Mutable struct field assignment dead code bug in THIR MIR builder (2026-03-20, FIXED)
- [dyn-boxing-casting-fix.md](dyn-boxing-casting-fix.md) - ref dyn Behavior fat pointer type resolution: 7 bugs in type/cast/enum/store/alloca (2026-03-19, FIXED)
- [when-pattern-binding-fix.md](when-pattern-binding-fix.md) - When-pattern enum payload bindings alias payload_ptr (fixes Maybe[mut ref T] dangling pointer, 2026-03-19, FIXED)
- [nullable-maybe-double-load.md](nullable-maybe-double-load.md) - Nullable Maybe[ref T] double-load crash in method dispatch (2026-03-19, FIXED)
- [dyn-behavior-codegen-fix.md](dyn-behavior-codegen-fix.md) - Full dyn Behavior codegen: fat pointers, vtables, vtable dispatch (2026-03-19, FIXED)
- [fnptr-sret-mismatch-fix.md](fnptr-sret-mismatch-fix.md) - Fn ptr indirect calls missing sret for struct returns (2026-03-19, FIXED)
- [fn-ptr-local-var-fix.md](fn-ptr-local-var-fix.md) - Function pointer in local variable indirect call fix (2026-03-17, FIXED)
- [fn-type-mangling-fix.md](fn-type-mangling-fix.md) - parse_tokens_with_pattern greedy token fix + Fn mangling (2026-03-16, FIXED)
- [where-clause-functype-fix.md](where-clause-functype-fix.md) - match_where_pattern_call FuncType handling (2026-03-16, FIXED)
- [runtime-lib-naming-fix.md](runtime-lib-naming-fix.md) - Dual .lib/.a naming for Zig CC (2026-03-16, FIXED)
- [where-clause-type-equalities.md](where-clause-type-equalities.md) - `where I::Item = ref T` constraint resolution (2026-03-15, partial fix)
- [suite-merging-investigation.md](suite-merging-investigation.md) - %struct.This/%struct.T and toowned_assoc bugs
- [this-parameter-conventions.md](this-parameter-conventions.md) - How `this`/`self` parameter types are determined in codegen
- [builtin-enum-monomorphization.md](builtin-enum-monomorphization.md) - Maybe[T]::default() fails: built-in enums
- [array-mut-this-dispatch.md](array-mut-this-dispatch.md) - Array `mut this` method dispatch failure
- [gen-path-unsigned-flag.md](gen-path-unsigned-flag.md) - gen_path() missing last_expr_is_unsigned_ (I32::MIN sext bug)
- [compiler-test-suite-issues.md](compiler-test-suite-issues.md) - Compiler test suite collision patterns

## Recent Fixes (2026-03-20)

### Function Pointer Call Integer Literal Coercion -- FIXED
- Bug: `f(42)` where `f: func(I64) -> I64` generated `call i64 %fn(i32 42)` — i32/i64 mismatch corrupted stack
- Root cause: AST codegen `call.cpp` function pointer call paths used `gen_expr` + `last_expr_type_` for arg types. Integer literals default to i32 but the declared param type is i64. No coercion was applied.
- Fix: 3 call sites in `call.cpp` now extract declared param types from FuncType/ClosureType and sext integer args when src_bits < dst_bits
- Files: `compiler/src/codegen/llvm/expr/call.cpp` (FieldExpr fat ptr ~264, IdentExpr fat ptr ~1635, IdentExpr thin ptr ~1809)
- MIR path already correct: `emit_indirect_call` uses `mir_func_type.params` for arg types, not expression-inferred types
- Note: Original repro also had `List[I64]::new()` without required `initial_capacity` arg — type checker gap (separate issue)

## Recent Fixes (2026-03-19)

### When-Pattern Enum Payload Dangling Pointer -- FIXED
- Bug: `Maybe[mut ref T]` methods (as_mut, get_mut etc.) returned ptr to local alloca → ACCESS_VIOLATION
- Root cause: `when.cpp` enum payload bindings for primitives did load+copy to local alloca. `mut ref val` returned ptr to copy, which is stack-local and freed on return.
- Fix: All 3 binding sites in when.cpp now alias payload_ptr directly (both struct AND primitive types). gen_ident loads through ptr for by-value access.
- File: `compiler/src/codegen/llvm/control/when.cpp` (lines ~759, ~844, ~948)
- Tests fixed: option_as_mut, shared_getmut, sync_getmut, array_get_mut

### Nullable Maybe[ref T] Double-Load Crash -- FIXED
- Bug: `arr.get(1).is_just()` crashed (ACCESS_VIOLATION) because codegen loaded through nullable ptr
- Root cause: `method.cpp:973-983` did `load ptr, ptr %receiver` for nullable-ptr Maybe, but receiver was already the loaded nullable ptr value. The extra load dereferenced the pointer as if it pointed to another pointer.
- Fix: Removed the extra load for `enum_type_name == "ptr"` case. The receiver from `gen_expr` is always the value, not an alloca pointer.
- Tests fixed: core/array 20/20, core/slice 21/21, core/cell 27/27, core/iter 52/52, core/types 7/7

### Nested Generic Enum Monomorphization -- FIXED
- Bug: `Poll[Outcome[I64, MyError]]` generated `%struct.Outcome__I32__I32` instead of `%struct.Outcome__I64__MyError`
- Root cause: In AST codegen `call.cpp`, when generating inner enum constructors (e.g., `Outcome::Ok(42)` inside `Poll::Ready(...)`), `expected_enum_type_` was not propagated from the outer constructor to the inner one. Unresolved generic params defaulted to I32.
- Fix: Before calling `gen_expr` for the inner arg, extract the inner type from the outer mangled name and set `expected_enum_type_`. For single-type-param enums like `Poll[T]`, parse `Poll__Outcome__I64__MyError` to extract `Outcome__I64__MyError` as the expected inner type.
- File: `compiler/src/codegen/llvm/expr/call.cpp:924` (pending_generic_enums_ path)
- Limitation: Only handles single-type-param outer enums. Multi-type-param nesting (e.g., Outcome[Maybe[I32], Str]) requires proper mangled name parsing.
- Note: Type checker changes (types_checker.cpp, expr_call.cpp) were attempted but caused regressions; reverted. The codegen-level fix is sufficient.

### Async/Await Stale Cache + MIR AwaitInst -- FIXED
- Root cause: `compiler_build_hash()` in `query_incr.cpp` used `__DATE__`/`__TIME__` which only changes when THAT file is recompiled. Incremental C++ builds don't recompile it when codegen files change.
- Fix: Changed to use binary mtime (`GetModuleHandleExA` on Windows, `/proc/self/exe` on Linux)
- Also: MIR codegen `instructions.cpp` silently ignored `AwaitInst` (no handler). Added handler that extracts Poll[T] payload field 1.
- Key insight: `tml run` uses query pipeline (MIR by default), but `tml build --emit-ir` forces AST codegen. A stale cache can make `run` fail while `build --emit-ir` works fine.
- Files: `query_incr.cpp:23`, `instructions.cpp:374`

## Recent Fixes (2026-03-16)

### Runtime Library Dual Naming -- FIXED
- `builder_helpers.cpp`: All runtime lib finders now try both `.lib` and `.a` (json, profiler, search + deps)
- `testing_compile.cpp`: Added import-scanning heuristic for test files in non-standard locations

### Where Clause FuncType Pattern Matching -- FIXED
- `call.cpp:match_where_pattern_call` lacked FuncType/ClosureType handling
- `from_fn[F,T](gen:F) where F=func()->Maybe[T]` resolved T=Unit instead of I32
- Fix: Added FuncType+ClosureType branches before NamedType check

### iter_from_fn Test -- FIXED
- Combined effect of above: `from_fn__Fn__Unit` (wrong) -> `from_fn__Fn__I32` (correct)

### Compiler Test Suite: 158/158 -- ALL PASS
- 3 zstd tests: fixed by import-scanning in testing_compile.cpp
- iter_from_fn: fixed by FuncType pattern matching

### Older Fixes (same session)
- gen_path() last_expr_is_unsigned_ bug
- 12 compiler tests missing `use test`
- `--no-suite` CLI flag

## Key Findings

### Code Generation Ordering
1. Library modules at generate.cpp:588
2. Library impls registered in `functions_` with lazy deferred bodies
3. Local structs/enums/consts at generate.cpp:738+
4. Pre-register local function signatures at generate.cpp:826-853
5. Local impl blocks at generate.cpp:855+
6. Local functions at generate.cpp:855+
7. Lazy deferred library bodies at runtime_modules.cpp:1061
8. Vtables at generate.cpp:1310+

### Active Bugs
- **future_fuse.test.tml**: Double-load on field.method() chain (GEP+load then load again)
- **array_ascii_is_ascii.test.tml**: Const-generic specialized impl (`impl[const N] Array[U8, N]`) not resolved
- **toowned_assoc.test.tml**: Local/library impl name collision for primitives
- **Built-in enum monomorphization**: Maybe[T]::default() etc.
- **Full suite crash**: Resource exhaustion running all 1477 tests concurrently

### MIR Codegen Method Call Pipeline
- THIR builder creates CallInst directly (not MethodCallInst) for NON-dyn method calls
- For dyn dispatch, THIR builder now creates MethodCallInst with is_dyn_dispatch=true
- Devirtualization pass can also convert MethodCallInst -> CallInst
- All MIR method dispatch fixes must go in `emit_call_inst` in instructions.cpp
- Default pipeline: CompilerOptions::use_thir = true → ThirMirBuilder, NOT HirMirBuilder

### Generic Inference Patterns
- FuncType vs ClosureType: closures return ClosureType, not FuncType (fixed in extract_type_params)
- where clause type equalities: `resolve_impl_where_clause` handles FuncType but `match_where_pattern_call` didn't (now fixed)
- Unit fallback at call.cpp:1958 when generic not inferred from args or where clause

### Name Mangling for Primitives
- `find_module_for_type("I32")` always returns `"core"` -> local+library impls share mangled name
- `functions_` uses flat key `"I32_to_owned"` which also collides
