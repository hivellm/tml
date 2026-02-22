# Tasks: Standard Library Essentials

**Status**: In Progress (55%) - Core utilities needed for production use

**Note**: This task covers essential standard library modules that make TML usable for real-world applications. Many core modules are now implemented with working functionality.

**Already implemented**:
- `std::collections::HashMap` — `lib/std/src/collections/hashmap.tml`
- `std::collections::List` — `lib/std/src/collections/list.tml`
- `std::collections::HashSet` — `lib/std/src/collections/class_collections.tml` (create, add, remove, contains)
- `std::collections::BTreeMap` — `lib/std/src/collections/btreemap.tml` (I64 keys/values, sorted arrays + binary search)
- `std::collections::BTreeSet` — `lib/std/src/collections/btreeset.tml` (I64 only, backed by BTreeMap)
- `std::collections::Deque` — `lib/std/src/collections/deque.tml` (generic, ring buffer)
- `std::collections::Vec` — `lib/std/src/collections/class_collections.tml` (new, push, pop, get, set)
- `std::datetime` — `lib/std/src/datetime.tml` (now, from_timestamp, parse, format, accessors)
- `std::random` — `lib/std/src/random.tml` (Rng xoshiro256**, ThreadRng, shuffle, convenience fns)
- `std::time` — `lib/std/src/time.tml` (Instant, SystemTime, Duration, sleep)
- `std::os` — `lib/std/src/os.tml` (env_get/set/unset, args_count/get, process_exit, exec, homedir, tmpdir, current_dir)
- `std::file` — file/dir/path operations in `lib/std/src/file/`
- `core::time` — basic time module in `lib/core/src/time.tml`

**Still needed (this task)**:
- HashSet set operations (union, intersection, difference) — NOT yet implemented
- BTreeMap/BTreeSet generic versions — currently I64-only
- BufReader/BufWriter — NOT yet implemented
- Path/PathBuf standalone types — basic path exists in file module, needs enhancement
- Vec higher-level methods (extend, drain, sort, dedup) — NOT yet implemented

**Related Tasks**:
- Network I/O → `async-network-stack` (sync net already implemented)
- JSON → already implemented in `lib/std/src/json/`
- Sync → already implemented in `lib/std/src/sync/`

## Phase 1: Collection Aliases and Additions

> **Status**: Mostly Done — core types implemented, missing advanced methods and set operations

### 1.1 Vec[T] (in `class_collections.tml`)
- [x] 1.1.1 Create `Vec[T]` type in `lib/std/src/collections/class_collections.tml`
- [x] 1.1.2 Add `Vec::new()`, `Vec::with_capacity()` constructors
- [ ] 1.1.3 Implement `extend()`, `append()`, `drain()` methods
- [ ] 1.1.4 Implement `retain()`, `dedup()`, `sort()` methods
- [ ] 1.1.5 Add `Vec::from_iter()` for collection from iterators
- [x] 1.1.6 Add unit tests for Vec operations

### 1.2 HashSet[T] (in `class_collections.tml`)
- [x] 1.2.1 Design `HashSet[T]` in `lib/std/src/collections/class_collections.tml`
- [x] 1.2.2 Implement `create()`, `with_capacity()` constructors
- [x] 1.2.3 Implement `add()`, `remove()`, `contains()` methods
- [x] 1.2.4 Implement `count()`, `is_empty()`, `clear()` methods
- [ ] 1.2.5 Implement set operations: `union()`, `intersection()`, `difference()`, `symmetric_difference()`
- [ ] 1.2.6 Implement `is_subset()`, `is_superset()`, `is_disjoint()`
- [ ] 1.2.7 Implement `Iterator` for HashSet
- [x] 1.2.8 Add unit tests for HashSet

### 1.3 BTreeMap[K, V] (in `btreemap.tml`)
- [x] 1.3.1 Design `BTreeMap` in `lib/std/src/collections/btreemap.tml` (sorted arrays + binary search, I64-only)
- [x] 1.3.2 Implement sorted array structure with binary search
- [x] 1.3.3 Implement `insert()`, `get()`, `remove()` methods
- [ ] 1.3.4 Implement `range()` for range queries
- [ ] 1.3.5 Implement ordered iteration (Iterator behavior)
- [x] 1.3.6 Add unit tests for BTreeMap

### 1.4 BTreeSet[T] (in `btreeset.tml`)
- [x] 1.4.1 Design `BTreeSet` based on BTreeMap (I64-only)
- [ ] 1.4.2 Implement all set operations
- [ ] 1.4.3 Implement ordered iteration (Iterator behavior)
- [x] 1.4.4 Add unit tests for BTreeSet

