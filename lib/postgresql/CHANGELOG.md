# Changelog — TML PostgreSQL Driver (`lib/postgresql`)

All notable changes to the PostgreSQL driver will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] — 2026-04-04

### Added

- `PgConnection` implementing the `Connection` behavior (14 methods)
  - `execute`, `begin`, `commit`, `rollback`, `is_alive`, `close`
  - Parameterized variants: `execute_i64_params`, `execute_i64_2`, `execute_str`, `execute_str_i64`, `execute_i64_str`
  - Scalar queries: `query_scalar_i64`, `exists_by_i64`, `exists_by_str`
- PostgreSQL-specific helpers on `PgConnection` (not part of the behavior)
  - `execute_3_i64`, `execute_2str`, `execute_2str_i64`
  - `query_scalar_str`, `query_one_i64`, `query_one_str`
  - `raw_handle`, `server_version`, `transaction_status`
- `PgStatement` implementing the `PreparedStatement` behavior
- `PostgresDialect` implementing the `Dialect` behavior
  - `$N` numbered parameter placeholders
  - Double-quote identifier quoting
  - `supports_returning()` returns `true`
- `PgDriver` marker type for the driver registry
- PostgreSQL OID constants and `pg_oid_to_column_type` mapping for 20+ built-in types
- `build.tml` for Rust-style native library resolution via `tml:` directives
- Pre-built libpq binaries for Windows x64, Linux x64, macOS arm64
- `scripts/fetch-libpq.sh` for updating native binaries from the official PostgreSQL distribution
