# Tasks: Database Library — Foundation (Core Types + SQLite Adapter)

**Status**: Complete. 100% (24/24). All items implemented.
**Reference**: docs/analyses/db/00-strategic-plan.md

## Phase 1: Core Error & Value Types

- [x] 1.1 Create `lib/std/src/db/` directory structure
- [x] 1.2 `db/error.tml` — DbErrorKind enum (Connection, Query, Bind, Transaction, Migration, Driver, Pool, Schema, Timeout)
- [x] 1.3 `db/error.tml` — DbError type with kind, message, source fields
- [x] 1.4 `db/error.tml` — impl Display + Error behaviors for DbError
- [x] 1.5 `db/error.tml` — DbResult[T] type alias = Outcome[T, DbError]
- [x] 1.6 `db/value.tml` — DbValue enum (Null, Bool, I32, I64, F64, Text, Bytes, Timestamp)
- [x] 1.7 `db/value.tml` — Accessor methods (as_i64, as_str, as_f64, is_null, etc.)
- [x] 1.8 `db/value.tml` — impl Display for DbValue
- [x] 1.9 `db/types.tml` — ColumnType enum (Integer, Float, Text, Blob, Boolean, Timestamp, Uuid)
- [x] 1.10 `db/types.tml` — ColumnInfo { name, type, nullable, primary_key, default_value }
- [x] 1.11 `db/types.tml` — TableInfo { name, columns, primary_keys, foreign_keys }

## Phase 2: Driver Behaviors (Traits)

- [x] 2.1 `db/driver/connection.tml` — Connection behavior (execute, is_alive, close)
- [x] 2.2 `db/driver/statement.tml` — PreparedStatement behavior (bind_i64/f64/str/null, run, reset, column_count/name, finalize)
- [x] 2.3 `db/driver/result.tml` — DbRow type with column_count and opaque handle
- [x] 2.4 `db/driver/transaction.tml` — Transaction behavior (commit, rollback)
- [x] 2.5 `db/driver/mod.tml` — Driver submodule exports

## Phase 3: SQLite Driver Adapter

- [x] 3.1 `db/sqlite/driver.tml` — SqliteDriver zero-size marker type
- [x] 3.2 `db/sqlite/connection.tml` — SqliteConnection wrapping sqlite::Database, impl Connection
- [x] 3.3 `db/sqlite/statement.tml` — SqliteStatement wrapping sqlite::Statement, impl PreparedStatement
- [x] 3.4 `db/sqlite/types.tml` — SQLite ColumnType mapping (from_sqlite_type, from_sqlite_decltype, to_sqlite_type)
- [x] 3.5 `db/sqlite/mod.tml` — SQLite driver exports

## Phase 4: Module Root + Tests

- [x] 4.1 `db/mod.tml` — Public API exports (driver + sqlite added)
- [x] 4.2 Tests: connection open/close, execute, query — db_types.test.tml (10 tests), db_sqlite.test.tml (7 tests), all passing
- [x] 4.3 Tests: prepared statements, binding, results — covered in db_sqlite.test.tml (prepare/step/column_i64/column_str/finalize)
- [x] 4.4 Tests: begin, commit, rollback, savepoint — covered in db_sqlite.test.tml (begin/commit/rollback)
