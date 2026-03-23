# TML Samples

Runnable examples covering every major feature of the TML language, from basic syntax to advanced networking. Each sample is a standalone `.tml` file you can compile and run.

```bash
# Run any sample
tml run samples/01-basics/hello-world.tml

# Build without running
tml build samples/01-basics/hello-world.tml
```

---

## 01-basics/ --- Language Fundamentals

| Sample | Description |
|--------|-------------|
| [hello-world.tml](01-basics/hello-world.tml) | Entry point, `println`, basic program structure |
| [variables.tml](01-basics/variables.tml) | `let`, `var`, `const`, type annotations, numeric types |
| [control-flow.tml](01-basics/control-flow.tml) | `if`/`else`, `when` (pattern matching), `loop`, `for`..`in` ranges |
| [functions.tml](01-basics/functions.tml) | Parameters, return types, recursion, boolean functions |
| [structs.tml](01-basics/structs.tml) | Struct definition, fields, methods, `impl` blocks |

## 02-types/ --- Type System

| Sample | Description |
|--------|-------------|
| [enums.tml](02-types/enums.tml) | Enum variants, data payloads, `when` pattern matching, `Maybe[T]` |
| [generics.tml](02-types/generics.tml) | Generic structs `Pair[T]`, `KeyValue[K,V]`, generic return types |
| [error-handling.tml](02-types/error-handling.tml) | `Maybe[T]`, `Outcome[T,E]`, `Ok`/`Err`, `Just`/`Nothing`, `is_ok`/`is_err` |
| [arrays-slices.tml](02-types/arrays-slices.tml) | Fixed-size arrays, iteration, mutable arrays, fill, min/max |
| [cell.tml](02-types/cell.tml) | Interior mutability with `Cell[T]` (get, set, replace) and `RefCell[T]` |

## 03-functional/ --- Functional Patterns

| Sample | Description |
|--------|-------------|
| [closures.tml](03-functional/closures.tml) | `do(x) expr` syntax, higher-order functions, captures |
| [iterators.tml](03-functional/iterators.tml) | `for`..`in` ranges, `to`/`through`, nested loops, `break`/`continue` |
| [formatting.tml](03-functional/formatting.tml) | `to_string()`, string building, numeric formatting, table output, parsing |

## 04-collections/ --- Data Structures

| Sample | Description |
|--------|-------------|
| [list-hashmap.tml](04-collections/list-hashmap.tml) | `List[T]` (push, pop, get, set), `HashMap[K,V]` (set, get, has, remove) |
| [buffer.tml](04-collections/buffer.tml) | `Buffer` byte operations: write_u8, read_u8, write_i32, from_string, slice |

## 05-strings/ --- String Manipulation

| Sample | Description |
|--------|-------------|
| [string-ops.tml](05-strings/string-ops.tml) | `len`, `trim`, `contains`, `find`, `split`, `join`, `replace`, `repeat`, case conversion |
| [encoding.tml](05-strings/encoding.tml) | Base64 encode/decode, URL-safe Base64, hex encode/decode, round-trip verification |

## 06-io/ --- Input/Output

| Sample | Description |
|--------|-------------|
| [json.tml](06-io/json.tml) | JSON parsing, field access, arrays, constructors, error handling |
| [file-io.tml](06-io/file-io.tml) | `File::write_all`, `File::read_all`, line-by-line reading, append |
| [regex.tml](06-io/regex.tml) | Regex matching, character classes, quantifiers, anchors, replace/replace_all |
| [glob.tml](06-io/glob.tml) | Glob pattern matching: wildcards `*`, `?`, character classes `[abc]` |

## 07-stdlib/ --- Standard Library

