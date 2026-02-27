# Tasks: implement-name-mangling

**Status**: In Progress (3%)

## Phase 1: Immediate Fixes — Linkage and Short-Name Registration

- [x] 1.1 Add `if (current_module_prefix_.empty())` guard in `func.cpp` line 477 to prevent short-name registration for library functions (fixes core::str vs core::char::methods collision)
- [ ] 1.2 Force `internal` LLVM linkage for all test-local functions (non-library) so they never enter the global symbol namespace
- [ ] 1.3 Remove `max_per_suite=1` workaround from `exe_suite_runner.cpp` after Phase 1 confirms collisions are resolved
- [ ] 1.4 Remove `has_compiler_tests` workaround from `suite_execution.cpp`
- [ ] 1.5 Run `tml test --coverage --no-cache` and confirm 0 failures

## Phase 2: Hierarchical Path Encoding

- [ ] 2.1 Create `mangle_module_path(path: string) -> string` that converts `core::str` → `N4core3strE` using length-prefixed encoding
- [ ] 2.2 Create `mangle_func_name(module, name) -> string` that produces `@tml_N<path>E_<func>` with no ambiguous underscores
- [ ] 2.3 Update `func.cpp` `gen_function_def_impl()` to use the new mangler
- [ ] 2.4 Update `func.cpp` `gen_extern_decl()` to use the new mangler
- [ ] 2.5 Update `class_codegen.cpp`, `class_codegen_generic.cpp`, `class_codegen_virtual.cpp` — all `@tml_` + name constructions
- [ ] 2.6 Update `expr/call.cpp` — function lookup by mangled name
- [ ] 2.7 Update `expr/method_impl.cpp` — method call lookup
- [ ] 2.8 Update `core/drop.cpp` — destructor generation
- [ ] 2.9 Add a substitution table (Itanium-style) to deduplicate repeated long path segments in IR
- [ ] 2.10 In `func.cpp` `gen_function_def_impl()`: when `--emit-ir` is active, emit a LLVM IR comment immediately before each `define` with the original TML qualified name and signature (e.g., `; core::str::to_lowercase(Str) -> Str`)
- [ ] 2.11 Run full test suite and confirm valid IR

## Phase 3: Parameter Type Encoding

- [ ] 3.1 Create `MangledTypeCode` enum with codes for primitives: `v`=void, `c`=Char, `i`=I32, `l`=I64, `s`=Str, `b`=Bool, `f`=F32, `d`=F64, etc.
- [ ] 3.2 Create `mangle_param_types(params: Vec<Type>) -> string`
- [ ] 3.3 Create `mangle_type(ty: Type) -> string` for compound types: `Maybe[I32]` → `M_i`, `List[T]` → `L_T`, `Ptr[T]` → `P_T`
- [ ] 3.4 Integrate `mangle_param_types` into function name generation
- [ ] 3.5 Update call site resolution to include types in lookup
- [ ] 3.6 Write tests: two functions with the same name but different parameters produce distinct symbols

## Phase 4: Hash for Generic Instantiations

- [ ] 4.1 Integrate Blake2b-64 (or reuse existing CRC64) for generic instantiation hashing
- [ ] 4.2 Create `mangle_generic_hash(type_substitutions: Map<String, Type>) -> string`
- [ ] 4.3 Append `_h<16hex>` to names of instantiated generic functions
- [ ] 4.4 Verify `List[I32]::contains` and `List[Str]::contains` produce distinct symbols
- [ ] 4.5 Run generics test suite and confirm no collisions

## Phase 5: `@no_mangle` Decorator

- [ ] 5.1 Add `@no_mangle` decorator to the TML parser — recognized on `func` declarations
- [ ] 5.2 Store `no_mangle: bool` flag in the AST `FuncDecl` node
- [ ] 5.3 In `func.cpp` `gen_function_def_impl()`: when `func.no_mangle == true`, use `func.name` as-is for the LLVM symbol (no `@tml_` prefix, no path prefix, no type suffix)
- [ ] 5.4 Force `external` linkage for `@no_mangle` functions (they must be globally visible by exact name)
- [ ] 5.5 Emit a compiler error if `@no_mangle` is applied to a generic function (mangling is required for instantiation disambiguation)
- [ ] 5.6 Emit a compiler warning if two `@no_mangle` functions in the same module share the same name
- [ ] 5.7 Document `@no_mangle` in `docs/05-SEMANTICS.md` and `docs/09-CLI.md` (FFI section)
- [ ] 5.8 Write tests: `@no_mangle func add(a: I32, b: I32) -> I32` produces symbol `@add` in LLVM IR (not `@tml_N...E`)
- [ ] 5.9 Write test: `@no_mangle` + `@extern("c")` combo — verify no double-decoration conflict

## Phase 6: Specification and Documentation

- [ ] 6.1 Document the final scheme in `docs/08-IR.md` under "Symbol Mangling"
- [ ] 6.2 Add type code table to `docs/08-IR.md`
- [ ] 6.3 Update `docs/12-ERRORS.md` with mangling-related errors (e.g., duplicate symbol, `@no_mangle` on generic)
- [ ] 6.4 Add a section to `CLAUDE.md` explaining how to read TML symbols in IR output for debugging
- [ ] 6.5 Implement `tml demangle <symbol>` CLI command that converts `@tml_N4core3str12to_lowercaseE_s` back to `core::str::to_lowercase(Str)`
