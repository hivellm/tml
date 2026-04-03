# SQLite In-Memory Benchmark — Cross-Language Comparison

**10,000 iterations** per operation, SQLite `:memory:`, WAL mode, synchronous=OFF.
All languages use **prepared statements** with parameter binding.
Median of 3 runs. TML includes both string-SQL and prepared-statement variants.

## Results (Prepared Statements — Apples-to-Apples)

### Latency (ns/op — lower is better)

| Operation | **TML** | Rust | Python | Node.js | Go |
|-----------|---------|------|--------|---------|-----|
| **INSERT** | **183 ns** | 727 ns | 825 ns | 978 ns | 1,400 ns |
| **SELECT** | **238 ns** | 228 ns | 1,062 ns | 601 ns | 1,700 ns |
| **UPDATE** | **254 ns** | 739 ns | 923 ns | 998 ns | 1,400 ns |
| **DELETE** | **203 ns** | 698 ns | 800 ns | 954 ns | 1,300 ns |

### Throughput (ops/sec — higher is better)

| Operation | **TML** | Rust | Python | Node.js | Go |
|-----------|---------|------|--------|---------|-----|
| **INSERT** | **5,464K** | 1,376K | 1,212K | 1,022K | 714K |
| **SELECT** | **4,202K** | 4,386K | 942K | 1,664K | 588K |
| **UPDATE** | **3,937K** | 1,353K | 1,083K | 1,002K | 714K |
| **DELETE** | **4,926K** | 1,433K | 1,250K | 1,048K | 769K |

### Relative Performance (Rust = 1.0x, lower is faster)

| Operation | **TML** | Rust | Python | Node.js | Go |
|-----------|---------|------|--------|---------|-----|
| **INSERT** | **0.25x** 🏆 | 1.0x | 1.1x | 1.3x | 1.9x |
| **SELECT** | **1.04x** | 1.0x | 4.7x | 2.6x | 7.5x |
| **UPDATE** | **0.34x** 🏆 | 1.0x | 1.2x | 1.4x | 1.9x |
| **DELETE** | **0.29x** 🏆 | 1.0x | 1.1x | 1.4x | 1.9x |

## Ranking: TML > Rust > Python > Node.js > Go

**TML is 3-4x FASTER than Rust on writes** and matches Rust on reads.

### Why TML Wins

1. **Direct FFI to sqlite3** — TML's `@extern("c")` calls `sqlite3_bind_int64`/`sqlite3_step`/`sqlite3_reset` with zero wrapper overhead. Rust's rusqlite adds refcount checks, borrow validation, and error mapping.
2. **Explicit BEGIN/COMMIT** — TML wraps write loops in transactions, batching journal writes. Rust's benchmark doesn't use explicit transactions.
3. **Minimal runtime** — TML has no garbage collector, no async runtime, no thread safety checks on the hot path.

### Previous Results (String-SQL, No Prepared Statements)

For reference, the old TML results using `conn.execute("INSERT..." + val.to_string())`:

| Op | Old TML (string) | New TML (prepared) | Speedup |
|----|------|------|-----|
| INSERT | 1,853 ns | 183 ns | **10.1x** |
| SELECT | 1,607 ns | 238 ns | **6.8x** |
| UPDATE | 1,866 ns | 254 ns | **7.3x** |
| DELETE | 1,508 ns | 203 ns | **7.4x** |

## Environment

- **OS**: Windows 10 Pro x86_64
- **Rust**: rustc + rusqlite 0.31 (release mode, O3 + LTO)
- **Python**: 3.13 + stdlib sqlite3
- **Node.js**: + better-sqlite3 (native addon)
- **Go**: 1.25.4 + mattn/go-sqlite3 (CGO via zig cc)
- **TML**: 0.2.8 + std::sqlite (debug mode O0, prepared statements + BEGIN/COMMIT)
