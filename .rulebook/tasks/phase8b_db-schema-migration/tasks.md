# Tasks: Database Library — Schema & Migration Engine

**Status**: Planning. 0% (0/18).
**Depends on**: phase8a_db-query-pool

## Phase 1: Schema Types

- [ ] 1.1 `db/schema/table.tml` — TableSchema, ColumnDef, IndexDef, ForeignKeyDef
- [ ] 1.2 `db/schema/types.tml` — SQL type mapping per dialect
- [ ] 1.3 `db/schema/mod.tml` — Schema exports

## Phase 2: DDL Query Builders

- [ ] 2.1 `db/query/create_table.tml` — CreateTableBuilder
- [ ] 2.2 `db/query/alter_table.tml` — AlterTableBuilder
- [ ] 2.3 `db/query/drop_table.tml` — DropTableBuilder
- [ ] 2.4 Tests: DDL rendering for SQLite dialect

## Phase 3: Schema Introspection

- [ ] 3.1 introspect_table(conn, name) -> TableSchema
- [ ] 3.2 introspect_database(conn) -> List[TableSchema]
- [ ] 3.3 SQLite-specific PRAGMA queries
- [ ] 3.4 Tests: introspect existing SQLite tables

## Phase 4: Migration Engine

- [ ] 4.1 `db/migration/migration.tml` — Migration { version, name, up_sql, down_sql, checksum }
- [ ] 4.2 `db/migration/history.tml` — _tml_migrations table management
- [ ] 4.3 `db/migration/runner.tml` — migrate(), rollback(), status()
- [ ] 4.4 `db/migration/diff.tml` — schema_diff(current, target) -> List[MigrationStep]
- [ ] 4.5 `db/migration/generator.tml` — generate_sql(steps, dialect)
- [ ] 4.6 `db/migration/mod.tml` — Migration exports
- [ ] 4.7 Tests: migration apply, rollback, status, diff
- [ ] 4.8 Tests: checksum verification, out-of-order detection
