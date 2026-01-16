# Tasks: Standard Library Essentials

**Status**: Planning (0%) - Core utilities needed for production use

**Note**: This task covers essential standard library modules that make TML usable for real-world applications. These are the missing pieces between a working compiler and a production-ready language.

**Related Tasks**:
- Network I/O → [add-network-stdlib](../add-network-stdlib/tasks.md)
- Concurrency → [thread-safe-native](../thread-safe-native/tasks.md)
- JSON → [json-native-implementation](../json-native-implementation/tasks.md)

## Phase 1: Collection Aliases and Additions

> **Status**: Pending

### 1.1 Vec[T] Alias
- [ ] 1.1.1 Create `Vec[T]` as alias for `List[T]` in `lib/std/src/collections/vec.tml`
- [ ] 1.1.2 Add `Vec::new()`, `Vec::with_capacity()` constructors
- [ ] 1.1.3 Implement `extend()`, `append()`, `drain()` methods
- [ ] 1.1.4 Implement `retain()`, `dedup()`, `sort()` methods
- [ ] 1.1.5 Add `Vec::from_iter()` for collection from iterators
- [ ] 1.1.6 Add unit tests for Vec operations

### 1.2 HashSet[T]
- [ ] 1.2.1 Design `HashSet[T]` in `lib/std/src/collections/hashset.tml`
- [ ] 1.2.2 Implement `new()`, `with_capacity()` constructors
- [ ] 1.2.3 Implement `insert()`, `remove()`, `contains()` methods
- [ ] 1.2.4 Implement `len()`, `is_empty()`, `clear()` methods
- [ ] 1.2.5 Implement set operations: `union()`, `intersection()`, `difference()`, `symmetric_difference()`
- [ ] 1.2.6 Implement `is_subset()`, `is_superset()`, `is_disjoint()`
- [ ] 1.2.7 Implement `Iterator` for HashSet
- [ ] 1.2.8 Add unit tests for HashSet

### 1.3 BTreeMap[K, V]
- [ ] 1.3.1 Design `BTreeMap[K, V]` in `lib/std/src/collections/btreemap.tml`
- [ ] 1.3.2 Implement B-tree node structure (order 6)
- [ ] 1.3.3 Implement `insert()`, `get()`, `remove()` methods
- [ ] 1.3.4 Implement `range()` for range queries
- [ ] 1.3.5 Implement ordered iteration
- [ ] 1.3.6 Add unit tests for BTreeMap

### 1.4 BTreeSet[T]
- [ ] 1.4.1 Design `BTreeSet[T]` based on BTreeMap
- [ ] 1.4.2 Implement all set operations
- [ ] 1.4.3 Implement ordered iteration
- [ ] 1.4.4 Add unit tests for BTreeSet

### 1.5 Deque[T]
- [ ] 1.5.1 Design `Deque[T]` (ring buffer) in `lib/std/src/collections/deque.tml`
- [ ] 1.5.2 Implement `push_front()`, `push_back()`, `pop_front()`, `pop_back()`
- [ ] 1.5.3 Implement `front()`, `back()`, `get()` accessors
- [ ] 1.5.4 Implement growth strategy (doubling)
- [ ] 1.5.5 Add unit tests for Deque

## Phase 2: Environment and Process

> **Status**: Pending

### 2.1 Environment Variables
- [ ] 2.1.1 Design `std::env` module in `lib/std/src/env/mod.tml`
- [ ] 2.1.2 Implement `env::var(name: Str) -> Maybe[Str]`
- [ ] 2.1.3 Implement `env::set_var(name: Str, value: Str)`
- [ ] 2.1.4 Implement `env::remove_var(name: Str)`
- [ ] 2.1.5 Implement `env::vars() -> Iterator[(Str, Str)]`
- [ ] 2.1.6 Implement `env::current_dir() -> Outcome[Path, IoError]`
- [ ] 2.1.7 Implement `env::set_current_dir(path: Path) -> Outcome[Unit, IoError]`
- [ ] 2.1.8 Implement `env::home_dir() -> Maybe[Path]`
- [ ] 2.1.9 Implement `env::temp_dir() -> Path`

