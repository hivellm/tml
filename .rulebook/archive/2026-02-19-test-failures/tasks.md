# Tasks: Test Failures — Compiler/Runtime Bugs Blocking Coverage

**Status**: Complete (100% — only 8.4 async iterators deferred)
**Priority**: High
**Impact**: Unblocks ~365+ library functions from test coverage

## Context

This task tracks all compiler and runtime bugs discovered during test coverage work. These bugs prevent tests from compiling or running correctly. As new failures are found, they should be added here.

**Last updated**: 2026-02-15 (coverage at 62%, 7,631 tests passing)
**Tests executed**: 7,631 tests across 570+ test files — **0 failures**

**See also**: [modules-without-tests.md](./modules-without-tests.md) - Complete list of implemented modules without tests

## Phase 1: Primitive Trait Method Resolution (Core Bug)

- [x] 1.1a Add missing `impl BitAnd/BitOr/BitXor/Shl/Shr` for all integer primitives in `lib/core/src/ops/bit.tml` (DONE 2026-02-12)
- [x] 1.1b Fix compiler codegen bug: behavior methods on primitives return `()` instead of declared type (DONE 2026-02-12)
- [x] 1.2 Fix `cmp::PartialEq::eq/ne` and `cmp::PartialOrd::lt/le/gt/ge` on all primitive types (DONE 2026-02-12)
- [x] 1.3 Fix `cmp::Ord::cmp` and `cmp::PartialOrd::partial_cmp` on all primitive types (DONE 2026-02-12)
- [x] 1.4 Fix `hash::Hash::hash` returning `()` on all primitive types (DONE 2026-02-12)
- [x] 1.5 Fix `clone::Duplicate::duplicate` not being tracked by coverage for some primitive types (DONE 2026-02-15 — clone module now at 100% coverage 14/14; duplicate_primitives.test.tml 14/14 passed)
- [x] 1.6 Fix `borrow::ToOwned::to_owned` returning `()` on all primitive types (DONE 2026-02-12)
- [x] 1.7 Fix `cmp::clamp` codegen bug (DONE 2026-02-12)
- [x] 1.8 Fix `Str.char_at()` returning `()` instead of `I32` (DONE 2026-02-12)

## Phase 2: Assign Operators on Primitives

- [x] 2.1-2.5 Add missing `impl AddAssign/SubAssign/MulAssign/DivAssign/RemAssign` for all primitives (DONE 2026-02-12)
- [x] 2.6 Fix `ops::bit` assign variants on primitives (DONE 2026-02-12)

## Phase 3: Formatting Trait Methods

- [x] 3.1-3.6 Fix `fmt::Binary/Octal/LowerHex/UpperHex/LowerExp/UpperExp` on all types (DONE 2026-02-13)
- [x] 3.7 Fix `Maybe`/`Outcome` `to_string` — inline codegen for primitive inner types (DONE 2026-02-13 — maybe_outcome_tostring.test.tml 4/4 passed)

## Phase 4: Checked/Saturating/Wrapping Arithmetic

- [x] 4.1 Fix `num::overflow::checked_add/sub/mul/div/rem/neg` (DONE 2026-02-13)
- [x] 4.2 Fix `num::overflow::checked_shl/shr` (DONE 2026-02-13)
- [x] 4.3 Fix `num::saturating` methods on primitives (DONE 2026-02-13)
- [x] 4.4 Fix `num::wrapping` methods on primitives (DONE 2026-02-13)

## Phase 5: Generic Function Monomorphization

- [x] 5.1 Fix `std::types` generic functions (DONE 2026-02-13)
- [x] 5.2 Fix `unicode::char` functions (DONE 2026-02-13)
- [x] 5.3a Fix `mem::forget[T]` (DONE 2026-02-13)
- [x] 5.3b Fix `mem::zeroed[T]`, `mem::transmute[S,D]` (DONE 2026-02-13 — test_zeroed 3/3, test_transmute 1/1 passed)
- [x] 5.4 Fix `HashMapIter::key()` / `HashMapIter::value()` — i64-to-K/V type cast in codegen (DONE 2026-02-13 — test_hashmap_iter.test.tml 2/2 passed)
- [x] 5.5 Fix `ArrayList::new[T]()` / `Queue::new[T]()` (DONE 2026-02-13)

## Phase 6: Runtime Crash Fixes

- [x] 6.1 Fix `str::parse_*` stack overflow (DONE 2026-02-13)
- [x] 6.2 Fix `str::pad_left`, `str::pad_right` (DONE 2026-02-13)
- [x] 6.3 Fix `os::env_set` crash (DONE 2026-02-13)
- [x] 6.4 Fix `os::set_priority` crash (DONE 2026-02-13)
- [x] 6.5 Fix `Str::slice_str` crash (DONE 2026-02-13)
- [x] 6.6 Fix `Json::get_or` (DONE 2026-02-13)
- [x] 6.7 Fix `Text::data_ptr` — SSO mode crash from pointer-to-null comparison (DONE 2026-02-14 — fixed in text.c)

