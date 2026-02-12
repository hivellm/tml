# Proposal: Expand Core and Standard Library Modules

## Status: PROPOSED

## Why

TML has a solid foundation with 39 core modules and 21 std modules, but critical gaps remain that prevent real-world application development. Missing modules like math functions, encoding utilities, URL parsing, SQLite access, and subprocess management force developers to work around the language rather than with it. Additionally, the test framework lacks mocking and property-based testing capabilities essential for writing robust test suites.

Key gaps identified through comparison with Rust, Go, Python, and C# standard libraries:

- **Core gaps**: No math functions (sin/cos/sqrt), no encoding (base64/hex), no URL parsing, no SIMD API, no bitset, no ring buffer, no MIME types, no UUID, no semver
- **Std gaps**: No SQLite, no subprocess management, no signal handling, no CLI argument parsing, no template engine, no image I/O, no i18n
- **Test gaps**: No mocking framework, no property-based testing

## What Changes

### New Core Modules (9 modules)

| Module | File(s) | Purpose |
|--------|---------|---------|
| `core::math` | `math.tml` | sin, cos, sqrt, pow, log, exp, constants (PI, E, TAU) |
| `core::encoding` | `encoding/mod.tml` + subfiles | Base64, hex, percent-encoding, Base32, Base58, ASCII85 |
| `core::url` | `url.tml` | URL parsing/building per RFC 3986 |
| `core::simd` | `simd/mod.tml` + subfiles | Portable high-level SIMD API with typed vectors |
| `core::bitset` | `bitset.tml` | Fixed and dynamic bitsets with set operations |
| `core::ringbuf` | `ringbuf.tml` | Fixed-size circular buffer |
| `core::mime` | `mime.tml` | Static MIME type lookup table |
| `core::uuid` | `uuid.tml` | UUID v1-v8 (all versions per RFC 9562), parsing/formatting |
| `core::semver` | `semver.tml` | Semantic versioning parse, compare, ranges |

### New Standard Library Modules (7 modules)

| Module | File(s) | Purpose |
|--------|---------|---------|
| `std::sqlite` | `sqlite.tml` | Embedded SQLite driver (FFI to amalgamation) |
| `std::subprocess` | `subprocess.tml` | Process spawning and management |
| `std::signal` | `signal.tml` | OS signal handling (SIGINT, SIGTERM) |
| `std::cli` | `cli.tml` | CLI argument parsing framework |
| `std::pipe` | `pipe.tml` | Named pipes, shared memory IPC |
| `std::template` | `template.tml` | Simple template engine |
| `std::i18n` | `i18n.tml` | Internationalization, locale-aware formatting |
| `std::image` | `image/mod.tml` + `png.tml` + `jpeg.tml` | Basic PNG/JPEG read/write |

### New Test Framework Modules (2 modules)

| Module | File(s) | Purpose |
|--------|---------|---------|
| `test::mock` | `mock.tml` | Mocking framework for unit tests |
| `test::property` | `property.tml` | Property-based testing (QuickCheck-style) |

## Impact

### New Files

```
lib/core/src/
├── math.tml               # sin, cos, sqrt, pow, log, exp, constants
├── encoding/
│   ├── mod.tml            # Re-exports
│   ├── base64.tml         # Base64 encode/decode (standard + URL-safe)
│   ├── hex.tml            # Hex encode/decode
│   ├── percent.tml        # Percent-encoding (URL encoding)
│   ├── base32.tml         # Base32 encode/decode
│   ├── base58.tml         # Base58 encode/decode (Bitcoin-style)
│   └── ascii85.tml        # ASCII85 encode/decode
├── url.tml                # URL parse/build, query params
├── simd/
│   ├── mod.tml            # Portable SIMD API, re-exports
│   ├── i32x4.tml          # 4xI32 vector
│   ├── f32x4.tml          # 4xF32 vector
│   ├── i64x2.tml          # 2xI64 vector
│   ├── f64x2.tml          # 2xF64 vector
│   ├── i8x16.tml          # 16xI8 vector
│   └── u8x16.tml          # 16xU8 vector
├── bitset.tml             # BitSet[N] fixed-size
├── ringbuf.tml            # RingBuf[T, N] circular buffer
├── mime.tml               # MIME type lookup
├── uuid.tml               # UUID v1-v8
└── semver.tml             # SemVer parse/compare/ranges

lib/std/src/
├── sqlite.tml             # SQLite driver
├── subprocess.tml         # Process spawning
├── signal.tml             # OS signal handling
├── cli.tml                # Argument parsing
├── pipe.tml               # Named pipes, IPC
├── template.tml           # Template engine
├── i18n.tml               # Internationalization
└── image/
    ├── mod.tml            # Image type, load/save
    ├── png.tml            # PNG codec
    └── jpeg.tml           # JPEG codec

lib/test/src/
├── mock.tml               # Mocking framework
└── property.tml           # Property-based testing
```

