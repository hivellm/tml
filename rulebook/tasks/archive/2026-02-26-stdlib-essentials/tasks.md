# Tasks: Standard Library Essentials

**Status**: In Progress (98%) - Core utilities needed for production use

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
- Path enhancements — basic Path exists in `lib/std/src/file/path.tml` (parent, join, extension, filename, absolute), needs `is_absolute`, `is_relative`, `file_stem`, `with_extension`, PathBuf type
- Vec higher-level methods (extend, drain, sort, dedup) — NOT yet implemented
- Duration `*` and `/` operator overloads — only `.mul()`/`.div()` methods exist, not `impl Mul`/`impl Div`
- Instant `checked_add`/`checked_sub` — NOT yet implemented

**Related Tasks**:
- Network I/O → `async-network-stack` (sync net already implemented)
- JSON → already implemented in `lib/std/src/json/`
- Sync → already implemented in `lib/std/src/sync/`

## Phase 1: Collection Aliases and Additions

> **Status**: Mostly Done — core types implemented, missing advanced methods and set operations

### 1.1 Vec[T] (in `class_collections.tml`)
- [x] 1.1.1 Create `Vec[T]` type in `lib/std/src/collections/class_collections.tml`
- [x] 1.1.2 Add `Vec::new()`, `Vec::with_capacity()` constructors
- [x] 1.1.3 Implement `extend()`, `append()` methods (drain deferred)
- [x] 1.1.4 Implement `dedup()`, `sort()`, `remove_all()` methods (retain deferred)
- [ ] 1.1.5 Add `Vec::from_iter()` for collection from iterators
- [x] 1.1.6 Add unit tests for Vec operations

### 1.2 HashSet[T] (in `class_collections.tml`)
- [x] 1.2.1 Design `HashSet[T]` in `lib/std/src/collections/class_collections.tml`
- [x] 1.2.2 Implement `create()`, `with_capacity()` constructors
- [x] 1.2.3 Implement `add()`, `remove()`, `contains()` methods
- [x] 1.2.4 Implement `count()`, `is_empty()`, `clear()` methods
- [x] 1.2.5 Implement set operations: `union_with()`, `intersection()`, `difference()`, `symmetric_difference()`
- [x] 1.2.6 Implement `is_subset()`, `is_superset()`, `is_disjoint()`
- [ ] 1.2.7 Implement `Iterator` for HashSet
- [x] 1.2.8 Add unit tests for HashSet

### 1.3 BTreeMap[K, V] (in `btreemap.tml`)
- [x] 1.3.1 Design `BTreeMap` in `lib/std/src/collections/btreemap.tml` (sorted arrays + binary search, I64-only)
- [x] 1.3.2 Implement sorted array structure with binary search
- [x] 1.3.3 Implement `insert()`, `get()`, `remove()` methods
- [x] 1.3.4 Implement range queries: lower_bound, upper_bound, range_count, range_key_at, range_value_at, key_at, value_at
- [ ] 1.3.5 Implement ordered iteration (Iterator behavior)
- [x] 1.3.6 Add unit tests for BTreeMap

### 1.4 BTreeSet[T] (in `btreeset.tml`)
- [x] 1.4.1 Design `BTreeSet` based on BTreeMap (I64-only)
- [x] 1.4.2 Implement all set operations: union_with, intersection, difference, symmetric_difference, is_subset, is_superset, is_disjoint, get_at
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
- [x] 2.2.3 Implement `Args` iterator struct (has_next, next, reset, len)
- [x] 2.2.4 Add simple argument parser utilities (has_flag, flag_value)

### 2.3 Process Management (basic in `std::os`, Command builder pending)
- [x] 2.3.1 Implement basic process functions in `lib/std/src/os.tml`
- [ ] 2.3.2 Implement `Command` builder type (tracked in `expand-core-std-modules` Phase 11)
- [ ] 2.3.3-2.3.13 Command builder methods (spawn, output, status, Child, pipes)
- [x] 2.3.14 Platform implementations via `@extern("c")` FFI
- [x] 2.3.15 Basic process operations: `process_exit()`, `exec()`, `exec_status()`, `pid()`

## Phase 3: Buffered I/O

> **Status**: Mostly Done — BufReader, BufWriter, LineWriter implemented in `lib/std/src/file/bufio.tml` (file-specific, not generic)

**Note**: Implemented as file-specific types (wrap `File`), not generic `BufReader[R: Read]`. Located in `lib/std/src/file/bufio.tml`, not `lib/std/src/io/`.

