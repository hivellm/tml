# Tasks: Expand Core and Standard Library Modules

**Status**: Planning (0%)

**Related Tasks**:
- Collections, Env, Process, Path, DateTime, Random -> [stdlib-essentials](../stdlib-essentials/tasks.md)
- Regex -> [implement-regex-module](../implement-regex-module/tasks.md)
- Networking -> [async-network-stack](../async-network-stack/tasks.md)
- Reflection -> [implement-reflection](../implement-reflection/tasks.md)
- Serialization formats (TOML, YAML, CSV, XML, MessagePack, Protobuf) -> separate tasks
- Archive/tar -> extend `std::zlib`

---

## Phase 1: Core Math Module

> **Priority**: Critical | **File**: `lib/core/src/math.tml`

- [ ] 1.1.1 Create `lib/core/src/math.tml`
- [ ] 1.1.2 Implement trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- [ ] 1.1.3 Implement hyperbolic: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
- [ ] 1.1.4 Implement exponential/logarithmic: `exp`, `exp2`, `ln`, `log2`, `log10`, `log`
- [ ] 1.1.5 Implement power/root: `sqrt`, `cbrt`, `pow`, `hypot`
- [ ] 1.1.6 Implement rounding: `ceil`, `floor`, `round`, `trunc`, `fract`
- [ ] 1.1.7 Implement utility: `abs`, `min`, `max`, `clamp`, `copysign`, `signum`
- [ ] 1.1.8 Implement FMA: `mul_add` (fused multiply-add)
- [ ] 1.1.9 Define constants: `PI`, `E`, `TAU`, `SQRT_2`, `LN_2`, `LN_10`, `INFINITY`, `NAN`
- [ ] 1.1.10 Implement F32 and F64 variants for all functions
- [ ] 1.1.11 Add C runtime linkage (`libm` / MSVC CRT)
- [ ] 1.1.12 Write unit tests for `core::math`
- [ ] 1.1.13 Export `core::math` from `lib/core/src/mod.tml`

## Phase 2: Core Encoding Module

> **Priority**: Critical | **Dir**: `lib/core/src/encoding/`

- [ ] 2.1.1 Create `lib/core/src/encoding/mod.tml` with re-exports
- [ ] 2.1.2 Implement `encoding/base64.tml` — `encode`, `decode`, standard + URL-safe alphabets, padding options
- [ ] 2.1.3 Implement `encoding/hex.tml` — `encode`, `decode`, upper/lower case
- [ ] 2.1.4 Implement `encoding/percent.tml` — `encode`, `decode` per RFC 3986
- [ ] 2.1.5 Implement `encoding/base32.tml` — `encode`, `decode`, standard + hex alphabets
- [ ] 2.1.6 Implement `encoding/base58.tml` — `encode`, `decode` (Bitcoin alphabet)
- [ ] 2.1.7 Implement `encoding/ascii85.tml` — `encode`, `decode`
- [ ] 2.1.8 Write unit tests for each encoding format
- [ ] 2.1.9 Export `core::encoding` from `lib/core/src/mod.tml`

## Phase 3: Core URL Module

> **Priority**: Critical | **File**: `lib/core/src/url.tml`

- [ ] 3.1.1 Create `lib/core/src/url.tml`
- [ ] 3.1.2 Design `Url` struct: scheme, authority, userinfo, host, port, path, query, fragment
- [ ] 3.1.3 Implement `Url::parse(s: Str) -> Outcome[Url, UrlError]` per RFC 3986
- [ ] 3.1.4 Implement accessors: `scheme()`, `host()`, `port()`, `path()`, `query()`, `fragment()`
- [ ] 3.1.5 Implement `query_pairs() -> Iterator[(Str, Str)]`
- [ ] 3.1.6 Implement `Url::join(this, relative: Str) -> Outcome[Url, UrlError]`
- [ ] 3.1.7 Implement `to_string(this) -> Str`
- [ ] 3.1.8 Implement `UrlBuilder` for constructing URLs fluently
- [ ] 3.1.9 Write unit tests for `core::url`
- [ ] 3.1.10 Export `core::url` from `lib/core/src/mod.tml`

