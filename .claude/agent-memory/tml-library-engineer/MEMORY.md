# TML Library Engineer Memory

## Option Module Coverage Blockers (2026-03-07)

4 functions blocked by compiler codegen bugs. Real tests written, will pass when bugs are fixed:
- `iter` / `MaybeIter::next` -- struct ptr vs value: `'%this' ptr but expected '%struct.Maybe__I32'`
- `zip_with` -- tuple `%struct.I32_I32` load bug (note: `zip` works if you only check `is_just()`/`is_nothing()`)
- `transpose` -- nested generic layout: `Outcome__Maybe__Outcome__I32__Str__E` (3xi64) vs `Outcome__Maybe__I32__Str` (2xi64)
- `map_or_else[U]` with U!=T -- `Maybe__I32` vs `Maybe__U` mismatch (same-type works fine)

## Result Module Coverage Blockers (2026-03-07)

2 functions blocked by nested generic type layout codegen bugs. Real tests written, will pass when fixed:
- `flatten` -- `Outcome__I32__Str` ({i32,i64}) vs `Outcome__Outcome__I32__Str__Str` ({i32,[2xi64]}) return type mismatch
- `transpose` -- `Maybe__Outcome__Maybe__I32__Str` ({i32,[3xi64]}) vs `Maybe__Outcome__I32__Str` ({i32,[2xi64]}) store type mismatch

Test files: `lib/core/tests/result/outcome_flatten.test.tml`, `outcome_transpose.test.tml`

## Pool Module Coverage Blockers (2026-03-07)

Pool and ThreadLocalPool acquire/release are blocked by codegen bugs:
- `Pool::acquire` -- ACCESS_VIOLATION (-1073741819). lowlevel { (*node_ptr).next } with Ptr[PoolNode] from I64 cast + atomic CAS produces invalid code
- `Pool::release` -- requires acquire to test, same root cause
- `ThreadLocalPool::acquire` -- "invalid getelementptr indices" on opaque ptr for List[Ptr[T]].pop()
- `ThreadLocalPool::release` -- requires acquire to test

Working functions tested: Pool::with_doubling, Pool::clear, ThreadLocalPool::new, ::default, ::grow_by, ::stats, ::available, ::in_use, ::clear
Test files: `pool_acquire_release.test.tml`, `threadlocal_pool_basic.test.tml`, `threadlocal_pool_ops.test.tml`

## Alloc Module Coverage (2026-03-07)

- Shared::from_raw -- tested via manual memory layout construction (SharedInner is private)
- Sync::from_raw -- tested same way (SyncInner is private)
- Allocator behavior + AllocatorRef -- tested via SimpleAllocator impl in allocator_ref.test.tml
- handle_alloc_error -- tested in alloc_handle_error.test.tml
- Heap/Shared/Sync Display::fmt and Debug::fmt_debug -- BLOCKED by codegen (undefined symbol for trait-bounded generic impls)

## Stream Module Coverage -- FIXED (2026-03-07)

All 9 stream modules now at 100% coverage. The double `mut ref` forwarding bug was fixed
by removing redundant `mut ref`/`ref` when forwarding already-ref parameters in delegation methods.

**Pattern**: When `func foo(x: mut ref T)` delegates to `bar(x: mut ref T)`, pass `x` directly
(not `mut ref x`), because `x` is already `mut ref T`. Writing `mut ref x` creates double
indirection (`mut ref mut ref T`).

**Dead code removed**: `rbuf_clear` (readable_stream) and `wbuf_clear` (writable_stream)
were unused private functions. Removing them eliminated the uncoverable gap.

## Net Module Coverage Status (2026-03-07)

### net/ip (was 54/61, now 60/61)
- **FIXED**: Ipv4Addr to_string/debug_string, Ipv6Addr to_string/debug_string, IpAddr to_string/debug_string
  All 6 now work -- Display impls use simple `to_string(this) -> Str` pattern, no Formatter needed.
  Test file: `ip_display_debug.test.tml`
- **BLOCKED**: `IpAddr::eq` -- `==` operator uses built-in enum comparison, never enters PartialEq impl body.
  Coverage tool tracks the impl function but codegen bypasses it entirely.

