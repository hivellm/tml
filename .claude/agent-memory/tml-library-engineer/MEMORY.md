# TML Library Engineer Memory

## TML Struct Syntax (CRITICAL)
- `pub struct Foo { ... }` does NOT exist — use `pub type Foo { ... }` for record/struct types
- `pub struct` causes P001 parse cascade on EVERYTHING after it in the file
- Three valid type declarations: `pub enum`, `pub type Name = ...` (alias), `pub type Name { fields }` (struct)

## Sub-module re-exports in mod.tml
- Relative `pub use dtype::DType` fails if module isn't in the global AST yet
- Use fully-qualified `pub use std::ia::tensor::dtype::DType` form to guarantee resolution
- `pub use module::{A, B, C}` brace-imports work fine when using full paths

## DI Module (2026-04-03) — Phases 1-4 Complete
- `lib/std/src/di/` — registry.tml, module.tml, application.tml, config.tml, mod.tml
- Tests: 4 test files, 4/4 suites passing (24 tests total)
- **GlobalASTCache stale IR bug**: When config.tml was first compiled with `str_parse_i64` returning `Maybe[I64]`, then source was fixed, the GlobalASTCache still served stale IR with wrong `i32` return for `@tml_str_parse_i64`. **Fix: rename struct** (`Config` → `InjectedConfig`) — forces fresh layout in GlobalASTCache.
- **`use core::str::basic::len as str_len` → `@tml_str_len` undefined**: Imported free functions get a mangled TML name, but linker expects C name. Fix: use local `@extern("strlen") func di_c_strlen(s: Str) -> I64` declaration.
- **Integer parsing with `Maybe[I64]` cross-module**: Avoid entirely. Implement `config_parse_i64` as a module-private free function returning `I64` directly (uses `lowlevel { ptr_read[U8] }` inline parser).
- **Module.import_module**: Uses `keys()/values()` on HashMap to enumerate entries for merge loop.
- `InjectedConfig` (not `Config`) — final type name to avoid cache collision.

## db Benchmark Module (2026-04-03) — COMPLETE
- `lib/std/src/db/bench/` — runner.tml, stats.tml, suite.tml, report.tml, reference.tml, mod.tml
- Tests: db_bench.test.tml — 13 tests, 21/21 std/db suite passing
- **CRITICAL BUG WORKAROUND**: GlobalASTCache caches struct type layouts permanently. If a struct named X is compiled once with wrong field layout, ALL subsequent compilations use the wrong layout even after source changes. Fix: rename the struct (BenchResult→BenchmarkResult avoids the stale cache).
- **CRITICAL BUG**: HIR path struct field GEPs ALL resolve to index 0 when method names match field names. Fix: use distinct field names (prefixed: br_name, st_mean_ns) that never match method names.
- **CRITICAL BUG**: `var` in struct constructor forces HIR path with all-GEP-index-0 bug. Fix: extract var logic into helper function that returns the result; the helper with only a struct literal goes through THIR insertvalue path correctly.
- **CACHE NOTE**: `no_cache=true` on mcp__tml__test bypasses test-result cache but NOT the incremental IR cache (incr.bin). Struct type definitions from GlobalASTCache persist across runs. Only renaming the struct forces a fresh layout.
- Report functions use `ref BenchmarkResult` parameter — works correctly.

## db ORM Module (2026-04-02) — COMPLETE
- `lib/std/src/db/orm/` — model.tml, field.tml, mapper.tml, row_reader.tml, sql_builder.tml, repository.tml, query_set.tml, relation.tml, mod.tml
- Tests: db_orm.test.tml — 12/12 suite passing (all std/db tests)
- **CRITICAL**: Any file importing `SqliteStatement` causes linker to require sqlite3.lib. Split modules: pure SQL builders (no sqlite dep) vs sqlite-executing code. Tests import pure modules only.
- Pattern: `sql_builder.tml` (pure Str functions) + `repository.tml` (uses SqliteConnection/Statement). Tests use sql_builder, not repository.
- `mapper.tml` is pure (only `quote_str`); `row_reader.tml` has the sqlite-dependent `read_i64/str/f64/bool` helpers.

