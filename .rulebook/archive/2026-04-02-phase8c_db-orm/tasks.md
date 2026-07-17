# Tasks: Database Library — ORM Layer

**Status**: Complete. 100% (15/15).
**Depends on**: phase8b_db-schema-migration

## Phase 1: Model & Field Mapping

- [x] 1.1 `db/orm/model.tml` — Model behavior (table_name, columns, primary_key_name, primary_key_value, to_values, to_set_clause)
- [x] 1.2 `db/orm/field.tml` — FieldType enum (Integer/Text/Real/Blob/Boolean), FieldMapping with new() and pk()
- [x] 1.3 `db/orm/mapper.tml` — quote_str; sqlite read helpers split into row_reader.tml to avoid link-time sqlite3 deps in pure tests

## Phase 2: Repository

- [x] 2.1 `db/orm/sql_builder.tml` — pure SQL string builders (find_all_sql, find_by_id_sql, insert_sql, update_sql, delete_sql, count_sql)
- [x] 2.2 `db/orm/repository.tml` — insert, update, delete (execute against SqliteConnection)
- [x] 2.3 `db/orm/repository.tml` — count (prepare + step pattern)

## Phase 3: QuerySet

- [x] 3.1 `db/orm/query_set.tml` — QuerySet with filter, order_by, limit, offset (all immutable/value-returning)
- [x] 3.2 to_sql(), count_sql() — renders full SELECT and COUNT SQL strings

## Phase 4: Relations

- [x] 4.1 `db/orm/relation.tml` — RelationType enum (HasOne/HasMany/BelongsTo/ManyToMany) with to_string()
- [x] 4.2 Relation::has_one, has_many, belongs_to constructors; join_sql() generates LEFT JOIN SQL
- [x] 4.3 `db/orm/mod.tml` — exports model, field, mapper, row_reader, sql_builder, repository, query_set, relation; db/mod.tml updated with `pub mod orm`

## Phase 5: Tests

- [x] 5.1 `lib/std/tests/db/db_orm.test.tml` — FieldType/FieldMapping tests, sql_builder CRUD SQL tests
- [x] 5.2 QuerySet chaining: filter, order_by, limit, offset, multi-filter AND, count_sql
- [x] 5.3 Relation has_one/has_many/belongs_to construction and join_sql; RelationType to_string; quote_str/mapper

**Notes**:
- sqlite3 FFI symbols pulled in by any file importing SqliteStatement — split mapper into pure mapper.tml + row_reader.tml (sqlite-dependent) to avoid linker errors in pure tests
- repository.tml depends on sqlite; tests use sql_builder directly for SQL-building assertions
- 12/12 std/db suite passing after implementation
