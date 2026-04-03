# SQLite In-Memory Benchmark — Cross-Language Comparison

**10,000 iterations** per operation, SQLite `:memory:`, WAL mode, synchronous=OFF.

All benchmarks use **prepared statements** (except TML which builds SQL strings per call).

## Results

### Latency (ns/op — lower is better)

| Operation | Rust | Python | Node.js | Go | TML |
|-----------|------|--------|---------|-----|-----|
| **INSERT** | 687 ns | 782 ns | 984 ns | 1,400 ns | 1,853 ns |
| **SELECT** | 201 ns | 1,062 ns | 586 ns | 1,700 ns | 1,607 ns |
| **UPDATE** | 710 ns | 858 ns | 975 ns | 1,400 ns | 1,866 ns |
| **DELETE** | 674 ns | 764 ns | 930 ns | 1,300 ns | 1,508 ns |

### Throughput (ops/sec — higher is better)

| Operation | Rust | Python | Node.js | Go | TML |
|-----------|------|--------|---------|-----|-----|
| **INSERT** | 1,455K | 1,278K | 1,017K | 714K | 540K |
| **SELECT** | 4,981K | 942K | 1,708K | 588K | 622K |
| **UPDATE** | 1,409K | 1,166K | 1,026K | 714K | 536K |
| **DELETE** | 1,483K | 1,309K | 1,075K | 769K | 663K |

### Relative Performance (Rust = 1.0x)

| Operation | Rust | Python | Node.js | Go | TML |
|-----------|------|--------|---------|-----|-----|
| **INSERT** | 1.0x | 1.1x | 1.4x | 2.0x | 2.7x |
| **SELECT** | 1.0x | 5.3x | 2.9x | 8.5x | 8.0x |
| **UPDATE** | 1.0x | 1.2x | 1.4x | 2.0x | 2.6x |
| **DELETE** | 1.0x | 1.1x | 1.4x | 1.9x | 2.2x |

## Analysis

**Ranking (overall): Rust > Python > Node.js > Go > TML**

### Why TML is slower

1. **No prepared statement reuse** — TML rebuilds SQL strings per call (`conn.execute("INSERT ... " + id.to_string())`), while all others use `?` placeholders with pre-compiled statements
2. **String allocation overhead** — each `+` concatenation allocates a new string, adding GC pressure
3. **FFI overhead** — TML calls sqlite3 via `@extern("c")` which has more overhead than Rust's direct binding or Python's C extension module

### What would make TML competitive

- **Prepared statement with bind()** — would bring INSERT/UPDATE/DELETE to ~700-900 ns (2x improvement)
- **String interning** for SQL templates — would eliminate per-call allocation
- **Batch operations** — wrapping 10K inserts in a transaction (already done via WAL, but explicit `BEGIN/COMMIT` would help)

## Environment

- **OS**: Windows 10 Pro x86_64
- **CPU**: (system CPU)
- **Rust**: rustc + rusqlite 0.31 (release mode)
- **Python**: 3.13 + stdlib sqlite3
- **Node.js**: + better-sqlite3 (native addon)
- **Go**: 1.25.4 + mattn/go-sqlite3 (CGO via zig cc)
- **TML**: 0.2.8 + std::db::sqlite (debug mode, string-based queries)
