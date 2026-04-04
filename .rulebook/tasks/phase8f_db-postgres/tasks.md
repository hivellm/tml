# Tasks: Database Library — PostgreSQL Driver (lib/postgresql/)

**Status**: In Progress. 12/14 (86%). Phases 1-3 complete. Phase 4 (tests) pending libpq binaries.
**Depends on**: phase8_db-foundation, phase10_build-tml-package-system

## Phase 1: libpq FFI Bindings

- [x] 1.1 `lib/postgresql/src/ffi.tml` — PQconnectdb, PQfinish, PQstatus
- [x] 1.2 PQexec, PQprepare, PQexecPrepared, PQexecParams
- [x] 1.3 PQntuples, PQnfields, PQgetvalue, PQgetisnull, PQfname, PQftype
- [x] 1.4 PQclear, PQresultStatus, PQerrorMessage, PQcmdTuples, PQtransactionStatus

## Phase 2: Driver Implementation

- [x] 2.1 `lib/postgresql/src/driver.tml` — PgDriver marker type
- [x] 2.2 `lib/postgresql/src/connection.tml` — PgConnection impl Connection (14 methods + helpers)
- [x] 2.3 `lib/postgresql/src/statement.tml` — PgStatement impl PreparedStatement (bind/run/step/column)
- [x] 2.4 `lib/postgresql/src/types.tml` — PostgreSQL OID → ColumnType (20+ OIDs mapped)
- [x] 2.5 `lib/postgresql/src/dialect.tml` — PostgresDialect ($N placeholders, RETURNING)
- [x] 2.6 `lib/postgresql/src/mod.tml` — Re-exports all 6 submodules

## Phase 3: Package Setup

- [x] 3.1 `lib/postgresql/package.toml` — Package manifest with native-deps
- [x] 3.2 `lib/postgresql/build.tml` — Rust-style build script for platform-specific libpq linking

## Phase 4: Tests + Benchmarks (requires libpq binaries in native/)

- [ ] 4.1 Integration tests (needs libpq.lib/dll in native/win-x64/)
- [ ] 4.2 PostgreSQL benchmarks