### 1.5 Deque[T] (in `deque.tml`) — DONE
- [x] 1.5.1 Design `Deque[T]` (ring buffer) in `lib/std/src/collections/deque.tml`
- [x] 1.5.2 Implement `push_front()`, `push_back()`, `pop_front()`, `pop_back()`
- [x] 1.5.3 Implement `front()`, `back()`, `get()` accessors
- [x] 1.5.4 Implement growth strategy (doubling)
- [x] 1.5.5 Add unit tests for Deque

## Phase 2: Environment and Process

> **Status**: Partially Done — basic env/args/process in `std::os`, missing Command builder

**Note**: Env, args, and basic process functions implemented in `lib/std/src/os.tml` (not as separate modules). The `Command` builder pattern (subprocess with pipes) is tracked in `expand-core-std-modules` Phase 11.

### 2.1 Environment Variables (in `std::os`)
- [x] 2.1.1 Implement env functions in `lib/std/src/os.tml`
- [x] 2.1.2 Implement `os::env_get(name: Str) -> Maybe[Str]`
- [x] 2.1.3 Implement `os::env_set(name: Str, value: Str) -> Bool`
- [x] 2.1.4 Implement `os::env_unset(name: Str) -> Bool`
- [ ] 2.1.5 Implement `env::vars() -> Iterator[(Str, Str)]` (list all vars)
- [x] 2.1.6 Implement `os::current_dir() -> Str`
- [x] 2.1.7 Implement `os::set_current_dir(path: Str) -> Bool`
- [x] 2.1.8 Implement `os::homedir() -> Str`
- [x] 2.1.9 Implement `os::tmpdir() -> Str`

### 2.2 Command Line Arguments (in `std::os`)
- [x] 2.2.1 Implement args functions in `lib/std/src/os.tml`
- [x] 2.2.2 Implement `os::args_count() -> I32` and `os::args_get(index: I32) -> Str`
- [ ] 2.2.3 Implement `Args` iterator struct
- [ ] 2.2.4 Add simple argument parser utilities

### 2.3 Process Management (basic in `std::os`, Command builder pending)
- [x] 2.3.1 Implement basic process functions in `lib/std/src/os.tml`
- [ ] 2.3.2 Implement `Command` builder type (tracked in `expand-core-std-modules` Phase 11)
- [ ] 2.3.3-2.3.13 Command builder methods (spawn, output, status, Child, pipes)
- [x] 2.3.14 Platform implementations via `@extern("c")` FFI
- [x] 2.3.15 Basic process operations: `process_exit()`, `exec()`, `exec_status()`, `pid()`

## Phase 3: Buffered I/O

> **Status**: Pending

### 3.1 BufReader
- [ ] 3.1.1 Design `BufReader[R: Read]` in `lib/std/src/io/bufreader.tml`
- [ ] 3.1.2 Implement `new(inner: R) -> BufReader[R]`
- [ ] 3.1.3 Implement `with_capacity(cap: U64, inner: R) -> BufReader[R]`
- [ ] 3.1.4 Implement `Read` behavior for BufReader
- [ ] 3.1.5 Implement `BufRead` behavior with `fill_buf()`, `consume()`
- [ ] 3.1.6 Implement `read_line(this, buf: mut ref Text) -> Outcome[U64, IoError]`
- [ ] 3.1.7 Implement `lines(this) -> Lines` iterator
- [ ] 3.1.8 Implement `split(this, byte: U8) -> Split` iterator

### 3.2 BufWriter
- [ ] 3.2.1 Design `BufWriter[W: Write]` in `lib/std/src/io/bufwriter.tml`
- [ ] 3.2.2 Implement `new(inner: W) -> BufWriter[W]`
- [ ] 3.2.3 Implement `with_capacity(cap: U64, inner: W) -> BufWriter[W]`
- [ ] 3.2.4 Implement `Write` behavior for BufWriter
- [ ] 3.2.5 Implement automatic flushing on drop
- [ ] 3.2.6 Implement `into_inner(this) -> Outcome[W, IntoInnerError]`

### 3.3 LineWriter
- [ ] 3.3.1 Design `LineWriter[W: Write]` that flushes on newline
- [ ] 3.3.2 Implement `Write` behavior
- [ ] 3.3.3 Add unit tests for buffered I/O

## Phase 4: Path Utilities

> **Status**: Pending

