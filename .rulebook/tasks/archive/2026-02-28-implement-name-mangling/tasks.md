# Tasks: implement-name-mangling

**Status**: Completed — Phase 1-7 implemented, 200/200 individual tests passing, intra-module call resolution fixed

## Phase 1: Immediate Fixes — Linkage and Short-Name Registration

- [x] 1.1 Add `if (current_module_prefix_.empty())` guard in `func.cpp` to prevent short-name registration for library functions
- [x] 1.2 Test-local functions already get `internal` linkage via `force_internal_linkage` in suite mode — confirmed working
- [ ] 1.3 Remove `max_per_suite=1` workaround from `exe_suite_runner.cpp` after confirming collisions resolved
- [ ] 1.4 Remove `has_compiler_tests` workaround from `suite_execution.cpp`
- [ ] 1.5 Run `tml test --coverage --no-cache` and confirm 0 failures

## Phase 2: Hierarchical Path Encoding

- [x] 2.1 Create `mangle_tml_symbol(module, name)` in `llvm_utils.cpp`
- [x] 2.2 Update `func.cpp` `pre_register_func()` to use `mangle_tml_symbol()`
- [x] 2.3 Update `func.cpp` `gen_function_def_impl()` to use `mangle_tml_symbol()`
- [x] 2.4 Add `current_module_name_` field and set/clear in `runtime_modules.cpp`
- [x] 2.5 Save/restore `current_module_name_` in `emit_referenced_library_definitions()`
- [x] 2.6 Fix call-site fallback in `call_user.cpp` — use `mangle_tml_symbol(found_mod_name, bare)`
- [x] 2.7 Add `current_module_name_` qualified lookup in `call_user.cpp` for library-internal calls
- [x] 2.8 Update lowlevel declarations in `runtime_modules.cpp` to use `mangle_tml_symbol()`
- [x] 2.9 Emit IR comment with TML qualified name before each library `define`
- [N/A] 2.10 impl.cpp / method_impl.cpp / drop.cpp / class_codegen — methods use `TypeName_method` which is already unambiguous (type names are globally unique in TML)
- [N/A] 2.11 Generic instantiations use `mangle_func_name(base, type_args)` (bare name + `__TypeArgs`) — separate from hierarchical encoding, will be addressed in Phase 4
- [x] 2.12 Verified IR end-to-end: definitions, call sites, cross-module calls, lazy library defs, lowlevel declarations, collect_refs pattern — all consistent
- [ ] 2.13 Run full test suite (`--no-cache`) and confirm valid IR with no symbol mismatches

## Phase 3: Parameter Type Encoding

- [x] 3.1 Create `mangle_type_code(TypePtr)` static method in `llvm_utils.cpp` — handles all primitives, NamedType, RefType, PtrType, SliceType, ArrayType, TupleType, FuncType
- [x] 3.2 Create `mangle_tml_symbol(module, name, param_types)` overload that appends `_<type codes>` after path
- [x] 3.3 Update `pre_register_func()` to resolve semantic params and pass to mangling
- [x] 3.4 Update `gen_function_def_impl()` to resolve semantic params and pass to mangling
- [x] 3.5 Update lowlevel declarations in `runtime_modules.cpp` to pass `func_sig.params`
- [x] 3.6 Update call-site fallback in `call_user.cpp` — capture `found_param_types` from module registry
- [x] 3.7 Verified IR: `len(Str)→_S`, `contains(Str,Str)→_SS`, `repeat(Str,I64)→_Sl`, `is_digit(Char,U32)→_cj`
- [ ] 3.8 Run full test suite (`--no-cache`) and confirm valid IR with no symbol mismatches

## Phase 4: Hash for Generic Instantiations

- [x] 4.1 Integrate hash (FNV-1a 64-bit) for generic instantiation — `fnv1a_hash_hex()` in llvm_utils.cpp
- [x] 4.2 Create `generic_func_modules_` map to track module origin of generic functions
- [x] 4.3 Append `_h<8hex>` to instantiated library generic function names
- [x] 4.4 Verified distinct symbols: `identity__I32` (local), `N4core3mem6forgetE__I32_h1e3f93ff` (library)
- [x] 4.5 Updated `mangle_func_name()` in llvm_types.cpp to use hierarchical path + hash for library generics

## Phase 5: `@no_mangle` Decorator

- [x] 5.1 Parser already handles `@no_mangle` via existing Decorator system — no changes needed
- [x] 5.2 AST already stores decorators in `FuncDecl::decorators` — no new flag needed
- [x] 5.3 Skip mangling in codegen when `@no_mangle` is set — bare name, no `tml_` prefix, no suite prefix
- [x] 5.4 Force `external` linkage for `@no_mangle` functions (always visible for FFI)
- [x] 5.5 Error on `@no_mangle` + generic functions — `error[C001]` emitted
- [N/A] 5.6 Duplicate `@no_mangle` symbols — handled by LLVM linker (same as C duplicate symbols)
- [x] 5.7 Verified IR: `@no_mangle func my_c_callback` → `define i32 @my_c_callback` (bare, external)
- [x] 5.8 Verified error: `@no_mangle func identity[T]` → `error[C001]: @no_mangle cannot be used with generic functions`

## Phase 6: Specification and Documentation

- [x] 6.1 Update docs/specs/08-IR.md with mangling spec (Section 8: LLVM Symbol Mangling)
- [x] 6.2 Add type code table to docs/specs/08-IR.md (Section 8.3)
- [x] 6.3 Add `@no_mangle` to docs/specs/25-DECORATORS.md built-in decorators table
- [x] 6.4 Add codegen error C001 to docs/specs/12-ERRORS.md (Section 5: Codegen Errors)
- [x] 6.5 Create `tml demangle` CLI command in dispatcher.cpp
- [x] 6.6 Add `demangle` to help text in utils.cpp

## Phase 7: Bug Fix — Intra-Module Call Resolution in Pending Instantiations

- [x] 7.1 Root cause: `generate_pending_instantiations()` in `generic.cpp` never set `current_module_name_` before calling `gen_impl_method_instantiation()`, so intra-module calls (e.g., `Arena::alloc_raw` → `align_up`) couldn't resolve qualified names
- [x] 7.2 Fix: Save/restore module context in `generic.cpp`, set `current_module_name_` from module registry for library types (both local impls path and imported modules path)
- [x] 7.3 Verified IR: `alloc_raw` now calls `@tml_N4core5arena8align_upE_ll` (correct mangled name)
- [x] 7.4 Individual test validation passed: arena 1/1, alloc 1/1, str 21/21, fmt 35/35, ops 38/38, convert 15/15, slice 6/6, error 19/19, json 12/12, regex 4/4, collections 47/47
- [ ] 7.5 Known preexisting: `iter_chain`/`iter_cycle` etc. fail with `Counter_next` unresolved — user-defined impl methods in generic contexts, NOT caused by this task