## Phase 6b: Behavior Dispatch on Structs/Generics

- [x] 6b.1 Fix `DecodeUtf16Error::to_string` / `::debug_string` (DONE 2026-02-13)
- [x] 6b.2 Fix `MutexGuard::deref` / `::deref_mut` (DONE 2026-02-13 — test_mutex_deref 4/4 passed)
- [x] 6b.3 Fix `RwLockReadGuard::deref` / `RwLockWriteGuard::deref` / `::deref_mut` (DONE 2026-02-13 — test_rwlock_deref 4/4 passed)
- [x] 6b.4 Fix `compiler_fence` intrinsic (DONE 2026-02-13)
- [x] 6b.5 Fix `BorrowError::to_string` / `BorrowMutError::to_string` (DONE 2026-02-13)
- [x] 6b.6 Fix `SocketAddrV4`/`SocketAddr` trait impls (DONE 2026-02-13 — test_socket_eq 5/5 passed)

## Phase 6c: List[Str] LLVM Type Mismatch

- [x] 6c.1 Fix `str::split` / `str::lines` / `str::join` / `str::concat_all` / `Str::split` (DONE 2026-02-13)

## Phase 6d: Generic Wrapper Type Methods

- [x] 6d.1 Fix `Saturating[T]::value()` (DONE 2026-02-13)
- [x] 6d.2 Fix `Wrapping[T]::value()` (DONE 2026-02-13)
- [x] 6d.3 Fix `NonZero[T]::get()` (DONE 2026-02-13)
- [x] 6d.4 Fix `Saturating[T]::add/sub/mul()` — added methods + fixed stale meta cache bug (DONE 2026-02-14)
- [x] 6d.5 Fix `Wrapping[T]::add/sub/mul/neg()` — added methods + fixed stale meta cache bug (DONE 2026-02-14)

## Phase 6e: LLVM Type Mismatch on Maybe[ref T]

- [x] 6e.1 Fix `OnceCell::get()` — unit variant ident binding shadowed Nothing in when arms (DONE 2026-02-14 — fixed in when.cpp, test_oncecell_get_full 3/3 passed)
- [x] 6e.2 Fix `OnceLock::get()` — returns `Maybe[ref T]` correctly (DONE 2026-02-15 — OnceLock type fully implemented; sync_once_lock.test.tml 6/6 passed; manual verification of `get()` returning `Maybe[ref I32]` confirmed working)
- [x] 6e.3 Fix `OnceLock::get_or_init()` — closures captured variables by value instead of by reference; mutations inside closure (like `lock.value = Just(f())`) went to a copy; fixed capture-by-reference in closure.cpp; added ptr→{ptr,ptr} wrapping in call_user.cpp, method_impl.cpp, call_generic_struct.cpp (DONE 2026-02-15 — test_get_or_init.test.tml passed; full suite 7631/7631 passed)

## Phase 6f: Exception Class Inheritance

- [x] 6f.1 Fix all exception subclass allocation (DONE 2026-02-13 — exception_subclass.test.tml 10/10 passed, all exception test files passing: 35+ tests)

## Phase 6g: File I/O

- [x] 6g.1 Fix `File::open_write()`/`open_read()`/`open_append()` (DONE 2026-02-13)
- [x] 6g.2 `SocketAddr::from_v4()` — not broken, method is `SocketAddr.V4()` (DONE 2026-02-13)

## Phase 7: Generic Enum Codegen

