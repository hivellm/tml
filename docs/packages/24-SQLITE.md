# std::sqlite — SQLite Embedded Database

## Overview

The `std::sqlite` module provides FFI bindings to SQLite3 for embedded database operations. SQLite is a self-contained, serverless SQL database engine that stores all data in a single file.

```tml
use std::sqlite                              // all re-exports
use std::sqlite::{Database, Statement}
use std::sqlite::{Row, Value}
use std::sqlite::{SqliteError}
```

## Core Types

### Database

Main database connection:

```tml
type Database {
  handle: I64  // opaque SQLite3 database pointer
}

func Database::open(path: Str) -> Outcome[Heap[Database], Str]
  Open file-based database, create if not exists

func Database::memory() -> Heap[Database]
  Create in-memory database (data lost on close)

func exec(db: ref Database, sql: Str) -> Outcome[(), Str]
  Execute SQL statement without result set (INSERT, UPDATE, DELETE, CREATE, etc.)

func prepare(db: ref Database, sql: Str) -> Outcome[Heap[Statement], Str]
  Prepare statement for execution with parameters

func begin(db: ref Database) -> Outcome[(), Str]
  Start transaction (BEGIN)

func commit(db: ref Database) -> Outcome[(), Str]
  Commit transaction (COMMIT)

func rollback(db: ref Database) -> Outcome[(), Str]
  Rollback transaction (ROLLBACK)

func changes(db: ref Database) -> I64
  Get number of rows affected by last INSERT/UPDATE/DELETE

func last_insert_rowid(db: ref Database) -> I64
  Get ROWID of last inserted row
```

### Statement

Compiled prepared statement:

```tml
type Statement {
  handle: I64,  // opaque SQLite3 statement pointer
  db: Heap[ref Database]
}

func bind_i64(stmt: ref Statement, param: I64, value: I64) -> Outcome[(), Str]
  Bind integer parameter (1-indexed)

func bind_f64(stmt: ref Statement, param: I64, value: F64) -> Outcome[(), Str]
  Bind float parameter (1-indexed)

func bind_str(stmt: ref Statement, param: I64, value: Str) -> Outcome[(), Str]
  Bind string parameter (1-indexed)

func bind_null(stmt: ref Statement, param: I64) -> Outcome[(), Str]
  Bind NULL value at parameter (1-indexed)

func step(stmt: ref Statement) -> Outcome[Bool, Str]
  Execute statement, return true if row available, false if done

func execute(stmt: ref Statement) -> Outcome[I64, Str]
  Execute and return rows affected

func reset(stmt: ref Statement) -> Outcome[(), Str]
  Reset statement for re-execution
```

### Row

Single result row:

```tml
type Row {
  handle: I64,  // opaque SQLite3 statement pointer
  col_count: I64
}

func get_i64(row: ref Row, col: I64) -> I64
  Get integer column value (0-indexed)

func get_f64(row: ref Row, col: I64) -> F64
  Get float column value (0-indexed)

func get_str(row: ref Row, col: I64) -> Str
  Get string column value (0-indexed)

func is_null(row: ref Row, col: I64) -> Bool
  Check if column is NULL (0-indexed)

func column_count(row: ref Row) -> I64
  Get number of columns in result set

func column_name(row: ref Row, col: I64) -> Str
  Get column name (0-indexed)
```

### Value

Dynamic SQLite value:

```tml
enum Value {
  Integer(I64),
  Float(F64),
  Text(Str),
  Null
}

func Value::to_i64(v: ref Value) -> Maybe[I64]
func Value::to_f64(v: ref Value) -> Maybe[F64]
func Value::to_str(v: ref Value) -> Maybe[Str]
func Value::is_null(v: ref Value) -> Bool
```

## Example: CRUD Operations

```tml
use std::sqlite::{Database, Statement}

func main() {
  // Open database
  let db = Database::open("data.db").unwrap()

  // Create table
  db.exec("CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT UNIQUE
  )").unwrap()

  // Prepared insert
  let stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)").unwrap()
  stmt.bind_str(1, "Alice").unwrap()
  stmt.bind_str(2, "alice@example.com").unwrap()
  stmt.execute().unwrap()

  // Query with parameters
  let query = db.prepare("SELECT * FROM users WHERE id = ?").unwrap()
  query.bind_i64(1, 1).unwrap()

  when query.step() {
    Ok(true) => {
      print("Found user: {query.get_str(1)}\n")
    },
    Ok(false) => {
      print("Not found\n")
    },
    Err(e) => print("Error: {e}\n")
  }

  // Transaction
  db.begin().unwrap()
  db.exec("UPDATE users SET name = 'Bob' WHERE id = 1").unwrap()
  db.commit().unwrap()
}
```

## Example: Bulk Insert with Transaction

```tml
use std::sqlite::Database

func bulk_insert(db: ref Database, users: List[{name: Str, email: Str}]) {
  db.begin().unwrap()

  let stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)").unwrap()
  
  loop u in users {
    stmt.bind_str(1, u.name).unwrap()
    stmt.bind_str(2, u.email).unwrap()
    stmt.execute().unwrap()
    stmt.reset().unwrap()
  }

  db.commit().unwrap()
  print("Inserted {users.len()} rows\n")
}
```

## Example: Query with Type Handling

```tml
use std::sqlite::Database

func query_all_users(db: ref Database) {
  let query = db.prepare("SELECT id, name, email FROM users").unwrap()

  loop {
    when query.step() {
      Ok(true) => {
        let id = query.get_i64(0)
        let name = query.get_str(1)
        let email = when query.is_null(2) {
          true => "<unknown>",
          false => query.get_str(2)
        }
        print("{id}: {name} ({email})\n")
      },
      Ok(false) => break,  // done
      Err(e) => {
        print("Error: {e}\n")
        break
      }
    }
  }
}
```

## FFI Modules

- `sqlite::ffi` — Low-level C bindings to sqlite3.h
- `sqlite::constants` — SQLite result codes and configuration constants

## Supported Data Types

SQLite supports 5 fundamental datatypes:
- **NULL** — NULL value
- **INTEGER** — 64-bit signed integer
- **REAL** — 8-byte IEEE float
- **TEXT** — UTF-8 text string
- **BLOB** — Binary large object (treated as Vec[U8])

## See Also

- [std::file](./01-FS.md) — File operations (for database file handling)
- [std::encoding](./04-ENCODING.md) — Data encoding and serialization
- [std::collections](./10-COLLECTIONS.md) — In-memory collections

---

*Previous: [23-STREAM.md](./23-STREAM.md)*
*Next: [25-EVENTS.md](./25-EVENTS.md)*