## db Schema & Migration Module (2026-04-02) — COMPLETE
- `lib/std/src/db/schema/` — table.tml (ColumnDef, IndexDef, ForeignKeyDef), introspect.tml
- `lib/std/src/db/query/` — create_table.tml, alter_table.tml, drop_table.tml
- `lib/std/src/db/migration/` — migration.tml, history.tml, runner.tml, mod.tml
- Tests: db_schema.test.tml, db_ddl.test.tml, db_migration.test.tml — 12/12 suite passing
- **CRITICAL**: `Str::len()` returns `()` when called on a param that shadows a struct field of same name
- **CRITICAL**: `use core::str::basic::len as str_len` → `@tml_str_len` undefined at link time. Use local `@extern("strlen")` instead, or avoid string length entirely in pure schema/SQL code

## Type Alias Breaks Static Method Resolution (2026-04-02)
`use Foo::Bar as Alias` + `Alias::static_method()` → T069 "Pattern expects enum type" on `when` of result.
Fix: import by original name `use Foo::Bar` and call `Bar::static_method()`. Confirmed for `sqlite::Database`.

## db Foundation Module (2026-04-02) — Phase 1-3 Complete (item 3.4 pending)
- `lib/std/src/db/driver/` — Connection, PreparedStatement, Transaction behaviors + DbRow type
- `lib/std/src/db/sqlite/` — SqliteConnection (impl Connection), SqliteStatement (impl PreparedStatement), SqliteDriver
- `lib/std/src/db/mod.tml` — now exports driver + sqlite
- All 11 new files pass type check. Tasks: `.rulebook/tasks/phase8_db-foundation/tasks.md`

## std::console Module (2026-04-01) — COMPLETE
- `lib/std/src/console.tml` — log, warn, error, debug, trace, time/time_end, count/count_reset, group/group_end, assert, table
- `compiler/runtime/diagnostics/console.c` — global state for timers, counters, indent level
- 5 test files all passing in `lib/std/tests/console/`
- [Intrinsic name collision fix](codegen_intrinsic_name_collision.md) — `log` collided with `@llvm.log`

## HTTP/2 Module (Sprint 8+9, 2026-03-19) — COMPLETE
See [h2_module_notes.md](h2_module_notes.md)
- `lib/std/src/http/h2/` — frame.tml, hpack.tml, stream.tml, connection.tml, server.tml
- 13 test files all passing (7 Sprint 8 + 6 Sprint 9)
- **CRITICAL**: structs stored on heap via ptr_read/ptr_write MUST be pure scalar (no Buffer/pointer fields)
- **CRITICAL**: return types through Outcome MUST be scalar-only (no Buffer fields)
- **CRITICAL**: private methods in impl blocks cannot be resolved — use `pub` for all methods
- Incremental cache ignores dependency changes — add comment `// vN` to force test rehash

## Array Module Coverage (2026-03-08) — 21/39 = 53.8%

**Covered (21)**: len, is_empty, get, get_mut, first, first_mut, last, last_mut, map,
as_slice, each_ref, each_mut, eq, partial_cmp, cmp, duplicate, hash, to_string,
debug_string, TryFromSliceError::to_string, TryFromSliceError::debug_string

**Blocked by generic trait dispatch returning () (10)**:
- `try_map`, `zip` — "Unknown method" (extra generic params U/E not resolved)
- `default` — static method returns `Array[I32, N]` with N unresolved
- `from_fn`, `try_from_fn`, `repeat` — standalone generic functions: `Array[T, N]` unresolved
- `from_ref`, `from_mut` — standalone generic functions return ()
- `Borrow::borrow`, `AsRef::as_ref` — trait impl returns () instead of `ref Slice[T]`