- [x] 7.1 Fix generic enum method instantiation (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.2 Fix generic enum payload extraction (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.3 Fix `Poll[T]` methods `is_ready`/`is_pending` (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.4 Fix `Bound::Unbounded` resolution in generic contexts (DONE 2026-02-13 — confirmed working)
- [x] 7.5 Fix behavior constraint methods on generic enums (DONE 2026-02-13 — generic_enum_debug.test.tml 2/2 passed)

## Phase 8: Iterator Codegen

- [x] 8.1 Fix associated type substitution (`I::Item` -> concrete type) in iterator adapters (DONE 2026-02-14 — Take/Skip adapters work with custom iterators; test_iter_assoc_type.test.tml 4/4 passed)
- [x] 8.2 Fix default behavior method dispatch on concrete types (DONE 2026-02-13 — iter_basic.test.tml 2/2 passed)
- [x] 8.3 Fix higher-order generic types — `OnceWith`, `FromFn` (DONE 2026-02-14 — fixed recursive where clause type matching for nested generics like `F = func() -> Maybe[T]`; fixed `expected_enum_type_` leaking into closures; fixed generic function params to accept fat pointers `{ ptr, ptr }` for closures; fixed mutable capture semantics; test_iter_higher_order.test.tml 1/1, test_iter_from_fn.test.tml 1/1, test_from_fn_simple2.test.tml 1/1 all passed)
- [ ] 8.4 Fix async iterator support — requires Pin, Context, Waker, async runtime; deferred until async runtime is stable

## Phase 9: Closure and Function Pointer Issues

- [x] 9.0 Fix closures capturing fat pointers (DONE 2026-02-13 — closure_capture_fat_ptr.test.tml 5/5 passed)
- [x] 9.1 Fix capturing closures in struct fields (DONE 2026-02-14 — changed `func(...)` struct fields from thin `ptr` to fat `{ ptr, ptr }` to store both fn_ptr and env_ptr; struct construction promotes thin ptrs with null env; method dispatch checks env null/non-null and passes env as first arg for closures; test_fn_struct.test.tml 2/2 passed)
- [x] 9.2 Fix tuple type arguments in trait definitions (DONE 2026-02-14 — tested behavior returning `(I32, I32)` implemented on struct; test_tuple_trait.test.tml 1/1 passed)
- [x] 9.3 Fix returning closures from functions — changed `func(...)` type from thin `ptr` to fat `{ ptr, ptr }` throughout; closures with captures can now be stored in struct fields, returned from functions, and called through fat pointer dispatch (DONE 2026-02-15 — covered by 9.1 fat pointer changes and closure capture-by-reference fix)
- [x] 9.4 Fix function pointer field calling (DONE 2026-02-14 — confirmed working via test_fn_struct.test.tml; `cb.action(21)` dispatches correctly through struct field)
- [x] 9.5 Fix `this.field()` calls for func-typed fields in generic impl methods (DONE 2026-02-14 — `receiver_ptr` fallback for `this`; generic field type resolution via `current_type_subs_`)
- [x] 9.6 Fix `expected_enum_type_` leaking into closure bodies (DONE 2026-02-14 — save/restore in `gen_closure()`)
- [x] 9.7 Fix generic function params to accept fat pointer `{ ptr, ptr }` for closures (DONE 2026-02-14 — FuncType params in `gen_func_instantiation` now use `{ ptr, ptr }` matching struct field storage)
- [x] 9.8 Fix mutable capture semantics (DONE 2026-02-14 — closures now read/write directly to env struct via GEP pointer instead of copying to local alloca)

## Phase 10: Slice and Array Creation

- [x] 10.1 Fix array-to-slice conversion (DONE 2026-02-14 — implemented `as_slice()`/`as_mut_slice()` codegen in method_array.cpp; added SliceType inference in infer.cpp; added semantic type inference for unannotated let bindings in llvm_ir_gen_stmt.cpp; `Slice::from_array` not needed since `arr.as_slice()` provides same functionality)
- [x] 10.2 Fix `Slice[T].split_at()` — tuple return from generic method (DONE 2026-02-14 — root cause: tuple field access codegen failed because `infer_expr_type` returned null for LLVM tuple type strings like `{ i32, i32 }`, causing `.1` to access field 0 instead of 1; fixed in llvm_ir_gen_stmt.cpp and infer.cpp)
- [x] 10.3 Fix `Slice[T].binary_search()` (DONE 2026-02-14 — was blocked by tuple return codegen; now works after 10.2 fix)
- [x] 10.4 Fix `Slice[T].sort()` (DONE 2026-02-14 — private methods from generic impls not registered in module functions; fixed in env_module_support.cpp to include non-pub methods from generic impl blocks)

## Phase 11: Generic Option/Result Codegen

- [x] 11.1 Fix generic closures in `Maybe` methods (DONE 2026-02-14 — map/and_then/or_else/filter/unwrap_or_else/map_or all work with closures using both trailing expr and `return`; fixed: get_closure_value_expr ReturnExpr unwrapping, phi predecessor tracking, ref T param binding, closure return redirect mechanism, type checker save/restore current_return_type_)
- [x] 11.1b Fix generic closures in `Outcome` methods (DONE 2026-02-14 — map/map_or/and_then/or_else/unwrap_or_else/is_ok_and/is_err_and all work with closures; added closure return redirect + phi predecessor tracking to all 7 methods)
- [x] 11.2 Fix nested `Outcome` drop function generation (DONE 2026-02-14 — tested `Outcome[Outcome[I32, Str], Str]` construction and unwrapping; works correctly)
- [x] 11.3 Fix auto-drop glue for `Maybe[Arc[I32]]` — implemented auto-drop glue for enums with droppable payloads; compiler now generates drop functions that check variant discriminant and call payload's drop if applicable (DONE 2026-02-15)
- [x] 11.4 Fix Outcome variant constructors inside closures (DONE 2026-02-14 — `Ok(expr)`/`Err(expr)` inside inline-evaluated closures now checks `closure_return_type_` for the full generic enum type; fixed in call.cpp and core.cpp)

## Phase 12: Module and Linking Issues

- [x] 12.1 Fix external module method linking for `std::types::Object` (DONE 2026-02-13 — object.test.tml 7/7 passed including inheritance, virtual dispatch, reference_equals)
- [x] 12.2 Fix `unicode_data::UNICODE_VERSION` constant — tuple constants `(U8, U8, U8)` now supported (DONE 2026-02-15 — commit 7538307 added tuple constant support; char_unicode_version.test.tml 3/3 passed; destructuring `let (major, minor, patch) = UNICODE_VERSION` works correctly)
- [x] 12.3 Fix external inheritance for exception subclasses — fixed transitive cross-module class dependency resolution for `ArgumentNullException extends Exception extends Object`; base class LLVM struct types now fully resolved in `emit_external_class_type()` (DONE 2026-02-15)
- [x] 12.4 Fix const-generic impl method resolution — basic `[I32; 3].is_empty()` works (DONE 2026-02-14 — confirmed via test_const_generic.test.tml); more complex cases may still need work

## Phase 13: Test Runner Performance

- [x] 13.1 Fix test runner 100% CPU saturation — thread explosion from nested parallelism (DONE 2026-02-14 — capped concurrent suite compilations to 2 in suite_execution.cpp)

## Phase 14: Nested Generic Type Codegen

- [x] 14.1 Fix `%struct.T` generic type param not substituted in nested context — generic struct fields using type param `T` produced `%struct.T` instead of `%struct.I32`; fixed in `generate.cpp` to apply `current_type_subs_` when resolving struct field LLVM types (DONE 2026-02-14)
- [x] 14.2 Fix nested adapter type generation — `Take[Skip[FromFn[...]]]` now properly substitutes type parameters instead of producing recursive struct names (DONE 2026-02-15)
- [x] 14.3 Fix `FromFn[F]` as adapter input — generic type param `F` now properly substituted in nested context; `Take[FromFn[func() -> Maybe[I32]]]` allocates correctly (DONE 2026-02-15)
- [x] 14.4 Fix tuple-returning adapters with associated types — fixed 7 interconnected bugs across type checker and codegen for associated type resolution: (1) resolve.cpp: stopped current_associated_types_ shadowing for type params, (2) expr_call_method.cpp: recursive substitute_type for behavior method return types, (3) core.cpp: impl-level where constraints visible in methods, (4) type.cpp: This::Item → I::Item substitution, (5) types_resolve.cpp: persistent type_associated_types_ registry, (6) generate.cpp: populate registry from concrete impls, (7) method_impl.cpp: I::Item in local generic impl type_subs (DONE 2026-02-15 — Enumerate and Zip both pass)
- [x] 14.5 Fix `Maybe[StructType]` in generic adapters — already fixed by 14.4's associated type resolution improvements; Fuse[Counter] and Chain[Counter,Counter] both work correctly with Maybe[I] fields (DONE 2026-02-15 — Fuse and Chain both pass)

## Phase 15: Generic Method Instantiation for Library-Internal Types

- [x] 15.1 Fix `StackNode__I32_free`/`StackNode__I32_new` undefined — non-public generic struct methods from library modules not instantiated (DONE 2026-02-14)
  - Root cause 1: `call_generic_struct.cpp` set `is_library_type=false` for types present in `pending_generic_structs_` (from library source parsing) but absent from `pending_generic_impls_`; fixed to only check `pending_generic_impls_` for local type detection
  - Root cause 2: `generic.cpp` module search only checked `mod.structs` (public), not `mod.internal_structs`; non-public structs like `StackNode` were never found; fixed to check both maps
- [x] 15.2 Re-enable 8 previously disabled test files — lockfree_queue, lockfree_stack_peek, mpsc_channel, sync_mpsc, mpsc_repro_mutex_ptr, mpsc_channel_creation, sync_collections.consolidated, thread (DONE 2026-02-14 — all passing)
- [x] 15.3 Fix `kdf.test.tml` — tests rewritten as kdf_gaps.test.tml covering Argon2Variant/Argon2Params/ScryptParams (DONE 2026-02-15 — kdf_gaps.test.tml 9/9 passed)
- [x] 15.4 Fix `key.test.tml` — tests rewritten as key_gaps.test.tml covering KeyType/KeyFormat/KeyEncoding/RsaKeyGenOptions/EcKeyGenOptions (DONE 2026-02-15 — key_gaps.test.tml 15/15 passed)