### 4.1 Path Type
- [ ] 4.1.1 Design `Path` type in `lib/std/src/path/mod.tml`
- [ ] 4.1.2 Implement `Path::new(s: Str) -> Path`
- [ ] 4.1.3 Implement `as_str(this) -> Str`
- [ ] 4.1.4 Implement `parent(this) -> Maybe[Path]`
- [ ] 4.1.5 Implement `file_name(this) -> Maybe[Str]`
- [ ] 4.1.6 Implement `file_stem(this) -> Maybe[Str]`
- [ ] 4.1.7 Implement `extension(this) -> Maybe[Str]`
- [ ] 4.1.8 Implement `is_absolute(this) -> Bool`
- [ ] 4.1.9 Implement `is_relative(this) -> Bool`
- [ ] 4.1.10 Implement `join(this, other: Path) -> PathBuf`
- [ ] 4.1.11 Implement `with_extension(this, ext: Str) -> PathBuf`

### 4.2 PathBuf Type
- [ ] 4.2.1 Design `PathBuf` (owned path) in `lib/std/src/path/mod.tml`
- [ ] 4.2.2 Implement `PathBuf::new() -> PathBuf`
- [ ] 4.2.3 Implement `PathBuf::from(s: Str) -> PathBuf`
- [ ] 4.2.4 Implement `push(this, path: Path)`
- [ ] 4.2.5 Implement `pop(this) -> Bool`
- [ ] 4.2.6 Implement `set_file_name(this, name: Str)`
- [ ] 4.2.7 Implement `set_extension(this, ext: Str) -> Bool`
- [ ] 4.2.8 Implement `as_path(this) -> Path`