### 2.2 Command Line Arguments
- [ ] 2.2.1 Design `std::args` module in `lib/std/src/args/mod.tml`
- [ ] 2.2.2 Implement `args::args() -> Iterator[Str]`
- [ ] 2.2.3 Implement `args::Args` struct with iteration
- [ ] 2.2.4 Add simple argument parser utilities

### 2.3 Process Management
- [ ] 2.3.1 Design `std::process` module in `lib/std/src/process/mod.tml`
- [ ] 2.3.2 Implement `Command` builder type
- [ ] 2.3.3 Implement `Command::new(program: Str) -> Command`
- [ ] 2.3.4 Implement `Command::arg(this, arg: Str) -> Command`
- [ ] 2.3.5 Implement `Command::args(this, args: impl Iterator[Str]) -> Command`
- [ ] 2.3.6 Implement `Command::env(this, key: Str, val: Str) -> Command`
- [ ] 2.3.7 Implement `Command::current_dir(this, dir: Path) -> Command`
- [ ] 2.3.8 Implement `Command::spawn(this) -> Outcome[Child, IoError]`
- [ ] 2.3.9 Implement `Command::output(this) -> Outcome[Output, IoError]`
- [ ] 2.3.10 Implement `Command::status(this) -> Outcome[ExitStatus, IoError]`
- [ ] 2.3.11 Implement `Child` type with stdin/stdout/stderr pipes
- [ ] 2.3.12 Implement `Child::wait(this) -> Outcome[ExitStatus, IoError]`
- [ ] 2.3.13 Implement `Child::kill(this) -> Outcome[Unit, IoError]`
- [ ] 2.3.14 Add platform implementations (Windows/Unix)
- [ ] 2.3.15 Add unit tests for process management

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

> **Status**: Pending

### 5.1 Duration (Enhancement)
- [ ] 5.1.1 Enhance existing `Duration` in `lib/std/src/time/mod.tml`
- [ ] 5.1.2 Add `from_secs(secs: I64) -> Duration`
- [ ] 5.1.3 Add `from_millis(millis: I64) -> Duration`
- [ ] 5.1.4 Add `from_micros(micros: I64) -> Duration`
- [ ] 5.1.5 Add `from_nanos(nanos: I64) -> Duration`
- [ ] 5.1.6 Add `as_secs(this) -> I64`
- [ ] 5.1.7 Add `as_millis(this) -> I64`
- [ ] 5.1.8 Add `subsec_nanos(this) -> I32`
- [ ] 5.1.9 Add arithmetic operations (`+`, `-`, `*`, `/`)
- [ ] 5.1.10 Add `checked_add()`, `checked_sub()`, `saturating_*()` variants

### 5.2 Instant
- [ ] 5.2.1 Design `Instant` for monotonic time in `lib/std/src/time/instant.tml`
- [ ] 5.2.2 Implement `Instant::now() -> Instant`
- [ ] 5.2.3 Implement `elapsed(this) -> Duration`
- [ ] 5.2.4 Implement `duration_since(this, earlier: Instant) -> Duration`
- [ ] 5.2.5 Implement `checked_add(this, dur: Duration) -> Maybe[Instant]`
- [ ] 5.2.6 Implement `checked_sub(this, dur: Duration) -> Maybe[Instant]`
- [ ] 5.2.7 Platform implementations (QueryPerformanceCounter/clock_gettime)

### 5.3 SystemTime
- [ ] 5.3.1 Design `SystemTime` for wall-clock time
- [ ] 5.3.2 Implement `SystemTime::now() -> SystemTime`
- [ ] 5.3.3 Implement `UNIX_EPOCH` constant
- [ ] 5.3.4 Implement `duration_since(this, earlier: SystemTime) -> Outcome[Duration, SystemTimeError]`
- [ ] 5.3.5 Implement `elapsed(this) -> Outcome[Duration, SystemTimeError]`

