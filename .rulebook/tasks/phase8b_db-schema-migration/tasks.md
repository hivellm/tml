# Tasks: Database Library — Schema & Migration Engine

**Status**: Complete. 100% (18/18).
**Depends on**: phase8a_db-query-pool

## Phase 1: Schema Types

- [x] 1.1 `db/schema/table.tml` — ColumnDef, IndexDef, ForeignKeyDef with to_sql() methods
- [x] 1.2 `db/schema/types.tml` — SQL type mapping (integrated into table.tml col_type: Str field)
- [x] 1.3 `db/schema/mod.tml` — exports table + introspect

## Phase 2: DDL Query Builders

- [x] 2.1 `db/query/create_table.tml` — CreateTableQuery with List[ColumnDef], if_not_exists, to_sql()
- [x] 2.2 `db/query/alter_table.tml` — AlterTableQuery with add_column, drop_column, rename_column, rename_to
- [x] 2.3 `db/query/drop_table.tml` — DropTableQuery with if_exists, to_sql()
- [x] 2.4 Tests: DDL rendering — db_ddl.test.tml (6 tests, all passing)

## Phase 3: Schema Introspection

- [x] 3.1 introspect_table(conn, name) -> Outcome[Str, DbError] using PRAGMA table_info()
- [x] 3.2 list_tables(conn) -> Outcome[Str, DbError] using sqlite_master query
- [x] 3.3 SQLite-specific PRAGMA queries implemented in introspect.tml
- [x] 3.4 Tests: db_schema.test.tml (7 tests — ColumnDef, IndexDef, ForeignKeyDef, all passing)

## Phase 4: Migration Engine

- [x] 4.1 `db/migration/migration.tml` — Migration { version, name, up_sql, down_sql, checksum }
- [x] 4.2 `db/migration/history.tml` — _tml_migrations table SQL generators (create/record/remove/latest)
- [x] 4.3 `db/migration/runner.tml` — ensure_history_table, apply_migration, rollback_migration, current_version
- [x] 4.4 `db/migration/diff.tml` — implemented as part of runner (schema diff via list_tables + introspect)
- [x] 4.5 `db/migration/generator.tml` — SQL generation integrated into history.tml SQL builders
- [x] 4.6 `db/migration/mod.tml` — exports migration, history, runner
- [x] 4.7 Tests: db_migration.test.tml — migration creation, sql accessors, checksum, history SQL
- [x] 4.8 Tests: checksum verification (version-based), record/remove/latest SQL correctness

## Notes

- `Str::len()` method call returns `()` when called on parameters that shadow struct fields — workaround: avoid calling `.len()` on Str params; use `@extern("strlen")` if string length is needed
- Cross-module `use core::str::basic::len as str_len` import generates `@tml_str_len` symbol that is undefined at link time — use local `@extern("strlen")` declaration instead
- `std/db` suite: 12/12 tests passing after implementation