### net/parser (15/18, still blocked)
- `parse_socket_addr` -- SEGFAULT via SocketAddr::V6 path (even error-only calls crash)
- `SocketAddrV6::parse` -- static method cross-module: "expected SocketAddrV6, found ()"
- `SocketAddr::parse` -- static method cross-module: "Unknown method: is_ok" on return

### net/socket (30/33, still blocked)
- `SocketAddrV4::fmt` / `SocketAddrV6::fmt` / `SocketAddr::fmt` -- Formatter-based Display impl
  generates undefined symbol. These use `fmt(this, f: mut ref Formatter)` pattern unlike
  ip types which use simple `to_string(this) -> Str`.

### net/udp -- 100% (34/34)

### Workarounds
- **Maybe[Duration] Nothing**: Bare `Nothing` infers as `Maybe[I32]`. Use: `let none: Maybe[Duration] = Nothing`
- **SocketAddr eq**: alloca-GEP codegen bug. Test hash/cmp instead of ==.

## JSON Serialize Module Coverage Blockers (2026-03-07)

4 functions blocked by generic trait impl codegen bug. Return type resolves to `()` instead of actual type.
- `List::to_json` -- `impl[T: ToJson] ToJson for List[T]` returns `()` not `Json`
- `List::from_json` -- `impl[T: FromJson] FromJson for List[T]` returns `()` not `Outcome[List[T], Str]`
- `HashMap::to_json` -- `impl[V: ToJson] ToJson for HashMap[I64, V]` returns `()` not `Json`
- `HashMap::from_json` -- `impl[V: FromJson] FromJson for HashMap[I64, V]` returns `()` not `Outcome[HashMap[I64, V], Str]`

Error: "Type mismatch: expected Json, found ()"
Test files: `lib/std/tests/json/json_serialize_list.test.tml`, `json_serialize_hashmap.test.tml`
Root cause: Same "generic trait dispatch" codegen bug affecting ~140 functions project-wide.

## Sync Module Coverage (2026-03-07)

Previously blocked functions that NOW WORK (codegen bugs fixed):
- `Weak::upgrade` (Arc module) -- Maybe[Arc[T]] codegen fixed
- `OnceLock::get` -- Maybe[ref T] codegen fixed
- `OnceLock::get_or_init` -- closure+ref codegen fixed (single call only; calling twice crashes)
- `RwLockReadGuard::deref` / `RwLockWriteGuard::deref` / `RwLockWriteGuard::deref_mut` -- all work via explicit `.deref()`/`.deref_mut()` calls

Still blocked:
- `Condvar::wait` -- requires multi-threaded test (blocks forever in single-thread)
- `OnceLock::get_or_init` idempotent test -- calling get_or_init twice on same OnceLock crashes (access violation in fast-path Maybe[ref T] extraction)

## RefCell Module Coverage Blockers (2026-03-07)

3 functions blocked by codegen bugs:
- `try_borrow` / `try_borrow_mut` -- nullable-ptr optimization: `Maybe[Ref[T]]` is struct `{i32, i64}` but codegen emits `icmp eq ptr %t5, null`
- `RefMut::replace` -- runtime assertion failure (returned old value is incorrect, likely overwritten by store through cell ref)

Covered this session: `RefMut::get`, `RefCell::take`, `Ref::drop` (implicit), `RefMut::drop` (implicit)
Test files: `refcell_refmut.test.tml`, `refcell_take.test.tml`, `refcell_drop.test.tml`, `refcell_try_borrow.test.tml`

## Cache Module Coverage (2026-03-07)

- Removed dead code: `zeroed_array[T, N]()` -- private generic function, never called by any code path
- Added test for `SoaVec::set` (bounds-check path with I32, avoids lowlevel codegen bug with I64 store)
- `SoaVec::set` with I64: codegen bug `store i32 %t89` when value is i64 (lowlevel Ptr[T] always uses i32 store)
- SoaVec get/set successful paths still blocked by Str equality comparison bug in column name lookup

## Error Module Coverage Analysis (2026-03-07)

