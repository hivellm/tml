# Proposal: PostgreSQL Driver (lib/postgresql/)

## Why
Most requested SQL database driver. Separate library implementing std::db behaviors via libpq FFI.

## What Changes
- lib/postgresql/ with FFI bindings to libpq
- PostgresDriver, PgConnection, PgStatement implementing Driver behaviors
- PostgresDialect ($N placeholders, RETURNING support)
- Integration tests and benchmarks

## Impact
- Affected code: lib/postgresql/ (new separate library)
- Breaking change: NO
- User benefit: PostgreSQL support via familiar std::db API
