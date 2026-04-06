# Proposal: PostgreSQL Driver (lib/postgresql/)

**Task**: phase8f_db-postgres
**Status**: In Progress (86%, 12/14)
**Priority**: P1
**Estimated effort**: 1–2 days
**Risk**: Low

## Problem

TML has a working SQLite driver and the std::db abstraction layer, but no driver for
PostgreSQL — the most widely used production RDBMS. Applications that need ACID
transactions, advanced SQL, JSONB columns, or cloud-hosted databases (Supabase, Neon,
RDS) are currently blocked. The FFI layer and core driver are already implemented; the
remaining gap is Phase 4 integration tests that require libpq binaries to be present on
the CI host.

## Proposed Solution

A standalone `lib/postgresql/` library built on libpq FFI. PgConnection implements the
`std::db::Connection` behavior so existing code that targets the std::db API works with
zero changes. PostgresDialect handles `$N` parameter placeholders and the `RETURNING`
clause. A `build.tml` script locates and links the platform-specific libpq at compile
time (pkg-config on Linux/macOS, Registry lookup on Windows).

## Key Decisions

- **FFI to libpq, not pure TML wire protocol** — libpq handles TLS, SASL auth, and
  protocol negotiation. Re-implementing those in TML would be months of work.
- **$N positional parameters** — matches PostgreSQL's native syntax; avoids server-side
  rewriting of ? placeholders.
- **RETURNING clause support** — INSERT/UPDATE/DELETE can return rows without a second
  SELECT round-trip.
- **20+ OID type mappings** — Bool, Int2/4/8, Float4/8, Text, Bytea, Uuid, Timestamp,
  Numeric, Json/Jsonb, Array, and custom types via `pg_type` lookup.
- **Streaming rows via Cursor** — avoid materializing large result sets in memory.

## Files to Create/Modify

- `lib/postgresql/src/ffi.tml` — raw libpq `@extern("c")` declarations
- `lib/postgresql/src/driver.tml` — PostgresDriver implementing Driver behavior
- `lib/postgresql/src/connection.tml` — PgConnection implementing Connection behavior
- `lib/postgresql/src/statement.tml` — PgStatement with parameter binding
- `lib/postgresql/src/types.tml` — OID type mappings and DbValue conversions
- `lib/postgresql/src/dialect.tml` — PostgresDialect ($N, RETURNING, quoting)
- `lib/postgresql/src/mod.tml` — public re-exports
- `lib/postgresql/build.tml` — platform-specific libpq discovery and linking
- `lib/postgresql/package.toml` — package metadata and native-lib declaration

## Success Criteria

- All 14 checklist items marked done
- Phase 4 integration tests pass against a live PostgreSQL instance
- `pg_connect(url)?.query("SELECT $1::text", ["hello"])` returns correct row
- INSERT with RETURNING yields the inserted row without extra round-trip
- Prepared statement cache re-uses server-side statements across calls
- No memory leaks under Valgrind / tml debug --check-leaks

## Dependencies

- **Depends on**: std::db abstraction (phase8d, complete), libpq headers and binaries
  available in build environment
- **Blocks**: any application layer that needs PostgreSQL (HTTP CRUD demos, ORM layer)
