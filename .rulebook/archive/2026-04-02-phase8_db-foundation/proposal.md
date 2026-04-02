# Proposal: DB Foundation — Core Types + SQLite Adapter

## Why
Create the foundational std::db module with abstract driver behaviors and a concrete SQLite adapter wrapping the existing std::sqlite module. All other DB tasks depend on this.

## What Changes
- Create lib/std/src/db/ with error.tml, value.tml, types.tml
- Define Driver, Connection, PreparedStatement, ResultSet, Transaction behaviors
- Implement SQLite adapter (SqliteDriver, SqliteConnection, SqliteStatement)
- db/mod.tml public API exports

## Impact
- Affected code: lib/std/src/db/ (new), lib/std/tests/db/ (new)
- Breaking change: NO
- User benefit: Database-agnostic abstraction layer with SQLite built-in