8 uncovered functions, 3 coverable, 5 blocked:
- **`with_position`** (ParseError) -- coverable, test written (`parse_error_with_position.test.tml`).
  NOTE: coverage scanner has brace-counting bug in doc comments (`'}'` in line 550 of error.tml
  throws off `impl_brace_depth`), so function name lacks `ParseError::` prefix.
- **`Error::source`** / **`Error::description`** -- default behavior methods. Coverage scanner
  tracks them as `Error::source`/`Error::description` but codegen monomorphizes per concrete type
  (e.g., `SimpleError::source`), so coverage names never match. Tests written
  (`error_default_methods.test.tml`) but coverage tool won't count them.
- **`NeverError::to_string`** / **`NeverError::debug_string`** -- call `unreachable()`, untestable.
- **`BoxedError::new`** -- generic `[E: Error]` blocked by "Unknown method: to_string" codegen bug.
- **`error_chain`** / **`ErrorChain::next`** -- `ref dyn Error` trait object vtable dispatch SEGFAULT.

Pre-existing suite-bundling failure: `error_parse_debug_precise.test.tml` -- `Just(7)` assertion
fails in suite mode but passes individually. Brace depth or Maybe codegen conflict in merged IR.

## Coverage Tool Behavior

- Coverage counts ALL `func` declarations in a source file (both `pub func` and private module-level `func`)
- Example: `readable_stream.tml` has 56 pub + 15 private = 71 total tracked functions
- Private functions are covered transitively when called by tested public functions
- Transitive coverage works: if `PassThroughStream::unpipe_all()` calls `DuplexStream::unpipe_all()`, both are counted as covered
- Dead private functions (never called by any code path) must be removed to reach 100%
- **Coverage scanner brace bug**: Doc comments containing `}` characters (e.g., `'}'` in examples)
  corrupt `impl_brace_depth` tracking, causing methods to lose their `Type::` prefix in coverage names.
- **Behavior default methods**: Coverage scanner tracks them under behavior name (e.g., `Error::source`),
  but codegen monomorphizes per concrete type (e.g., `SimpleError::source`). Names never match,
  so behavior defaults always show as uncovered.

## Fmt/Ops Module Coverage Blockers (2026-03-07)

### fmt/helpers (2 gap)
- `i64_digit_char` / `u64_digit_char` -- private dead code, never called by any pub func. Cannot test.

### fmt/traits (2 gap)
- `Write::write_char` / `Write::write_fmt` -- behavior default method dispatch passes Str as i32 instead of ptr.

### fmt/impls (4 gap)
- Tuple Display/Debug (2 through 6-tuple) -- type checker returns `()` instead of `Str` for `.to_string()` on tuples.
- Unit Display/Debug -- "Type mismatch: expected (), found ()" on Unit assignment.

### fmt/rt (7 gap)
- `Argument::new_display[T]`/`new_debug[T]` -- "Unknown method: to_string" in generic function body.
- `format_args`/`format_join` -- array literal `[N x ptr]` vs slice `{ptr, i64}` type mismatch.
- `debug_slice[T]`/`display_slice[T]` -- both generic + array ref bugs.
- `Count::resolve` -- zero-length array ref heap corruption.

### ops/drop (2 gap)
- `drop_in_place[T]` -- lowlevel block calls itself recursively (stack overflow), not handled as compiler intrinsic.

### ops/try_trait (5 gap)
- `Maybe::from_output` -- WORKS (3 tests passing in `try_trait_from.test.tml`).
- `Maybe::from_residual` -- void parameter for Unit type.
- `Outcome::from_output`/`from_residual` -- generic E not resolved (`Outcome__I32__E` vs `Outcome__I32__Str`).
- `FromResidual` impls -- same root causes as above.

## Test File Patterns for Streams

- Test files in `lib/std/tests/stream/`
- Return type `I32` (not `Outcome[Unit, Str]`) -- stream tests use the older test pattern
- Use `use test::{assert, assert_eq, assert_true, assert_false}`
- Listener callbacks are `func(data: I64)` typed as `I64` via `as I64` cast
