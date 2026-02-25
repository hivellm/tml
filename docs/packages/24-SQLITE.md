# std::sqlite — SQLite Embedded Database

> **Status**: IMPLEMENTED (2026-02-24)

## 1. Overview

The `std::sqlite` module provides a safe TML interface to SQLite3 via FFI bindings, inspired by Node.js `node:sqlite`. Supports file-based and in-memory databases, prepared statements with typed parameters, transactions, and typed column access.

```tml
use std::sqlite::{Database, Statement, Row, Value}
```

**Dependency**: Requires `sqlite3` library (installed via vcpkg).

## 2. Module Structure

| File | Description |
|------|-------------|
| `mod.tml` | Module re-exports |
| `ffi.tml` | Raw `@extern("c")` bindings to sqlite3 C API |
| `constants.tml` | SQLite constants (SQLITE_OK, SQLITE_ROW, type codes, open flags) |
| `database.tml` | `Database` — connection, exec, prepare, transactions |
| `statement.tml` | `Statement` — prepared statements, bind, step, column access |
| `row.tml` | `Row` — single result row with typed getters |
| `value.tml` | `Value` — dynamic SQLite value type |

## 3. Database

```tml
pub type Database {
    handle: *Unit,
    is_open: Bool
}

impl Database {
    /// Open a database file (read-write, create if not exists).
    pub func open(path: Str) -> Outcome[Database, Str]

    /// Open with explicit flags (SQLITE_OPEN_READONLY, etc.).
    pub func open_with_flags(path: Str, flags: I32) -> Outcome[Database, Str]

    /// Open a read-only database.
    pub func open_readonly(path: Str) -> Outcome[Database, Str]

    /// Open an in-memory database.
    pub func open_in_memory() -> Outcome[Database, Str]

    /// Execute SQL that doesn't return rows.
    pub func exec(this, sql: Str) -> Outcome[Unit, Str]

    /// Prepare a SQL statement for execution.
    pub func prepare(this, sql: Str) -> Outcome[Statement, Str]

    /// Close the database connection.
    pub func close(mut this)

    /// Number of rows changed by last INSERT/UPDATE/DELETE.
    pub func changes(this) -> I64

    /// Total rows changed since connection opened.
    pub func total_changes(this) -> I64

    /// Row ID of last successful INSERT.
    pub func last_insert_rowid(this) -> I64

    /// Begin a transaction.
    pub func begin(this) -> Outcome[Unit, Str]

    /// Commit current transaction.
    pub func commit(this) -> Outcome[Unit, Str]

    /// Rollback current transaction.
    pub func rollback(this) -> Outcome[Unit, Str]
}
```

### Quick Start

```tml
use std::sqlite::database::Database

let db = Database::open_in_memory().unwrap()
db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
db.exec("INSERT INTO users (name, age) VALUES ('Alice', 30)")
db.exec("INSERT INTO users (name, age) VALUES ('Bob', 25)")

let stmt = db.prepare("SELECT id, name, age FROM users ORDER BY id").unwrap()
loop (stmt.step()) {
    print("{}: {} (age {})\n", stmt.column_i64(0), stmt.column_str(1), stmt.column_i64(2))
}
stmt.finalize()
db.close()
```

## 4. Statement

```tml
pub type Statement {
    handle: *Unit,
    db: *Unit,
    column_count: I32,
    has_row: Bool
}

impl Statement {
    // ── Execution ─────────────────────────────────────

    /// Step to next row. Returns true if row available, false when done.
    pub func step(mut this) -> Bool

    /// Execute and discard results (for INSERT/UPDATE/DELETE).
    pub func run(mut this) -> Outcome[Unit, Str]

    /// Reset statement for re-execution with new parameters.
    pub func reset(mut this) -> Outcome[Unit, Str]

    /// Release statement resources.
    pub func finalize(mut this)

    // ── Parameter Binding (1-based index) ──────────────

    pub func bind_i64(this, idx: I32, val: I64) -> Outcome[Unit, Str]
    pub func bind_f64(this, idx: I32, val: F64) -> Outcome[Unit, Str]
    pub func bind_str(this, idx: I32, val: Str) -> Outcome[Unit, Str]
    pub func bind_null(this, idx: I32) -> Outcome[Unit, Str]
    pub func bind_value(this, idx: I32, val: Value) -> Outcome[Unit, Str]

    /// Number of bound parameters in the SQL.
    pub func bind_parameter_count(this) -> I32

    /// Name of the parameter at index (e.g., ":name").
    pub func bind_parameter_name(this, idx: I32) -> Str

    /// Index of a named parameter.
    pub func bind_parameter_index(this, name: Str) -> I32

    // ── Column Access (0-based index) ──────────────────

    pub func column_i64(this, idx: I32) -> I64
    pub func column_f64(this, idx: I32) -> F64
    pub func column_str(this, idx: I32) -> Str
    pub func column_type(this, idx: I32) -> I32
    pub func column_name(this, idx: I32) -> Str
    pub func column_decltype(this, idx: I32) -> Str
    pub func column_count(this) -> I32
    pub func data_count(this) -> I32
    pub func column_value(this, idx: I32) -> Value
}
```