## Phase 4: Core SIMD Module

> **Priority**: High | **Dir**: `lib/core/src/simd/`

- [ ] 4.1.1 Create `lib/core/src/simd/mod.tml` with re-exports
- [ ] 4.1.2 Implement `simd/i32x4.tml` — 4-lane I32 vector (add, sub, mul, min, max, eq, lt, gt)
- [ ] 4.1.3 Implement `simd/f32x4.tml` — 4-lane F32 vector (add, sub, mul, div, sqrt, min, max, fma)
- [ ] 4.1.4 Implement `simd/i64x2.tml` — 2-lane I64 vector
- [ ] 4.1.5 Implement `simd/f64x2.tml` — 2-lane F64 vector
- [ ] 4.1.6 Implement `simd/i8x16.tml` — 16-lane I8 vector (for string ops)
- [ ] 4.1.7 Implement `simd/u8x16.tml` — 16-lane U8 vector
- [ ] 4.1.8 Implement lane operations: `splat`, `extract`, `replace`, `shuffle`
- [ ] 4.1.9 Implement horizontal operations: `sum`, `product`, `min`, `max`
- [ ] 4.1.10 Implement mask types: `Mask32x4`, `Mask64x2`
- [ ] 4.1.11 Implement `select` (blend/conditional) using masks
- [ ] 4.1.12 Map to LLVM vector intrinsics in codegen
- [ ] 4.1.13 Write unit tests for `core::simd`
- [ ] 4.1.14 Export `core::simd` from `lib/core/src/mod.tml`

## Phase 5: Core Bitset Module

> **Priority**: Medium | **File**: `lib/core/src/bitset.tml`

- [ ] 5.1.1 Create `lib/core/src/bitset.tml`
- [ ] 5.1.2 Implement `BitSet[N]` — fixed-size bitset backed by U64 array
- [ ] 5.1.3 Implement `set`, `clear`, `toggle`, `get`, `count_ones`, `count_zeros`
- [ ] 5.1.4 Implement set operations: `union`, `intersection`, `difference`, `symmetric_difference`
- [ ] 5.1.5 Implement `is_subset`, `is_superset`, `is_disjoint`
- [ ] 5.1.6 Implement `Iterator` for set bits
- [ ] 5.1.7 Implement `Display` and `Debug`
- [ ] 5.1.8 Write unit tests for `core::bitset`
- [ ] 5.1.9 Export `core::bitset` from `lib/core/src/mod.tml`

## Phase 6: Core RingBuf Module

> **Priority**: Medium | **File**: `lib/core/src/ringbuf.tml`

- [ ] 6.1.1 Create `lib/core/src/ringbuf.tml`
- [ ] 6.1.2 Implement `RingBuf[T, N]` — fixed-capacity circular buffer
- [ ] 6.1.3 Implement `push_back`, `push_front` (returns evicted if full)
- [ ] 6.1.4 Implement `pop_front`, `pop_back`
- [ ] 6.1.5 Implement `front`, `back`, `len`, `is_empty`, `is_full`
- [ ] 6.1.6 Implement `Iterator` and `Index` operator
- [ ] 6.1.7 Write unit tests for `core::ringbuf`
- [ ] 6.1.8 Export `core::ringbuf` from `lib/core/src/mod.tml`

## Phase 7: Core MIME Module

> **Priority**: Low | **File**: `lib/core/src/mime.tml`

- [ ] 7.1.1 Create `lib/core/src/mime.tml`
- [ ] 7.1.2 Define `Mime` struct: type, subtype, parameters
- [ ] 7.1.3 Implement `Mime::parse(s: Str) -> Outcome[Mime, MimeError]`
- [ ] 7.1.4 Implement `from_extension(ext: Str) -> Maybe[Mime]`
- [ ] 7.1.5 Define common constants: `TEXT_PLAIN`, `TEXT_HTML`, `APPLICATION_JSON`, `APPLICATION_OCTET_STREAM`, etc.
- [ ] 7.1.6 Implement `to_string(this) -> Str`
- [ ] 7.1.7 Write unit tests for `core::mime`
- [ ] 7.1.8 Export `core::mime` from `lib/core/src/mod.tml`