### 5.4 DateTime (High-Level)
- [ ] 5.4.1 Design `DateTime` struct with year/month/day/hour/min/sec
- [ ] 5.4.2 Implement `DateTime::now() -> DateTime`
- [ ] 5.4.3 Implement `DateTime::from_timestamp(ts: I64) -> DateTime`
- [ ] 5.4.4 Implement `timestamp(this) -> I64`
- [ ] 5.4.5 Implement component accessors (year, month, day, etc.)
- [ ] 5.4.6 Implement `format(this, fmt: Str) -> Text` (basic format strings)
- [ ] 5.4.7 Implement `parse(s: Str, fmt: Str) -> Outcome[DateTime, ParseError]`
- [ ] 5.4.8 Add unit tests for DateTime

## Phase 6: Random Number Generation

> **Status**: Pending

### 6.1 Random Trait
- [ ] 6.1.1 Design `Rng` behavior in `lib/std/src/random/mod.tml`
- [ ] 6.1.2 Define `next_u32(this) -> U32`
- [ ] 6.1.3 Define `next_u64(this) -> U64`
- [ ] 6.1.4 Define `fill_bytes(this, buf: mut ref [U8])`

### 6.2 ThreadRng
- [ ] 6.2.1 Design `ThreadRng` (thread-local RNG)
- [ ] 6.2.2 Implement using ChaCha or Xoshiro algorithm
- [ ] 6.2.3 Implement `thread_rng() -> ThreadRng`
- [ ] 6.2.4 Implement `Rng` behavior for ThreadRng

### 6.3 Convenience Functions
- [ ] 6.3.1 Implement `random[T: Random]() -> T`
- [ ] 6.3.2 Implement `random_range(min: I64, max: I64) -> I64`
- [ ] 6.3.3 Implement `random_bool() -> Bool`
- [ ] 6.3.4 Implement `shuffle[T](slice: mut ref [T])`
- [ ] 6.3.5 Implement `choose[T](slice: ref [T]) -> Maybe[ref T]`

### 6.4 Distributions
- [ ] 6.4.1 Design `Distribution[T]` behavior
- [ ] 6.4.2 Implement `Uniform` distribution
- [ ] 6.4.3 Implement `Bernoulli` distribution
- [ ] 6.4.4 Add unit tests for random generation

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
│   ├── mod.tml              # Re-exports
│   ├── vec.tml              # Vec[T] (alias for List)
│   ├── hashset.tml          # HashSet[T]
│   ├── btreemap.tml         # BTreeMap[K, V]
│   ├── btreeset.tml         # BTreeSet[T]
│   └── deque.tml            # Deque[T]
├── env/
│   └── mod.tml              # Environment variables
├── args/
│   └── mod.tml              # Command line arguments
├── process/
│   └── mod.tml              # Process spawning
├── io/
│   ├── bufreader.tml        # BufReader[R]
│   ├── bufwriter.tml        # BufWriter[W]
│   └── linewriter.tml       # LineWriter[W]
├── path/
│   └── mod.tml              # Path, PathBuf
├── time/
│   ├── mod.tml              # Duration (enhanced)
│   ├── instant.tml          # Instant
│   ├── systemtime.tml       # SystemTime
│   └── datetime.tml         # DateTime
└── random/
    └── mod.tml              # Rng, ThreadRng, functions
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

- [ ] V.1 All collections pass unit tests
- [ ] V.2 Environment and process work on Windows and Linux
- [ ] V.3 Buffered I/O provides measurable performance improvement
- [ ] V.4 Path operations handle edge cases (empty, root, relative)
- [ ] V.5 DateTime parsing handles common formats
- [ ] V.6 Random number generation is uniform
- [ ] V.7 All existing tests continue to pass
- [ ] V.8 Documentation complete with examples
- [ ] V.9 Performance within 20% of Rust equivalent