| Sample | Description |
|--------|-------------|
| [math.tml](07-stdlib/math.tml) | Constants (PI, E, TAU), `sqrt`, `pow`, `sin`/`cos`, `floor`/`ceil`, `min`/`max` |
| [datetime.tml](07-stdlib/datetime.tml) | `Duration` (from_secs, from_millis), `Instant::now()`, elapsed time measurement |
| [random.tml](07-stdlib/random.tml) | PRNG API reference (xoshiro256**), simple LCG demo, coin flip simulation |
| [os.tml](07-stdlib/os.tml) | System info (arch, platform, CPU, memory), user info, uptime, cwd, args |
| [hash.tml](07-stdlib/hash.tml) | FNV-1a (32/64-bit), Murmur2 (32/64-bit), hex output, ETag generation |
| [uuid.tml](07-stdlib/uuid.tml) | UUID v4 (random), v7 (timestamp), parsing, format validation |
| [url.tml](07-stdlib/url.tml) | URL parsing (scheme, host, port, path, query, fragment), error handling |
| [log.tml](07-stdlib/log.tml) | Structured logging: levels, filtering, module tags, compact/JSON output |
| [cmp.tml](07-stdlib/cmp.tml) | Comparison operators, min/max, clamp pattern, bubble sort, Ordering |

## 08-concurrency/ --- Threads and Synchronization

| Sample | Description |
|--------|-------------|
| [threads.tml](08-concurrency/threads.tml) | Current thread ID, `sleep_ms`, `yield_now`, `available_parallelism` |
| [channels.tml](08-concurrency/channels.tml) | MPSC channel API reference: `Sender[T]`, `Receiver[T]`, send/recv patterns |
| [mutex.tml](08-concurrency/mutex.tml) | `Mutex[T]` creation, lock/unlock, `MutexGuard[T]`, thread safety patterns |

## 09-networking/ --- Network Programming

| Sample | Description |
|--------|-------------|
| [http-server.tml](09-networking/http-server.tml) | Full HTTP server with routing, handlers, and configuration |

## 10-advanced/ --- Advanced Features

| Sample | Description |
|--------|-------------|
| [smart-pointers.tml](10-advanced/smart-pointers.tml) | `Heap[T]` (new, get, set, into_raw, from_raw), `Shared[T]` (refcounting) |
| [crypto.tml](10-advanced/crypto.tml) | SHA-256, SHA-512, SHA-1, MD5 hash functions with hex output |
| [compression.tml](10-advanced/compression.tml) | Deflate/inflate, gzip/gunzip round-trips, compression ratio demo |
| [sqlite.tml](10-advanced/sqlite.tml) | In-memory SQLite: create table, insert, query, parameters, transactions, aggregates |
| [search.tml](10-advanced/search.tml) | BM25 full-text search: index documents, search with ranking, IDF scores |
| [streams.tml](10-advanced/streams.tml) | `ByteStream` read/write, from_string/to_string, stream behavior API |

---

## Running All Samples

```bash
# Run a specific category
for f in samples/01-basics/*.tml; do echo "=== $f ===" && tml run "$f"; done

# Run all samples (excluding http-server which starts a listener)
find samples -name "*.tml" ! -path "*/http-server/*" ! -name "http-server.tml" -exec sh -c 'echo "=== {} ===" && tml run "{}"' \;
```

## Prerequisites

Most samples only need the TML compiler. Some advanced samples require:

| Sample | Requires |
|--------|----------|
| crypto.tml | OpenSSL 3.0+ or Windows BCrypt |
| compression.tml | zlib (bundled with compiler) |
| sqlite.tml | SQLite3 (bundled with compiler) |
| hash.tml | Crypto runtime (bundled with compiler) |
| http-server.tml | Network access (listens on port 3000) |

## Known Limitations

Some standard library modules have codegen limitations in standalone programs:

| Module | Issue |
|--------|-------|
| `std::random::Rng` | Integer overflow in xoshiro256** multiply (checked arithmetic) |
| `std::sync::mpsc` | Generic `send()`/`recv()` require test suite monomorphization |
| `core::fmt::helpers` | Cross-module hex/binary/octal formatters return `()` |
| `std::os::env_get` | `Maybe[Str]` return type codegen issue in standalone programs |
