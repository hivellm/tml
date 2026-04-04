# Tasks: TypeORM Feature Parity — From 66% to 90%+

**Status**: In Progress. 40/40 (100%). Phases 1-6 complete.
**Baseline**: 42/64 TypeORM features (66%)
**Target**: 58/64 features (90%+)

## Phase 1: Query Builder — GROUP BY, HAVING, Subqueries, Params (8 items)

- [x] 1.1 `db/query/select.tml` — Add `group_by(column: Str)` to SelectQuery
- [x] 1.2 `db/query/select.tml` — Add `having(condition: Str)` to SelectQuery
- [x] 1.3 `db/query/expression.tml` — Add `Expr::param(index: I32)` for `?` placeholder
- [x] 1.4 `db/query/expression.tml` — Add `Expr::named_param(name: Str)` for `:name` placeholder
- [x] 1.5 `db/query/select.tml` — Add `join(clause: JoinClause)` to SelectQuery for integrated JOINs
- [x] 1.6 `db/query/select.tml` — Add `subquery(sub: SelectQuery)` support in WHERE (IN subquery)
- [x] 1.7 `db/query/expression.tml` — Add `Expr::in_subquery(col, subquery_sql)` for `col IN (SELECT ...)`
- [x] 1.8 Tests: GROUP BY/HAVING/param/subquery rendering — `lib/std/tests/db/db_query_groupby.test.tml` (18 tests)

## Phase 2: Entity Enhancements — Timestamps, Unique, Length (7 items)

- [x] 2.1 `db/orm/entity.tml` — Add `Column::unique()` builder method
- [x] 2.2 `db/orm/entity.tml` — Add `Column::varchar(name, length)` with length constraint in DDL
- [x] 2.3 `db/orm/entity.tml` — Add `Column::decimal(name, precision, scale)` for numeric precision
- [x] 2.4 `db/orm/entity.tml` — Add `Column::timestamp(name)` for TIMESTAMP columns
- [x] 2.5 `db/orm/entity.tml` — Add `Column::created_at()` → `created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP`
- [x] 2.6 `db/orm/entity.tml` — Add `Column::updated_at()` → `updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP`
- [x] 2.7 Tests: unique DDL, varchar(N), decimal(P,S), auto-timestamp columns — `lib/std/tests/db/db_orm_entity_phase2.test.tml` (19 tests)

## Phase 3: Repository CRUD — Upsert, FindBy, Count, SoftDelete (9 items)

- [x] 3.1 `db/orm/repository.tml` — `save()` — INSERT OR REPLACE (upsert by PK)
- [x] 3.2 `db/orm/repository.tml` — `find_by(table, col, val)` — SELECT WHERE any column = value
- [x] 3.3 `db/orm/repository.tml` — `find_many(table, where_clause, limit)` — multi-row SELECT
- [x] 3.4 `db/orm/repository.tml` — `count_where(conn, table, condition)` — conditional COUNT
- [x] 3.5 `db/orm/repository.tml` — `soft_delete(conn, table, pk_col, pk_val, timestamp)` — SET deleted_at = timestamp
- [x] 3.6 `db/orm/repository.tml` — `restore(conn, table, pk_col, pk_val)` — SET deleted_at = NULL
- [x] 3.7 `db/orm/repository.tml` — `increment(conn, table, col, amount, pk_col, pk_val)` — atomic increment
- [x] 3.8 `db/orm/repository.tml` — `exists(conn, table, col, val)` — quick existence check
- [x] 3.9 Tests: upsert, findBy, count_where, soft_delete, restore, increment, exists — `lib/std/tests/db/db_orm_repository_phase3.test.tml` (14 tests)

## Phase 4: Relations — Eager Loading, Cascade, JoinTable (8 items)

- [x] 4.1 `db/orm/relation.tml` — Add `Relation::with_cascade(insert, update, delete)` options (already implemented, verified)
- [x] 4.2 `db/orm/relation.tml` — Add `Relation::join_table(pivot_table, local_col, foreign_col)` for M:N (already implemented, verified)
- [x] 4.3 `db/orm/relation.tml` — Add `Relation::inverse_side(field_name)` for bidirectional refs (already implemented, verified)
- [x] 4.4 `db/orm/eager.tml` — `eager_load_sql(table, relations)` → generates SELECT with LEFT JOINs (already implemented, verified)
- [x] 4.5 `db/orm/eager.tml` — `load_with(conn, table, pk, relations)` → executes eager query (already implemented, verified)
- [x] 4.6 `db/orm/cascade.tml` — `cascade_insert` + `cascade_insert_sql` (already implemented, verified)
- [x] 4.7 `db/orm/cascade.tml` — `cascade_delete` + `cascade_delete_sql` (already implemented, verified)
- [x] 4.8 Tests: `db_orm_relations_phase4.test.tml` — 19 tests: cascade options, join table, inverse side, eager SQL, cascade SQL. `eager` + `cascade` modules exported from `db/orm/mod.tml`.

## Phase 5: QuerySet Integration — JOIN, GROUP, Full Chain (4 items)

- [x] 5.1 `db/orm/query_set.tml` — Add `join(join_clause: Str)` → raw SQL join appended to query. `QuerySet` renamed `OrmQuerySet` to avoid GlobalASTCache layout collision.
- [x] 5.2 `db/orm/query_set.tml` — Add `group_by(col)` and `having(condition)` delegating to SelectQuery; tracked in OrmQuerySet fields for join-path SQL construction.
- [x] 5.3 `db/orm/query_set.tml` — Add `with_relation(rel: Relation)` shorthand using `rel.join_sql(table)`.
- [x] 5.4 Tests: `db_orm_queryset_phase5.test.tml` (4 tests) + `db_qs_phase5.test.tml` (6 tests) + `db_qs_join_smoke.test.tml` (4 tests). Split across files due to test harness stack limit with large OrmQuerySet structs.

## Phase 6: Tests & Documentation (4 items)

- [x] 6.1 Integration tests: `db_orm_lifecycle.test.tml` — entity DDL + filter/limit/offset query lifecycle (4 tests).
- [x] 6.2 Integration tests: `db_orm_relations_integration.test.tml` — eager load SQL, cascade SQL, M:N config (4 tests).
- [x] 6.3 Updated `db/orm/mod.tml` doc comments to include `eager` and `cascade` modules with descriptions.
- [x] 6.4 Feature parity: OrmQuerySet now covers join, group_by, having, with_relation, order_by, limit, offset, filter, count_sql. 27/28 test suites passing (pre-existing failures unchanged).

## Coverage Target

After completion:
| Categoria | TypeORM | TML | % |
|-----------|---------|-----|---|
| Entity/Model | 12 | 10 | 83% |
| Relations | 10 | 8 | 80% |
| Repository/CRUD | 12 | 11 | 92% |
| Query Builder | 16 | 15 | 94% |
| Migrations | 8 | 7 | 88% |
| Connection/Driver | 6 | 5 | 83% |
| **Total** | **64** | **56** | **88%** |

## What Stays Out of Scope (requires compiler features)
- Generic `Repository[T]` struct (needs codegen for generic structs with behavior bounds)
- Compile-time decorator → metadata reflection (needs codegen emit for decorator metadata)
- Lazy loading via proxies (needs runtime code generation)
- Auto column type inference from TML types (needs compile-time type introspection)