## Phase 8: Core UUID Module

> **Priority**: High | **File**: `lib/core/src/uuid.tml`

- [ ] 8.1.1 Create `lib/core/src/uuid.tml`
- [ ] 8.1.2 Define `Uuid` struct (128-bit, stored as `[U8; 16]`)
- [ ] 8.1.3 Implement `Uuid::nil()` and `Uuid::max()`
- [ ] 8.1.4 Implement `Uuid::parse(s: Str) -> Outcome[Uuid, UuidError]` (8-4-4-4-12 format)
- [ ] 8.1.5 Implement `to_string`, `to_urn`
- [ ] 8.1.6 Implement `version()`, `variant()`
- [ ] 8.1.7 Implement `Uuid::v1(timestamp, clock_seq, node)` — timestamp-based
- [ ] 8.1.8 Implement `Uuid::v3(namespace, name)` — MD5-based
- [ ] 8.1.9 Implement `Uuid::v4()` — random (requires `std::random` at call site)
- [ ] 8.1.10 Implement `Uuid::v5(namespace, name)` — SHA1-based
- [ ] 8.1.11 Implement `Uuid::v6(timestamp, clock_seq, node)` — reordered time (RFC 9562)
- [ ] 8.1.12 Implement `Uuid::v7()` — Unix epoch time + random (RFC 9562)
- [ ] 8.1.13 Implement `Uuid::v8(custom)` — custom data (RFC 9562)
- [ ] 8.1.14 Define namespace constants: `DNS`, `URL`, `OID`, `X500`
- [ ] 8.1.15 Implement `Hash`, `Eq`, `Ord`, `Display`, `Debug` behaviors
- [ ] 8.1.16 Write unit tests for all UUID versions
- [ ] 8.1.17 Export `core::uuid` from `lib/core/src/mod.tml`

## Phase 9: Core SemVer Module

> **Priority**: High | **File**: `lib/core/src/semver.tml`

- [ ] 9.1.1 Create `lib/core/src/semver.tml`
- [ ] 9.1.2 Define `Version` struct: major, minor, patch, pre-release, build metadata
- [ ] 9.1.3 Implement `Version::parse(s: Str) -> Outcome[Version, SemVerError]`
- [ ] 9.1.4 Implement `to_string`
- [ ] 9.1.5 Implement `PartialEq`, `Eq`, `PartialOrd`, `Ord` (SemVer 2.0 precedence)
- [ ] 9.1.6 Implement `is_prerelease`, `increment_major`, `increment_minor`, `increment_patch`
- [ ] 9.1.7 Implement `VersionReq` — version requirement ranges
- [ ] 9.1.8 Implement `VersionReq::parse` and `VersionReq::matches`
- [ ] 9.1.9 Support range operators: `^`, `~`, `>=`, `<=`, `>`, `<`, `=`, `*`
- [ ] 9.1.10 Write unit tests for `core::semver`
- [ ] 9.1.11 Export `core::semver` from `lib/core/src/mod.tml`

## Phase 10: Standard Library — SQLite

> **Priority**: High | **File**: `lib/std/src/sqlite.tml`

- [ ] 10.1.1 Create `lib/std/src/sqlite.tml`
- [ ] 10.1.2 Implement FFI bindings to sqlite3 C API
- [ ] 10.1.3 Implement `Database::open(path)`, `Database::open_in_memory()`
- [ ] 10.1.4 Implement `Database::execute(sql)`, `Database::prepare(sql)`
- [ ] 10.1.5 Implement `Statement` — `bind_int`, `bind_float`, `bind_text`, `bind_blob`, `bind_null`
- [ ] 10.1.6 Implement `Statement::step()`, column accessors
- [ ] 10.1.7 Implement `Database::query(sql) -> Rows` with `Rows` iterator
- [ ] 10.1.8 Implement `Row` accessors: `get_int`, `get_float`, `get_text`, `get_blob`
- [ ] 10.1.9 Implement transactions: `begin()`, `commit()`, `rollback()`
- [ ] 10.1.10 Implement `Database::close()` with proper cleanup
- [ ] 10.1.11 Bundle sqlite3 amalgamation source for embedded build
- [ ] 10.1.12 Write unit tests for `std::sqlite`
- [ ] 10.1.13 Export `std::sqlite` from `lib/std/src/mod.tml`

