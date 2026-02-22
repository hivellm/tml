# Tasks: Expand Core and Standard Library Modules

**Status**: In Progress (70%) — Phases 1-4, 8-9 complete, Phase 11 (OS) partially done (env/args exist in std::os), Phases 5-7, 10, 12-17 not started

**Note**: Many stdlib modules exist but are tracked by OTHER tasks (not this one). This task covers modules that don't have their own task.

**Already implemented (by other tasks)**:
- `std::collections` (HashMap, List, Buffer, HashSet, BTreeMap, BTreeSet, Deque) — see `stdlib-essentials`
- `std::crypto` (15 files: cipher, RSA, ECDH, HMAC, X.509, etc.)
- `std::net` (TCP, UDP, DNS, TLS, IP, Sockets) — see `async-network-stack`
- `std::json` (builder, serialize, types) — see `json-native-implementation` (archived)
- `std::zlib` (gzip, brotli, deflate, zstd, CRC32)
- `std::sync` (Mutex, RwLock, CondVar, Barrier, Arc, Atomic, MPSC, Once)
- `std::thread` (threads, scopes, thread-local)
- `std::file` (files, directories, paths)
- `std::search` (BM25, HNSW, string distance)
- `std::hash` (hash traits in core)
- `std::os` (OS interface, env, args, process)
- `std::random` (Rng, ThreadRng, shuffle)
- `std::regex` (Thompson's NFA, captures, replace, split) — see `implement-regex-module`
- `core::cell` (Cell, RefCell, UnsafeCell, Lazy, Once)
- `core::iter` (20+ adapter files, sources, traits)
- `core::fmt` (Display, Debug, number formatting, string interpolation)
- `core::future`, `core::task` (minimal/stub)

**Related Tasks**:
- Collections, Env, Process, DateTime, Random -> [stdlib-essentials](../stdlib-essentials/tasks.md)
- Regex -> [implement-regex-module](../../archive/) (archived)
- Networking -> [async-network-stack](../async-network-stack/tasks.md)
- Reflection -> [implement-reflection](../implement-reflection/tasks.md)
- SIMD -> [simd-optimization](../simd-optimization/tasks.md)

---

## Phase 1: Math Module

> **Priority**: Critical

- [x] 1.1.1 Math module exists at `lib/std/src/math.tml` (278 lines, F64-based)
- [x] 1.1.2 Trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- [x] 1.1.3 Hyperbolic: `sinh`, `cosh`, `tanh`
- [x] 1.1.4 Exponential/logarithmic: `exp`, `ln`, `log2`, `log10`, `pow`
- [x] 1.1.5 Power/root: `sqrt`, `cbrt`, `hypot`
- [x] 1.1.6 Rounding: `ceil`, `floor`, `round`, `trunc`
- [x] 1.1.7 Utility: `abs`, `min`, `max`, `clamp`, `to_radians`, `to_degrees`
- [x] 1.1.8 Constants: `PI`, `E`, `TAU`, `SQRT_2`, `LN_2`, `LN_10`, `LOG2_E`, `LOG10_E`
- [x] 1.1.9 C runtime linkage via `@extern("c")` for transcendental functions
- [ ] 1.1.10 F32 variants for all functions (currently F64 only)
- [ ] 1.1.11 `mul_add` (fused multiply-add)
- [ ] 1.1.12 `fract`, `copysign`, `signum`

## Phase 2: Core Encoding Module

> **Priority**: Critical | **Dir**: `lib/core/src/encoding/`

- [x] 2.1.1 Create `lib/core/src/encoding/mod.tml` with re-exports
- [x] 2.1.2 Implement `encoding/base64.tml` — `encode`, `decode`, standard + URL-safe alphabets (13 tests)
- [x] 2.1.3 Implement `encoding/hex.tml` — `encode`, `decode`, upper/lower case (9 tests)
- [x] 2.1.4 Implement `encoding/percent.tml` — `encode`, `decode` per RFC 3986 (10 tests)
- [x] 2.1.5 Implement `encoding/base32.tml` — `encode`, `decode` (24 tests)
- [x] 2.1.6 Implement `encoding/base58.tml` — `encode`, `decode` (24 tests)
- [x] 2.1.7 Implement `encoding/ascii85.tml` — `encode`, `decode` (24 tests)
- [x] 2.1.8 Write unit tests for all 6 encoding modules (145 tests passing)
- [x] 2.1.9 Export `core::encoding` from `lib/core/src/mod.tml`

## Phase 3: URL Module

> **Priority**: Critical | **File**: `lib/std/src/url.tml`

- [x] 3.1.1 Create `lib/std/src/url.tml`
- [x] 3.1.2 Design `Url` struct: scheme, userinfo, host, port, path, raw_query, fragment
- [x] 3.1.3 Implement `Url::parse(s: Str) -> Outcome[Url, Str]` per RFC 3986
- [x] 3.1.4 Implement accessors: `get_scheme()`, `get_host()`, `get_port()`, `get_path()`, `get_query()`, `get_fragment()`, `authority()`, `host_port()`
- [x] 3.1.5 Implement `query_pairs() -> List[QueryPair]`
- [x] 3.1.6 Implement `Url::join(this, relative: Str) -> Outcome[Url, Str]`
- [x] 3.1.7 Implement `to_string(this) -> Str`
- [x] 3.1.8 Implement `UrlBuilder` with fluent API (set_scheme, set_host, set_port, set_path, set_query, set_fragment, add_query, build)
- [x] 3.1.9 Write unit tests for `std::url` (23 tests: 7 parse, 4 format, 4 query, 4 builder, 4 join)

## Phase 4: SIMD Module

> **Priority**: High | **Dir**: `lib/core/src/simd/` | **See also**: `simd-optimization` task

- [x] 4.1.1 Create `lib/core/src/simd/mod.tml` with re-exports
- [x] 4.1.2 Implement `simd/i32x4.tml` — 4-lane I32 vector
- [x] 4.1.3 Implement `simd/f32x4.tml` — 4-lane F32 vector
- [x] 4.1.4 Implement `simd/i64x2.tml` — 2-lane I64 vector
- [x] 4.1.5 Implement `simd/f64x2.tml` — 2-lane F64 vector
- [x] 4.1.6 Implement `simd/i8x16.tml` — 16-lane I8 vector
- [x] 4.1.7 Implement `simd/u8x16.tml` — 16-lane U8 vector
- [x] 4.1.8 Implement lane ops: `splat`, `get`, `set`, `new`, `zero`
- [x] 4.1.9 Implement horizontal ops: `sum`, `product`, `hmin`, `hmax`, `min`, `max`
- [x] 4.1.10 Implement mask types (Mask2, Mask4, Mask16) and `select`
- [x] 4.1.11 `@simd` annotation — codegen emits LLVM vector types (`<N x T>`), `insertelement`/`extractelement`
- [x] 4.1.12 Write unit tests for `core::simd` (74 tests passing)

## Phase 5: Bitset Module

> **Priority**: Medium | **File**: `lib/core/src/bitset.tml`

- [ ] 5.1.1 Create `lib/core/src/bitset.tml`
- [ ] 5.1.2 Implement `BitSet[N]` — fixed-size bitset backed by U64 array
- [ ] 5.1.3 Implement `set`, `clear`, `toggle`, `get`, `count_ones`, `count_zeros`
- [ ] 5.1.4 Implement set operations: `union`, `intersection`, `difference`, `symmetric_difference`
- [ ] 5.1.5 Implement `is_subset`, `is_superset`, `is_disjoint`
- [ ] 5.1.6 Implement `Iterator` for set bits
- [ ] 5.1.7 Write unit tests

## Phase 6: RingBuf Module

> **Priority**: Medium | **File**: `lib/core/src/ringbuf.tml`

- [ ] 6.1.1 Create `lib/core/src/ringbuf.tml`
- [ ] 6.1.2 Implement `RingBuf[T, N]` — fixed-capacity circular buffer
- [ ] 6.1.3 Implement `push_back`, `push_front`, `pop_front`, `pop_back`
- [ ] 6.1.4 Implement `front`, `back`, `len`, `is_empty`, `is_full`
- [ ] 6.1.5 Implement `Iterator` and `Index` operator
- [ ] 6.1.6 Write unit tests

## Phase 7: MIME Module

> **Priority**: Low | **File**: `lib/std/src/mime.tml`

- [ ] 7.1.1 Create `lib/std/src/mime.tml`
- [ ] 7.1.2 Define `Mime` struct: type, subtype, parameters
- [ ] 7.1.3 Implement `Mime::parse(s: Str) -> Outcome[Mime, MimeError]`
- [ ] 7.1.4 Implement `from_extension(ext: Str) -> Maybe[Mime]`
- [ ] 7.1.5 Define common constants: `TEXT_PLAIN`, `TEXT_HTML`, `APPLICATION_JSON`, etc.
- [ ] 7.1.6 Write unit tests

## Phase 8: UUID Module

> **Priority**: High | **File**: `lib/std/src/uuid.tml`

- [x] 8.1.1 Create `lib/std/src/uuid.tml`
- [x] 8.1.2 Define `Uuid` struct (128-bit, stored as 4xI64 fields)
- [x] 8.1.3 Implement `Uuid::nil()`, `Uuid::max()`, `Uuid::parse()`
- [x] 8.1.4 Implement `to_string`, `to_urn`
- [x] 8.1.5 Implement `version()`, `variant()`
- [x] 8.1.6 Implement `Uuid::v4()` — random via std::random
- [x] 8.1.7 Implement `Uuid::v7()` — Unix epoch time + random (RFC 9562)
- [x] 8.1.8 Implement all UUID versions (v1-v8) per RFC 9562
- [ ] 8.1.9 Implement `Hash`, `Eq`, `Ord`, `Display`, `Debug` behaviors
- [x] 8.1.10 Write unit tests (20 tests: basic, generate, v7, parse)

> Commit `11d9d61`: feat(std): implement all UUID versions (v1-v8) per RFC 9562

## Phase 9: SemVer Module

> **Priority**: High | **File**: `lib/std/src/semver.tml`

- [x] 9.1.1 Create `lib/std/src/semver.tml`
- [x] 9.1.2 Define `Version` struct: major, minor, patch, pre-release, build metadata
- [x] 9.1.3 Implement `Version::parse`, `to_string`
- [x] 9.1.4 Implement `eq`, `compare`, `lt`, `gt`, `le`, `ge` (SemVer 2.0 precedence)
- [x] 9.1.5 Implement `VersionReq` — version requirement ranges (exact, gt, ge, lt, le, caret, tilde)
- [x] 9.1.6 Implement `VersionReq::parse` and `VersionReq::matches`
- [x] 9.1.7 Write unit tests (31 tests: basic, compare, parse, format, req)

## Phase 10: SQLite Module

> **Priority**: High | **File**: `lib/std/src/sqlite.tml`

- [ ] 10.1.1 Create `lib/std/src/sqlite.tml`
- [ ] 10.1.2 Implement FFI bindings to sqlite3 C API
- [ ] 10.1.3 Implement `Database::open`, `Database::open_in_memory()`
- [ ] 10.1.4 Implement `Database::execute`, `Database::prepare`
- [ ] 10.1.5 Implement `Statement` bind/step/column
- [ ] 10.1.6 Implement `Database::query(sql) -> Rows` iterator
- [ ] 10.1.7 Implement transactions: `begin`, `commit`, `rollback`
- [ ] 10.1.8 Bundle sqlite3 amalgamation source
- [ ] 10.1.9 Write unit tests

## Phase 11: OS Integration

> **Priority**: High

### 11.1 Subprocess (`lib/std/src/subprocess.tml`)
- [ ] 11.1.1 Design `Command` builder: `new`, `arg`, `args`, `env`, `current_dir`
- [ ] 11.1.2 Implement `stdin`, `stdout`, `stderr` config (Inherit, Piped, Null)
- [ ] 11.1.3 Implement `spawn`, `output`, `status`
- [ ] 11.1.4 Implement `Child::wait`, `Child::kill`, `Child::id`
- [ ] 11.1.5 Platform: `CreateProcess` (Windows), `fork/exec` (Unix)
- [ ] 11.1.6 Write unit tests

### 11.2 Signal Handling (`lib/std/src/signal.tml`)
- [ ] 11.2.1 Define `Signal` enum: `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGUSR1`, `SIGUSR2`
- [ ] 11.2.2 Implement `signal::on`, `signal::reset`, `signal::ignore`, `signal::wait`
- [ ] 11.2.3 Platform: `SetConsoleCtrlHandler` (Windows), `sigaction` (Unix)
- [ ] 11.2.4 Write unit tests

### 11.3 Pipes / IPC (`lib/std/src/pipe.tml`)
- [ ] 11.3.1 Implement anonymous pipes: `pipe() -> Outcome[(PipeReader, PipeWriter), IoError]`
- [ ] 11.3.2 Implement named pipes
- [ ] 11.3.3 Platform: `CreateNamedPipe` (Windows), `mkfifo` (Unix)
- [ ] 11.3.4 Write unit tests

## Phase 12: CLI Argument Parsing

> **Priority**: Medium | **File**: `lib/std/src/cli.tml`

- [ ] 12.1.1 Design `App` builder: `new`, `version`, `description`, `arg`, `subcommand`
- [ ] 12.1.2 Design `Arg` builder: `new`, `short`, `long`, `required`, `default_value`
- [ ] 12.1.3 Implement `App::parse() -> Outcome[Matches, CliError]`
- [ ] 12.1.4 Implement automatic `--help` and `--version` generation
- [ ] 12.1.5 Write unit tests

## Phase 13: Template Engine

> **Priority**: Low | **File**: `lib/std/src/template.tml`

- [ ] 13.1.1 Design template syntax: `{{ variable }}`, `{% if %}`, `{% for %}`
- [ ] 13.1.2 Implement template parser and renderer
- [ ] 13.1.3 Write unit tests

## Phase 14: Image I/O

> **Priority**: Low | **Dir**: `lib/std/src/image/`

- [ ] 14.1.1 Design `Image` struct: width, height, pixel format
- [ ] 14.1.2 Implement PNG decode/encode (uses `std::zlib`)
- [ ] 14.1.3 Implement JPEG decode/encode (baseline DCT)
- [ ] 14.1.4 Write unit tests

## Phase 15: Internationalization

> **Priority**: Low | **File**: `lib/std/src/i18n.tml`

- [ ] 15.1.1 Implement `Locale::current()`, `Locale::from_tag(tag)` (BCP 47)
- [ ] 15.1.2 Implement locale-aware number, date, currency formatting
- [ ] 15.1.3 Implement string collation
- [ ] 15.1.4 Write unit tests

## Phase 16: Test Framework — Mock and Property Testing

> **Priority**: Medium

### 16.1 Mocking Framework (`lib/test/src/mock.tml`)
- [ ] 16.1.1 Design `Mock[T]` type for creating mock objects from behaviors
- [ ] 16.1.2 Implement `when(method).returns(value)`, call counting, `verify()`
- [ ] 16.1.3 Write unit tests

### 16.2 Property-Based Testing (`lib/test/src/property.tml`)
- [ ] 16.2.1 Design `Arbitrary[T]` behavior — generate random values
- [ ] 16.2.2 Implement `prop_test(name, f)` with random inputs
- [ ] 16.2.3 Implement shrinking — minimize failing input
- [ ] 16.2.4 Write unit tests

## Phase 17: Integration and Validation

> **Priority**: High | **Depends on**: All previous phases

- [ ] 17.1.1 Run full test suite with coverage
- [ ] 17.1.2 Cross-module integration tests
- [ ] 17.1.3 Update all mod.tml files with new exports
- [ ] 17.1.4 Update coverage report