**Blocked by other codegen bugs (8)**:
- `as_mut_slice` — `MutSlice` unsized type alloc failure
- `BorrowMut::borrow_mut`, `AsMut::as_mut` — MutSlice unsized + trait dispatch ()
- `TryFrom::try_from` — impl[T: Copy] returns ()
- `eq_slice` — ref Slice param: `ptr` vs `{ptr, i64}` type mismatch
- `index`, `index_mut` — coverage tool doesn't track built-in `arr[i]` codegen

**array/iter (0/19)**: ALL blocked by ArrayIter const generic struct layout:
  `[0 x %struct.T]` instead of `[N x i32]`. Also `Array::iter/iter_mut/into_iter`
  "Unknown method" (cross-file impl resolution).

**array/ascii (0/9)**: ALL blocked by type-specialized impl dispatch:
  `impl[const N: I64] Array[U8, N]` methods return `()` instead of actual type.

**Array re-index codegen bug**: `doubled[1]` after `arr.map()` loads whole `[3 x i32]`
instead of indexing. Only `[0]` works for map results. Workaround: use `let v: I32 = result[0]`.

## LazyCell Module Coverage (2026-03-08)

4 of 5 functions now tested and passing: `new`, `get`, `is_initialized`, `get_mut`
- `into_inner` BLOCKED: `Maybe[I32]` from `OnceCell::into_inner()` collides with `Maybe[func() -> I32]` — both `{i32, i32}` but different LLVM type names
Test files: `lazy_cell.test.tml`, `lazy_into_inner.test.tml` (placeholder), `lazy_coverage.test.tml` (updated)

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

## Parser Reserved Words (2026-03-07)

`base` is a reserved keyword in the TML parser. Using it as a variable name causes
"Expected pattern" parse errors. Use `start`, `origin`, `p0`, or other names instead.

## Intrinsics Module Coverage Blockers (2026-03-07)

Newly covered this session (5 new test files, 21 new tests):
- `checked_div`, `ptr_offset`, `copy`, `write_bytes`, `transmute`
- `atomic_load`, `atomic_store`, `atomic_add`, `atomic_sub`, `atomic_and`, `atomic_or`
- `field_count`, `variant_count`
- `simd_store`, `simd_insert`, `simd_splat`

Still blocked:
- `cast[T,U]` / `volatile_read[T]` / `volatile_write[T]` / `atomic_cmpxchg[T]` / `atomic_xor[T]` -- generic intrinsic monomorphization emits `%struct.T` instead of concrete type
- `field_name` -- returns empty string (codegen returns null/empty for field name data)
- `field_type_id` / `field_offset` -- return 0 for all fields (codegen stub, not implemented)
- `slice_swap` -- direct call via lowlevel cast `p as mut ref T` does not produce correct reference; covered transitively via `MutSlice.swap()`

## Any Module Coverage Blockers (2026-03-07)

All AnyValue methods (`from[T]`, `downcast[T]`, `downcast_mut[T]`, `into_inner[T]`, `drop`, `debug_string`)
are blocked by `Cannot allocate unsized type %struct.T` codegen bug. Additionally, adding ANY new
test file to `lib/core/tests/any/` triggers the suite merging codegen bug -- existing 7 files work
but 8+ causes `%struct.T` symbol collision in merged IR.

`TypeId::hash` is blocked by "Unknown method: write_u64" -- generic trait dispatch on `H: Hasher`
cannot resolve `write_u64` method on the generic parameter.

## Iterator Adapter Coverage Blockers (2026-03-08)

- **peekable** (7 funcs) -- BLOCKED: nested `Maybe[Maybe[T]]` type layout mismatch
- **cloned** (3 funcs) -- BLOCKED: `where I::Item = ref T` emits `%struct.T` not concrete type
- **copied** (3 funcs) -- BLOCKED: same as cloned
- **intersperse** (2 funcs) -- BLOCKED: "expected comma after load's type" IR parse error
- **flatten** (2 funcs) -- BLOCKED: `Maybe[U]` Nothing init emits `store struct 0` not zeroinitializer
- **flat_map** (2 funcs) -- PARTIAL: constructor + first next() work (1 test passing), but Maybe[U] field mutation bug prevents exhaustion/multi-element tests