### Modified Files

- `lib/core/src/mod.tml` - Export 9 new core modules
- `lib/std/src/mod.tml` - Export 7 new std modules
- `lib/test/src/mod.tml` - Export 2 new test modules

### Dependencies Between Modules

| Module | Depends On |
|--------|------------|
| `core::math` | None (compiler intrinsics / libm) |
| `core::encoding` | `core::str`, `core::slice` |
| `core::url` | `core::str`, `core::encoding` (percent-encoding) |
| `core::simd` | None (compiler intrinsics) |
| `core::bitset` | `core::ops`, `core::fmt` |
| `core::ringbuf` | `core::mem`, `core::ops` |
| `core::mime` | `core::str` |
| `core::uuid` | `core::fmt`, `core::encoding::hex`, `core::hash` |
| `core::semver` | `core::str`, `core::cmp`, `core::fmt` |
| `std::sqlite` | `std::file` (FFI to libsqlite3) |
| `std::subprocess` | `std::file` (pipes), OS FFI |
| `std::signal` | OS FFI |
| `std::cli` | `core::str`, `std::collections` |
| `std::pipe` | OS FFI |
| `std::template` | `core::str`, `std::collections` |
| `std::image` | `core::slice`, `std::file`, `std::zlib` (PNG) |
| `std::i18n` | `core::str`, OS locale APIs |
| `test::mock` | `core::any`, `core::reflect` |
| `test::property` | `std::random`, `core::fmt` |

## Success Criteria

1. **Core modules** are pure (no OS dependency, no heap allocation required)
2. **Std modules** handle platform differences transparently
3. **All modules** have comprehensive test suites
4. **UUID** supports all versions including v6, v7, v8 (latest RFC 9562)
5. **Encoding** covers all major encoding schemes
6. **SQLite** is embedded (no external server dependency)
7. **Performance** within 20% of Rust/C equivalents

## Out of Scope

Covered by existing tasks:
- **Collections** (Vec, HashSet, BTreeMap) -> `stdlib-essentials`
- **Networking** (TCP/UDP/HTTP) -> `async-network-stack`
- **Regex** -> `implement-regex-module`
- **Reflection** -> `implement-reflection`
- **Bitflags** -> `add-bitflags-support`
- **Buffered I/O** -> `stdlib-essentials`
- **Path utilities** -> `stdlib-essentials`
- **DateTime** -> `stdlib-essentials`
- **Random** -> `stdlib-essentials`
- **Environment/Process** -> `stdlib-essentials`

Separate tasks (not in scope of this task):
- **Serialization formats** (TOML, YAML, CSV, XML, MessagePack, Protobuf) -> separate dedicated tasks per format
- **Archive/tar** -> extend existing `std::zlib` module when needed

## References

- [RFC 9562 - UUIDs](https://www.rfc-editor.org/rfc/rfc9562) (v1-v8)
- [RFC 3986 - URI Syntax](https://www.rfc-editor.org/rfc/rfc3986)
- [RFC 4648 - Base Encodings](https://www.rfc-editor.org/rfc/rfc4648)
- [SQLite C Interface](https://www.sqlite.org/cintro.html)
- [Semantic Versioning 2.0](https://semver.org/)
- [Rust core::simd](https://doc.rust-lang.org/std/simd/)
- [Hypothesis (Python property testing)](https://hypothesis.readthedocs.io/)
