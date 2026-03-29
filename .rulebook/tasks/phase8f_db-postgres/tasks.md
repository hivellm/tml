# Tasks: Database Library — PostgreSQL Driver (lib/postgresql/)

**Status**: Planning. 0% (0/14).
**Depends on**: phase8_db-foundation, phase8e_db-conditional-compilation

## Phase 1: libpq FFI Bindings

- [ ] 1.1 `lib/postgresql/src/ffi.tml` — PQconnectdb, PQfinish, PQstatus
- [ ] 1.2 PQexec, PQprepare, PQexecPrepared, PQexecParams
- [ ] 1.3 PQntuples, PQnfields, PQgetvalue, PQgetisnull, PQfname, PQftype
- [ ] 1.4 PQclear, PQresultStatus, PQerrorMessage

## Phase 2: Driver Implementation

- [ ] 2.1 `lib/postgresql/src/driver.tml` — PostgresDriver impl Driver
- [ ] 2.2 `lib/postgresql/src/connection.tml` — PgConnection impl Connection
- [ ] 2.3 `lib/postgresql/src/statement.tml` — PgStatement impl PreparedStatement
- [ ] 2.4 `lib/postgresql/src/types.tml` — PostgreSQL OID -> ColumnType
- [ ] 2.5 `lib/postgresql/src/dialect.tml` — PostgresDialect ($N placeholders, RETURNING)
- [ ] 2.6 `lib/postgresql/src/mod.tml` — Exports

## Phase 3: Package Setup

- [ ] 3.1 `lib/postgresql/tml.toml` — Package manifest
- [ ] 3.2 Build instructions for libpq on Windows/Linux/macOS

## Phase 4: Tests + Benchmarks

- [ ] 4.1 Integration tests
- [ ] 4.2 PostgreSQL benchmarks
- [ ] 4.3 SQLite vs PostgreSQL comparison
