# Codegen Debugger Memory

## Index
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

## Recent Fixes (2026-03-19)

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