## Phase 11: Standard Library — OS Integration

> **Priority**: High

### 11.1 Subprocess (`lib/std/src/subprocess.tml`)
- [ ] 11.1.1 Create `lib/std/src/subprocess.tml`
- [ ] 11.1.2 Design `Command` builder: `new`, `arg`, `args`, `env`, `current_dir`
- [ ] 11.1.3 Implement `stdin`, `stdout`, `stderr` config (Inherit, Piped, Null)
- [ ] 11.1.4 Implement `spawn() -> Outcome[Child, IoError]`
- [ ] 11.1.5 Implement `output()` (run and capture), `status()` (run and get exit code)
- [ ] 11.1.6 Implement `Child::wait()`, `Child::kill()`, `Child::id()`
- [ ] 11.1.7 Platform: `CreateProcess` (Windows), `fork/exec` (Unix)
- [ ] 11.1.8 Write unit tests for `std::subprocess`
- [ ] 11.1.9 Export `std::subprocess` from `lib/std/src/mod.tml`

### 11.2 Signal Handling (`lib/std/src/signal.tml`)
- [ ] 11.2.1 Create `lib/std/src/signal.tml`
- [ ] 11.2.2 Define `Signal` enum: `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGUSR1`, `SIGUSR2`
- [ ] 11.2.3 Implement `signal::on(sig, handler)`, `signal::reset(sig)`, `signal::ignore(sig)`
- [ ] 11.2.4 Implement `signal::wait(sig)` — block until signal received
- [ ] 11.2.5 Platform: `SetConsoleCtrlHandler` (Windows), `sigaction` (Unix)
- [ ] 11.2.6 Write unit tests for `std::signal`
- [ ] 11.2.7 Export `std::signal` from `lib/std/src/mod.tml`

### 11.3 Pipes / IPC (`lib/std/src/pipe.tml`)
- [ ] 11.3.1 Create `lib/std/src/pipe.tml`
- [ ] 11.3.2 Implement anonymous pipes: `pipe() -> Outcome[(PipeReader, PipeWriter), IoError]`
- [ ] 11.3.3 Implement named pipes: `NamedPipe::create(name)`, `NamedPipe::connect(name)`
- [ ] 11.3.4 Implement `Read` and `Write` behaviors for pipes
- [ ] 11.3.5 Platform: `CreateNamedPipe` (Windows), `mkfifo` (Unix)
- [ ] 11.3.6 Write unit tests for `std::pipe`
- [ ] 11.3.7 Export `std::pipe` from `lib/std/src/mod.tml`

## Phase 12: Standard Library — CLI Argument Parsing

> **Priority**: Medium | **File**: `lib/std/src/cli.tml`

- [ ] 12.1.1 Create `lib/std/src/cli.tml`
- [ ] 12.1.2 Design `App` builder: `new`, `version`, `description`, `arg`, `subcommand`
- [ ] 12.1.3 Design `Arg` builder: `new`, `short`, `long`, `required`, `default_value`, `takes_value`
- [ ] 12.1.4 Implement `App::parse() -> Outcome[Matches, CliError]`
- [ ] 12.1.5 Implement `Matches::get`, `Matches::is_present`, `Matches::subcommand`
- [ ] 12.1.6 Implement automatic `--help` and `--version` generation
- [ ] 12.1.7 Write unit tests for `std::cli`
- [ ] 12.1.8 Export `std::cli` from `lib/std/src/mod.tml`

## Phase 13: Standard Library — Template Engine

> **Priority**: Low | **File**: `lib/std/src/template.tml`