### 3.1 BufReader (in `lib/std/src/file/bufio.tml`)
- [x] 3.1.1 Design `BufReader` type wrapping File
- [x] 3.1.2 Implement `open(path: Str)` and `from_file(file: File)` constructors
- [x] 3.1.3 Implement `is_open()`, `is_eof()`, `lines_read()` accessors
- [x] 3.1.4 Implement `read_line()` — reads next line
- [x] 3.1.5 Implement `read_all()` — reads all remaining lines
- [x] 3.1.6 Implement `close()` — close file
- [x] 3.1.7 Implement `lines()` iterator (returns Lines iterator struct with has_next/next/close)
- [ ] 3.1.8 Generic `BufReader[R: Read]` (currently file-specific only)

### 3.2 BufWriter (in `lib/std/src/file/bufio.tml`)
- [x] 3.2.1 Design `BufWriter` type wrapping File with buffer
- [x] 3.2.2 Implement `open(path: Str)` and `open_append(path: Str)` constructors
- [x] 3.2.3 Implement `with_capacity(path: Str, capacity: I64)` constructor
- [x] 3.2.4 Implement `from_file(file: File)` constructor
- [x] 3.2.5 Implement `write(data: Str)` and `write_line(data: Str)` methods
- [x] 3.2.6 Implement `flush()` — flush buffer to disk
- [x] 3.2.7 Implement `buffered()`, `total_written()`, `capacity()` accessors
- [x] 3.2.8 Implement `close()` — flush and close

### 3.3 LineWriter (in `lib/std/src/file/bufio.tml`)
- [x] 3.3.1 Design `LineWriter` that auto-flushes on newline
- [x] 3.3.2 Implement `open()`, `open_append()`, `from_file()` constructors
- [x] 3.3.3 Implement `write()` with auto-flush on newline detection
- [x] 3.3.4 Implement `write_line()`, `flush()`, `close()` methods
- [x] 3.3.5 Add unit tests for buffered I/O (bufio.test.tml, bufwriter.test.tml, bufreader.test.tml, linewriter.test.tml)

## Phase 4: Path Utilities

> **Status**: Partially Done — Static Path operations in `lib/std/src/file/path.tml`, PathBuf not yet implemented

**Note**: Path is implemented as a type with static methods (taking `Str` paths), located in `lib/std/src/file/path.tml` (not `lib/std/src/path/`). No instance-based Path/PathBuf types yet.

### 4.1 Path Static Methods (in `lib/std/src/file/path.tml`)
- [x] 4.1.1 Design `Path` type with static methods
- [x] 4.1.2 Implement `Path::join(base: Str, child: Str) -> Str`
- [x] 4.1.3 Implement `Path::parent(path: Str) -> Str`
- [x] 4.1.4 Implement `Path::filename(path: Str) -> Str`
- [x] 4.1.5 Implement `Path::extension(path: Str) -> Str`
- [x] 4.1.6 Implement `Path::absolute(path: Str) -> Str`
- [x] 4.1.7 Implement `Path::exists(path: Str) -> Bool`
- [x] 4.1.8 Implement `Path::is_file(path: Str) -> Bool`
- [x] 4.1.9 Implement `Path::is_dir(path: Str) -> Bool`
- [x] 4.1.10 Implement `Path::remove()`, `rename()`, `copy()`, `create_dir()`, `create_dir_all()`, `remove_dir()`
- [x] 4.1.11 Implement `Path::file_stem(path: Str) -> Str`
- [x] 4.1.12 Implement `Path::is_absolute(path: Str) -> Bool`
- [x] 4.1.13 Implement `Path::is_relative(path: Str) -> Bool`
- [x] 4.1.14 Implement `Path::with_extension(path: Str, ext: Str) -> Str`

### 4.2 PathBuf Type (in `lib/std/src/file/path.tml`) — DONE
- [x] 4.2.1 Design `PathBuf` (owned, mutable path) type
- [x] 4.2.2 Implement `PathBuf::new()`, `PathBuf::from(s: Str)`
- [x] 4.2.3 Implement `push()`, `pop()`, `set_file_name()`, `set_extension()`
- [x] 4.2.4 Implement `as_str()`, `file_name()`, `file_stem()`, `extension()`, `parent()`, `is_absolute()`, `exists()` accessors

