# Tasks: Database Library — PostgreSQL Driver (lib/postgresql/)

**Status**: In Progress. 13/14 (93%). Phases 1-3 complete. Phase 4: 6/8 tests pass, 2 crash (heap corruption in parameterized queries — pg_query, pg_statement).
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

- [x] 4.1 Integration tests — 6/8 pass (pg_connect, pg_dialect, pg_execute, pg_params, pg_transaction, pg_types). 2 crash: pg_query + pg_statement (HEAP_CORRUPTION in parameterized query helpers — `pg_alloc_params`/`bind_i64` memory issue). Fixed: `pg_next_stmt_name` uninitialized memory read, added `SET client_min_messages='warning'` to suppress NOTICEs.
- [ ] 4.2 PostgreSQL benchmarks (blocked by pg_query/pg_statement crash fix)