- [ ] 13.1.1 Create `lib/std/src/template.tml`
- [ ] 13.1.2 Design template syntax: `{{ variable }}`, `{% if %}`, `{% for %}`, `{% block %}`
- [ ] 13.1.3 Implement template parser and renderer
- [ ] 13.1.4 Implement conditionals, loops, filters (`upper`, `escape`, etc.)
- [ ] 13.1.5 Write unit tests for `std::template`
- [ ] 13.1.6 Export `std::template` from `lib/std/src/mod.tml`

## Phase 14: Standard Library — Image I/O

> **Priority**: Low | **Dir**: `lib/std/src/image/`

- [ ] 14.1.1 Create `lib/std/src/image/mod.tml`
- [ ] 14.1.2 Design `Image` struct: width, height, pixel format (RGB, RGBA, Grayscale)
- [ ] 14.1.3 Implement `Image::new`, `Image::load`, `Image::save`
- [ ] 14.1.4 Implement `image/png.tml` — PNG decode/encode (uses `std::zlib`)
- [ ] 14.1.5 Implement `image/jpeg.tml` — JPEG decode/encode (baseline DCT)
- [ ] 14.1.6 Implement pixel access: `get_pixel`, `set_pixel`
- [ ] 14.1.7 Write unit tests for `std::image`
- [ ] 14.1.8 Export `std::image` from `lib/std/src/mod.tml`

## Phase 15: Standard Library — Internationalization

> **Priority**: Low | **File**: `lib/std/src/i18n.tml`

- [ ] 15.1.1 Create `lib/std/src/i18n.tml`
- [ ] 15.1.2 Implement `Locale::current()`, `Locale::from_tag(tag)` (BCP 47)
- [ ] 15.1.3 Implement locale-aware number, date, and currency formatting
- [ ] 15.1.4 Implement string collation (locale-aware sorting)
- [ ] 15.1.5 Write unit tests for `std::i18n`
- [ ] 15.1.6 Export `std::i18n` from `lib/std/src/mod.tml`

## Phase 16: Test Framework — Mock and Property Testing

> **Priority**: Medium

### 16.1 Mocking Framework (`lib/test/src/mock.tml`)
- [ ] 16.1.1 Create `lib/test/src/mock.tml`
- [ ] 16.1.2 Design `Mock[T]` type for creating mock objects from behaviors
- [ ] 16.1.3 Implement `when(method).returns(value)`, `when(method).raises(error)`
- [ ] 16.1.4 Implement call counting: `times(n)`, `at_least(n)`, `at_most(n)`, `never()`
- [ ] 16.1.5 Implement `Mock::verify()` — assert all expectations met
- [ ] 16.1.6 Write unit tests for `test::mock`
- [ ] 16.1.7 Export `test::mock` from `lib/test/src/mod.tml`

### 16.2 Property-Based Testing (`lib/test/src/property.tml`)
- [ ] 16.2.1 Create `lib/test/src/property.tml`
- [ ] 16.2.2 Design `Arbitrary[T]` behavior — generate random values of type T
- [ ] 16.2.3 Implement `Arbitrary` for primitives and collections
- [ ] 16.2.4 Implement `prop_test(name, f)` — run with random inputs
- [ ] 16.2.5 Implement shrinking — minimize failing input
- [ ] 16.2.6 Implement configurable iterations and seed-based reproducibility
- [ ] 16.2.7 Write unit tests for `test::property`
- [ ] 16.2.8 Export `test::property` from `lib/test/src/mod.tml`

## Phase 17: Integration and Validation

> **Priority**: High | **Depends on**: All previous phases

- [ ] 17.1.1 Run full test suite with coverage
- [ ] 17.1.2 Verify no regressions in existing tests
- [ ] 17.1.3 Cross-module integration test: URL parsing + encoding
- [ ] 17.1.4 Cross-module integration test: UUID + crypto (for v3/v5)
- [ ] 17.1.5 Cross-module integration test: SQLite + subprocess
- [ ] 17.1.6 Update `lib/core/src/mod.tml` with all new core exports
- [ ] 17.1.7 Update `lib/std/src/mod.tml` with all new std exports
- [ ] 17.1.8 Update `lib/test/src/mod.tml` with all new test exports
- [ ] 17.1.9 Update coverage report
