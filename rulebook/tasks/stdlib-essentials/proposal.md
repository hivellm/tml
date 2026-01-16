# Proposal: Standard Library Essentials

## Status: PROPOSED

## Why

TML has a mature compiler but lacks essential standard library utilities that developers expect from a production-ready language. Without these fundamentals, even simple programs require significant boilerplate or external dependencies.

Key gaps:
- **Collections**: No `HashSet`, `BTreeMap`, ordered collections
- **Environment**: No way to read environment variables or command-line arguments
- **Process**: Cannot spawn subprocesses or pipe I/O
- **Buffered I/O**: File operations are unbuffered, hurting performance
- **Path**: No cross-platform path manipulation
- **DateTime**: Only basic `Duration`, no calendar/time-of-day support
- **Random**: No random number generation

## What Changes

### New Modules

| Module | Purpose |
|--------|---------|
| `std::collections` | `Vec[T]`, `HashSet[T]`, `BTreeMap[K,V]`, `BTreeSet[T]`, `Deque[T]` |
| `std::env` | Environment variables, current directory |
| `std::args` | Command-line argument parsing |
| `std::process` | Subprocess spawning with pipes |
| `std::io` | `BufReader`, `BufWriter`, `LineWriter` |
| `std::path` | `Path`, `PathBuf` with cross-platform support |
| `std::time` | `Instant`, `SystemTime`, `DateTime` |
| `std::random` | `Rng` trait, `ThreadRng`, distributions |

### Design Principles

| Principle | Implementation |
|-----------|----------------|
| Zero-cost abstractions | Collections use same layout as primitives |
| Platform abstraction | Path separators, env vars work everywhere |
| Composable | BufReader wraps any `Read` implementor |
| Predictable | No hidden allocations, explicit capacity |
| Familiar API | Mirrors Rust std where sensible |

## Impact

### New Files

```
lib/std/src/
├── collections/
│   ├── mod.tml
│   ├── vec.tml
│   ├── hashset.tml
│   ├── btreemap.tml
│   ├── btreeset.tml
│   └── deque.tml
├── env/
│   └── mod.tml
├── args/
│   └── mod.tml
├── process/
│   └── mod.tml
├── io/
│   ├── bufreader.tml
│   ├── bufwriter.tml
│   └── linewriter.tml
├── path/
│   └── mod.tml
├── time/
│   ├── mod.tml
│   ├── instant.tml
│   ├── systemtime.tml
│   └── datetime.tml
└── random/
    └── mod.tml
```

### Modified Files

- `lib/std/src/mod.tml` - Export new modules

### Platform Requirements

- **Windows**: `GetEnvironmentVariable`, `CreateProcess`, `QueryPerformanceCounter`
- **Unix**: `getenv`, `fork/exec`, `clock_gettime`
- **Both**: File system operations for path resolution

## Success Criteria

1. **Functionality**
   - [ ] All collection types pass comprehensive tests
   - [ ] Environment/args work on Windows and Linux
   - [ ] Process spawning handles stdout/stderr/stdin
   - [ ] Buffered I/O improves throughput by 5x+
   - [ ] Path operations handle all edge cases
   - [ ] DateTime formats ISO 8601 correctly
   - [ ] Random passes statistical uniformity tests

2. **Performance**
   - [ ] HashSet insert/lookup O(1) amortized
   - [ ] BTreeMap operations O(log n)
   - [ ] BufReader reduces syscalls by 100x
   - [ ] Memory usage comparable to Rust equivalents

3. **Ergonomics**
   - [ ] Simple use cases require minimal code
   - [ ] Error messages are actionable
   - [ ] Documentation with examples for all types
   - [ ] Consistent API across modules

## Out of Scope

The following are covered by separate tasks:

- **Networking** (TCP/UDP/HTTP) → `add-network-stdlib`
- **Threading/Concurrency** → `thread-safe-native`
- **JSON** → `json-native-implementation`

## References

- [Rust std::collections](https://doc.rust-lang.org/std/collections/)
- [Rust std::env](https://doc.rust-lang.org/std/env/)
- [Rust std::process](https://doc.rust-lang.org/std/process/)
- [Rust std::time](https://doc.rust-lang.org/std/time/)
- [Go time package](https://pkg.go.dev/time)