### Prepared Statements Example

```tml
let insert = db.prepare("INSERT INTO users (name, age) VALUES (?, ?)").unwrap()
insert.bind_str(1, "Charlie")
insert.bind_i64(2, 35)
insert.run()

insert.reset()
insert.bind_str(1, "Diana")
insert.bind_i64(2, 28)
insert.run()
insert.finalize()
```

## 5. Row

```tml
pub type Row {
    stmt_handle: *Unit,
    num_columns: I32
}

impl Row {
    pub func columns(this) -> I32
    pub func column_name(this, idx: I32) -> Str
    pub func column_type(this, idx: I32) -> I32
    pub func get_i64(this, idx: I32) -> I64
    pub func get_i32(this, idx: I32) -> I32
    pub func get_f64(this, idx: I32) -> F64
    pub func get_str(this, idx: I32) -> Str
    pub func is_null(this, idx: I32) -> Bool
    pub func get_value(this, idx: I32) -> Value
}
```

## 6. Value

Dynamic SQLite value for generic column access.

```tml
pub type Value {
    kind: I32,     // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_NULL
    int_val: I64,
    float_val: F64,
    text_val: Str
}

impl Value {
    pub func integer(v: I64) -> Value
    pub func float(v: F64) -> Value
    pub func text(v: Str) -> Value
    pub func null() -> Value

    pub func as_i64(this) -> I64
    pub func as_f64(this) -> F64
    pub func as_str(this) -> Str
    pub func type_code(this) -> I32
    pub func type_name(this) -> Str
    pub func is_null(this) -> Bool
}
```

## 7. Transactions

```tml
let db = Database::open_in_memory().unwrap()
db.exec("CREATE TABLE t (id INTEGER, name TEXT)")

db.begin()
db.exec("INSERT INTO t VALUES (1, 'Alice')")
db.exec("INSERT INTO t VALUES (2, 'Bob')")
db.commit()

// Or rollback on error:
db.begin()
db.exec("INSERT INTO t VALUES (3, 'Charlie')")
db.rollback()  // Charlie is NOT saved
```

## 8. Constants

Key constants from `std::sqlite::constants`:

| Constant | Value | Description |
|----------|-------|-------------|
| `SQLITE_OK` | 0 | Success |
| `SQLITE_ROW` | 100 | `step()` has another row |
| `SQLITE_DONE` | 101 | `step()` finished |
| `SQLITE_INTEGER` | 1 | Column type: integer |
| `SQLITE_FLOAT` | 2 | Column type: float |
| `SQLITE_TEXT` | 3 | Column type: text |
| `SQLITE_NULL` | 5 | Column type: NULL |
| `SQLITE_OPEN_READONLY` | 1 | Open read-only |
| `SQLITE_OPEN_READWRITE` | 2 | Open read-write |
| `SQLITE_OPEN_CREATE` | 4 | Create if not exists |
| `SQLITE_OPEN_MEMORY` | 128 | In-memory database |

## 9. Test Coverage

7 test files in `lib/std/tests/sqlite/` with 77+ tests:

| File | Tests | Description |
|------|-------|-------------|
| `database.test.tml` | 18 | Open, close, exec, prepare, changes, rowid |
| `exec.test.tml` | 8 | DDL/DML execution, error handling |
| `transaction.test.tml` | 6 | Begin, commit, rollback |
| `bind.test.tml` | 12 | Typed parameter binding, named params |
| `statement.test.tml` | 10 | Column metadata, step, reset, finalize |
| `row.test.tml` | 13 | Typed getters, null checks, metadata |
| `value.test.tml` | 10 | Value constructors, type detection |