### 4.3 Platform-Specific
- [ ] 4.3.1 Handle path separators (`/` vs `\`)
- [ ] 4.3.2 Handle drive letters (Windows)
- [ ] 4.3.3 Handle UNC paths (Windows)
- [ ] 4.3.4 Add unit tests for path operations

## Phase 5: DateTime

> **Status**: Mostly Done — Instant, SystemTime, DateTime implemented in `lib/std/src/time.tml` and `lib/std/src/datetime.tml`

### 5.1 Duration (Enhancement)
- [ ] 5.1.1 Enhance existing `Duration` in `lib/std/src/time.tml`
- [ ] 5.1.2 Add `from_secs(secs: I64) -> Duration`
- [ ] 5.1.3 Add `from_millis(millis: I64) -> Duration`
- [ ] 5.1.4 Add `from_micros(micros: I64) -> Duration`
- [ ] 5.1.5 Add `from_nanos(nanos: I64) -> Duration`
- [ ] 5.1.6 Add `as_secs(this) -> I64`
- [ ] 5.1.7 Add `as_millis(this) -> I64`
- [ ] 5.1.8 Add `subsec_nanos(this) -> I32`
- [ ] 5.1.9 Add arithmetic operations (`+`, `-`, `*`, `/`)
- [ ] 5.1.10 Add `checked_add()`, `checked_sub()`, `saturating_*()` variants

### 5.2 Instant (in `lib/std/src/time.tml`) — DONE
- [x] 5.2.1 Design `Instant` for monotonic time
- [x] 5.2.2 Implement `Instant::now() -> Instant`
- [x] 5.2.3 Implement `elapsed(this) -> Duration`
- [x] 5.2.4 Implement `duration_since(this, earlier: Instant) -> Duration`
- [ ] 5.2.5 Implement `checked_add(this, dur: Duration) -> Maybe[Instant]`
- [ ] 5.2.6 Implement `checked_sub(this, dur: Duration) -> Maybe[Instant]`
- [x] 5.2.7 Platform implementations via `@extern("c")` FFI

### 5.3 SystemTime (in `lib/std/src/time.tml`) — DONE
- [x] 5.3.1 Design `SystemTime` for wall-clock time
- [x] 5.3.2 Implement `SystemTime::now() -> SystemTime`
- [x] 5.3.3 Implement `unix_epoch()` constructor
- [x] 5.3.4 Implement `duration_since_epoch(this) -> Duration`
- [x] 5.3.5 Implement `elapsed(this) -> Duration`

### 5.4 DateTime (in `lib/std/src/datetime.tml`) — DONE
- [x] 5.4.1 Design `DateTime` struct with year/month/day/hour/min/sec/timestamp
- [x] 5.4.2 Implement `DateTime::now() -> DateTime`
- [x] 5.4.3 Implement `DateTime::from_timestamp(ts: I64) -> DateTime`
- [x] 5.4.4 Implement `timestamp(this) -> I64`
- [x] 5.4.5 Implement component accessors (year, month, day, hour, minute, second, weekday, day_of_year)
- [x] 5.4.6 Implement formatting: `to_iso8601()`, `to_date_string()`, `to_time_string()`, `to_rfc2822()`
- [x] 5.4.7 Implement `parse_iso8601()`, `parse_date()`, `parse()` parsers
- [x] 5.4.8 Add unit tests for DateTime (datetime, datetime_format, datetime_parse)

## Phase 6: Random Number Generation

> **Status**: Mostly Done — Rng, ThreadRng, convenience functions implemented in `lib/std/src/random.tml`

### 6.1 Rng Type (in `lib/std/src/random.tml`)
- [x] 6.1.1 Design `Rng` type using xoshiro256** algorithm
- [x] 6.1.2 Implement `next_i64(this) -> I64`
- [x] 6.1.3 Implement `range(this, min, max) -> I64`, `next_f64()`, `next_bool()`
- [ ] 6.1.4 Implement `fill_bytes(this, buf: mut ref [U8])`

### 6.2 ThreadRng (in `lib/std/src/random.tml`) — DONE
- [x] 6.2.1 Design `ThreadRng` (thread-local RNG)
- [x] 6.2.2 Implement using xoshiro256** algorithm
- [x] 6.2.3 Implement `ThreadRng::new()` with `thread_random_i64()`, `thread_random_range()` convenience
- [x] 6.2.4 Implement `range()`, `next_i64()`, `next_bool()`, `next_f64()`, `reseed()`

### 6.3 Convenience Functions — Mostly Done
- [ ] 6.3.1 Implement generic `random[T: Random]() -> T`
- [x] 6.3.2 Implement `random_range(min: I64, max: I64) -> I64`
- [x] 6.3.3 Implement `random_bool() -> Bool`
- [x] 6.3.4 Implement `shuffle_i64()`, `shuffle_i32()` (type-specific, not generic)
- [ ] 6.3.5 Implement `choose[T](slice: ref [T]) -> Maybe[ref T]`

### 6.4 Distributions
- [ ] 6.4.1 Design `Distribution[T]` behavior
- [ ] 6.4.2 Implement `Uniform` distribution
- [ ] 6.4.3 Implement `Bernoulli` distribution
- [x] 6.4.4 Add unit tests for random generation (basic, shuffle, convenience, thread_rng)

## Phase 7: Integration Testing

> **Status**: Pending

### 7.1 Cross-Module Tests
- [ ] 7.1.1 Create integration test: file + buffered I/O
- [ ] 7.1.2 Create integration test: process + environment
- [ ] 7.1.3 Create integration test: datetime formatting
- [ ] 7.1.4 Create integration test: random + collections (shuffle)

### 7.2 Benchmarks
- [ ] 7.2.1 Benchmark collection operations vs Rust
- [ ] 7.2.2 Benchmark I/O throughput vs Rust
- [ ] 7.2.3 Benchmark string operations vs Rust
- [ ] 7.2.4 Document performance results

## File Structure

```
lib/std/src/
├── collections/
│   ├── mod.tml              # Re-exports (✅)
│   ├── class_collections.tml # Vec, HashSet, Queue, Stack, LinkedList (✅)
│   ├── hashmap.tml          # HashMap[K, V] (✅)
│   ├── list.tml             # List[T] (✅)
│   ├── btreemap.tml         # BTreeMap (I64-only) (✅)
│   ├── btreeset.tml         # BTreeSet (I64-only) (✅)
│   ├── deque.tml            # Deque[T] (✅)
│   ├── buffer.tml           # Buffer (✅)
│   └── behaviors.tml        # Collection behaviors (✅)
├── os.tml                   # Env, args, process, system info (✅)
├── datetime.tml             # DateTime (✅)
├── time.tml                 # Instant, SystemTime, Duration, sleep (✅)
├── random.tml               # Rng, ThreadRng, convenience fns (✅)
├── io/                      # NOT YET CREATED
│   ├── bufreader.tml        # BufReader[R] (pending)
│   ├── bufwriter.tml        # BufWriter[W] (pending)
│   └── linewriter.tml       # LineWriter[W] (pending)
└── path/                    # NOT YET CREATED
    └── mod.tml              # Path, PathBuf (pending)
```

## Dependencies

| Phase | Depends On |
|-------|------------|
| Phase 1 (Collections) | None |
| Phase 2 (Env/Process) | None |
| Phase 3 (Buffered I/O) | None |
| Phase 4 (Path) | None |
| Phase 5 (DateTime) | None |
| Phase 6 (Random) | None |
| Phase 7 (Integration) | All previous phases |

## Validation

- [x] V.1 All collections pass unit tests (btreemap, btreeset, deque, hashset, vec tests passing)
- [x] V.2 Environment and process work on Windows (via std::os FFI)
- [ ] V.3 Buffered I/O provides measurable performance improvement
- [ ] V.4 Path operations handle edge cases (empty, root, relative)
- [x] V.5 DateTime parsing handles common formats (ISO 8601, date, RFC 2822)
- [x] V.6 Random number generation tested (basic, shuffle, convenience, thread_rng)
- [x] V.7 All existing tests continue to pass
- [ ] V.8 Documentation complete with examples
- [ ] V.9 Performance within 20% of Rust equivalent
