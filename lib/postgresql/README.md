# TML PostgreSQL Driver

PostgreSQL driver for the TML database abstraction layer. Wraps libpq via FFI, implements the `Connection` and `PreparedStatement` behaviors, and provides a PostgreSQL-specific SQL dialect.

[Changelog](CHANGELOG.md)

## Quick Start

```tml
use postgresql::connection::PgConnection

// Connect using keyword=value format
let conn = PgConnection::connect("host=localhost port=5432 dbname=mydb user=myuser password=secret")!

// Or using a URI
let conn = PgConnection::connect("postgresql://myuser:secret@localhost:5432/mydb")!

// Execute DDL or DML
conn.execute("CREATE TABLE users (id SERIAL PRIMARY KEY, name TEXT NOT NULL)")!
conn.execute("INSERT INTO users (name) VALUES ('alice')")!

// Parameterized insert (one I64 parameter)
conn.execute_i64_params("DELETE FROM users WHERE id = $1", 42)!

// Query a scalar count
let count = conn.query_scalar_i64("SELECT COUNT(*) FROM users")!

// Transactions
conn.begin()!
conn.execute("UPDATE users SET name = 'bob' WHERE id = 1")!
conn.commit()!

conn.close()
```

## Module Index

| Module | Path | Description |
|--------|------|-------------|
| connection | `postgresql::connection` | `PgConnection` — `Connection` behavior implementation |
| statement | `postgresql::statement` | `PgStatement` — `PreparedStatement` behavior implementation |
| dialect | `postgresql::dialect` | `PostgresDialect` — `$N` numbered placeholders, double-quote identifiers |
| driver | `postgresql::driver` | `PgDriver` — marker type for the driver registry |
| types | `postgresql::types` | PostgreSQL OID constants and `ColumnType` mapping |
| ffi | `postgresql::ffi` | Raw libpq C function bindings (low-level) |

## API Overview

### PgConnection

`PgConnection` implements the `Connection` behavior from `std::db`.

**Construction:**

| Method | Description |
|--------|-------------|
| `PgConnection::connect(conninfo: Str)` | Connect using a keyword=value or URI connection string |

**Connection behavior methods (14 total):**

| Method | Description |
|--------|-------------|
| `execute(sql)` | Execute a statement, return rows affected |
| `execute_i64_params(sql, p1)` | Parameterized statement with one I64 |
| `execute_i64_2(sql, p1, p2)` | Parameterized statement with two I64s |
| `execute_str(sql, p1)` | Parameterized statement with one Str |
| `execute_str_i64(sql, p1, p2)` | Parameterized statement with Str + I64 |
| `execute_i64_str(sql, p1, p2)` | Parameterized statement with I64 + Str |
| `query_scalar_i64(sql)` | Query a single I64 value (e.g. `COUNT(*)`) |
| `exists_by_i64(sql, p1)` | Check row existence with an I64 parameter |
| `exists_by_str(sql, p1)` | Check row existence with a Str parameter |
| `begin()` | Begin a transaction |
| `commit()` | Commit the current transaction |
| `rollback()` | Roll back the current transaction |
| `is_alive()` | Check whether the connection is open and reachable |
| `close()` | Close the connection and release libpq resources |

**PostgreSQL-specific helpers (not part of the `Connection` behavior):**

| Method | Description |
|--------|-------------|
| `execute_3_i64(sql, p1, p2, p3)` | Three I64 parameters |
| `execute_2str(sql, p1, p2)` | Two Str parameters |
| `execute_2str_i64(sql, p1, p2, p3)` | Two Str + one I64 |
| `query_scalar_str(sql)` | Query a single Str scalar |
| `query_one_i64(sql, p1)` | Query one I64 row with one I64 parameter |
| `query_one_str(sql, p1)` | Query one Str row with one I64 parameter |
| `raw_handle()` | Return the raw libpq `PGconn*` handle |
| `server_version()` | Server version as integer (e.g. 150000 for 15.0) |
| `transaction_status()` | Current transaction status (0=idle, 1=active, ...) |

### PostgresDialect

`PostgresDialect` implements the `Dialect` behavior for use with the ORM query builder.

```tml
use postgresql::dialect::PostgresDialect

let dialect = PostgresDialect::new()
dialect.placeholder(1)         // "$1"
dialect.placeholder(2)         // "$2"
dialect.quote_identifier("id") // "\"id\""
dialect.supports_returning()   // true
```

### Type Mapping

`postgresql::types` provides OID constants for 20+ PostgreSQL built-in types and a `pg_oid_to_column_type` function mapping them to the abstract `ColumnType` enum:

| PostgreSQL type | OID | `ColumnType` |
|----------------|-----|-------------|
| `bool` | 16 | `Boolean` |
| `int2` | 21 | `SmallInt` |
| `int4` | 23 | `Integer` |
| `int8` | 20 | `BigInt` |
| `float4` | 700 | `Float` |
| `float8` | 701 | `Double` |
| `numeric` | 1700 | `Decimal` |
| `text`, `varchar`, `bpchar` | 25, 1043, 1042 | `Text` |
| `bytea` | 17 | `Blob` |
| `date` | 1082 | `Date` |
| `timestamp` | 1114 | `Timestamp` |
| `timestamptz` | 1184 | `TimestampTz` |
| `uuid` | 2950 | `Uuid` |
| `json`, `jsonb` | 114, 3802 | `Json` |

## Build System

`build.tml` is the build script — it runs before compilation and emits `tml:` directives to configure the linker for the correct platform:

```
tml:link-search=native/win-x64      (Windows)
tml:link-lib=libpq
tml:copy-artifact=native/win-x64/libpq.dll

tml:link-search=native/linux-x64    (Linux)
tml:link-lib=pq

tml:link-search=native/macos-arm64  (macOS)
tml:link-lib=pq
```

## Native Dependencies

Pre-built libpq binaries are bundled under `native/`:

| Path | Contents |
|------|----------|
| `native/win-x64/` | `libpq.dll`, `libpq.lib`, `libssl-3-x64.dll`, `libcrypto-3-x64.dll`, `libiconv-2.dll`, `libintl-9.dll` |
| `native/linux-x64/` | `libpq.so`, `libpq.so.5.16` |
| `native/macos-arm64/` | `libpq.dylib`, `libpq.5.dylib`, `libpq.a` |

`scripts/fetch-libpq.sh` downloads updated binaries from the official PostgreSQL distribution.

## Platform Support

| Platform | Architecture | Status |
|----------|-------------|--------|
| Windows | x64 | Supported — DLL bundled |
| Linux | x64 | Supported — shared library bundled |
| macOS | arm64 | Supported — dylib bundled |
| macOS | x64 | Not bundled (build from source) |

## License

TML source code in this package is licensed under the **Apache License 2.0**.

The bundled libpq binaries are licensed under the **PostgreSQL License** (a permissive BSD-like license). On Windows, OpenSSL is bundled alongside libpq and is licensed under the **Apache License 2.0**.

See [LICENSE](LICENSE) and [NOTICE](NOTICE) for full license texts.