### 4.3 Platform-Specific
- [ ] 4.3.1 Handle path separators (`/` vs `\`)
- [ ] 4.3.2 Handle drive letters (Windows)
- [ ] 4.3.3 Handle UNC paths (Windows)
- [x] 4.3.4 Add unit tests for path operations (path_basic, path, pathbuf)

## Phase 5: DateTime

> **Status**: Mostly Done — Duration fully implemented in `lib/core/src/time.tml`, Instant/SystemTime in `lib/std/src/time.tml`, DateTime in `lib/std/src/datetime.tml`

### 5.1 Duration (in `lib/core/src/time.tml`) — DONE
- [x] 5.1.1 Duration type with secs/nanos fields in `lib/core/src/time.tml`
- [x] 5.1.2 `from_secs(secs: I64) -> Duration`
- [x] 5.1.3 `from_millis(millis: I64) -> Duration`
- [x] 5.1.4 `from_micros(micros: I64) -> Duration`
- [x] 5.1.5 `from_nanos(nanos: I64) -> Duration`
- [x] 5.1.6 `as_secs(this) -> I64`
- [x] 5.1.7 `as_millis(this) -> I64` (also `as_micros()`)
- [x] 5.1.8 `subsec_nanos(this) -> I32`
- [x] 5.1.9 Arithmetic: `+` (impl Add), `-` (impl Sub) operators; `.mul()`, `.div()` methods
- [x] 5.1.9a Arithmetic: `*` and `/` operator overloads (impl Mul[I32], impl Div[I32])
- [x] 5.1.10 `checked_add()`, `checked_sub()`, `saturating_add()`, `saturating_sub()`

### 5.2 Instant (in `lib/std/src/time.tml`) — DONE
- [x] 5.2.1 Design `Instant` for monotonic time
- [x] 5.2.2 Implement `Instant::now() -> Instant`
- [x] 5.2.3 Implement `elapsed(this) -> Duration`
- [x] 5.2.4 Implement `duration_since(this, earlier: Instant) -> Duration`
- [x] 5.2.5 Implement `checked_add(this, dur: Duration) -> Maybe[Instant]`
- [x] 5.2.6 Implement `checked_sub(this, dur: Duration) -> Maybe[Instant]`
- [x] 5.2.7 Platform implementations via `@extern("c")` FFI
- [x] 5.2.8 Implement `as_nanos(this) -> I64`

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
- [x] 6.1.4 Implement `fill_bytes(this, buf: List[I64])`, `next_u8()`, `next_i32()`

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
- [ ] 6.4.1 Design `Distribution[T]` behavior (needs generics)
- [x] 6.4.2 Implement `Uniform` distribution (new, sample, lower, upper)
- [x] 6.4.3 Implement `Bernoulli` distribution (new, sample, probability, clamp)
- [x] 6.4.4 Add unit tests for random generation (basic, shuffle, convenience, thread_rng, distributions)

## Phase 7: Integration Testing

> **Status**: Partially Done

### 7.1 Cross-Module Tests
- [x] 7.1.1 Create integration test: file + buffered I/O (bufio.test.tml covers write+read)
- [x] 7.1.2 Create integration test: process + environment (integ_env_process.test.tml)
- [x] 7.1.3 Create integration test: datetime formatting (integ_datetime.test.tml)
- [x] 7.1.4 Create integration test: random + collections (integ_random_collections.test.tml)

### 7.2 Benchmarks
- [ ] 7.2.1 Benchmark collection operations vs Rust
- [ ] 7.2.2 Benchmark I/O throughput vs Rust
- [ ] 7.2.3 Benchmark string operations vs Rust
- [ ] 7.2.4 Document performance results

## File Structure

```
lib/core/src/
├── time.tml                 # Duration (from_secs, from_millis, checked_add, etc.) (✅)

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
├── file/
│   ├── bufio.tml            # BufReader, BufWriter, LineWriter (file-specific) (✅)
│   └── path.tml             # Path static methods (join, parent, extension, etc.) (✅)
├── os.tml                   # Env, args, process, system info (✅)
├── datetime.tml             # DateTime (✅)
├── time.tml                 # Instant, SystemTime, sleep (✅)
└── random.tml               # Rng, ThreadRng, convenience fns (✅)
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
- [x] V.3 Buffered I/O types implemented (BufReader, BufWriter, LineWriter in bufio.tml)
- [x] V.4 Path static operations implemented (join, parent, filename, extension, absolute)
- [x] V.5 DateTime parsing handles common formats (ISO 8601, date, RFC 2822)
- [x] V.6 Random number generation tested (basic, shuffle, convenience, thread_rng)
- [x] V.7 All existing tests continue to pass
- [ ] V.8 Documentation complete with examples
- [ ] V.9 Performance within 20% of Rust equivalent
