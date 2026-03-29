# Tasks: Database Library — Foundation (Core Types + SQLite Adapter)

**Status**: Planning. 0% (0/20).
**Reference**: docs/analyses/db/00-strategic-plan.md

## Phase 1: Core Error & Value Types

- [ ] 1.1 Create `lib/std/src/db/` directory structure
- [ ] 1.2 `db/error.tml` — DbErrorKind enum (Connection, Query, Bind, Transaction, Migration, Driver, Pool, Schema, Timeout)
- [ ] 1.3 `db/error.tml` — DbError type with kind, message, source fields
- [ ] 1.4 `db/error.tml` — impl Display + Error behaviors for DbError
- [ ] 1.5 `db/error.tml` — DbResult[T] type alias = Outcome[T, DbError]
- [ ] 1.6 `db/value.tml` — DbValue enum (Null, Bool, I32, I64, F64, Text, Bytes, Timestamp)
- [ ] 1.7 `db/value.tml` — Accessor methods (as_i64, as_str, as_f64, is_null, etc.)
- [ ] 1.8 `db/value.tml` — impl Display for DbValue
- [ ] 1.9 `db/types.tml` — ColumnType enum (Integer, Float, Text, Blob, Boolean, Timestamp, Uuid)
- [ ] 1.10 `db/types.tml` — ColumnInfo { name, type, nullable, primary_key, default_value }
- [ ] 1.11 `db/types.tml` — TableInfo { name, columns, primary_keys, foreign_keys }

## Phase 2: Driver Behaviors (Traits)

- [ ] 2.1 `db/driver/connection.tml` — Connection behavior (execute, query, prepare, begin, close, is_alive)
- [ ] 2.2 `db/driver/statement.tml` — PreparedStatement behavior (bind, execute, query, reset, close)
- [ ] 2.3 `db/driver/result.tml` — ResultSet type + Row type with column access
- [ ] 2.4 `db/driver/transaction.tml` — Transaction behavior (commit, rollback, savepoint, release)
- [ ] 2.5 `db/driver/mod.tml` — Driver behavior definition + exports

## Phase 3: SQLite Driver Adapter

- [ ] 3.1 `db/sqlite/driver.tml` — SqliteDriver type + impl Driver
- [ ] 3.2 `db/sqlite/connection.tml` — SqliteConnection wrapping std::sqlite::Database, impl Connection
- [ ] 3.3 `db/sqlite/statement.tml` — SqliteStatement wrapping std::sqlite::Statement, impl PreparedStatement
- [ ] 3.4 `db/sqlite/types.tml` — SQLite ColumnType mapping
- [ ] 3.5 `db/sqlite/mod.tml` — SQLite driver exports

## Phase 4: Module Root + Tests

- [ ] 4.1 `db/mod.tml` — Public API exports
- [ ] 4.2 Tests: connection open/close, execute, query
- [ ] 4.3 Tests: prepared statements, binding, results
- [ ] 4.4 Tests: begin, commit, rollback, savepoint
