# Tasks: TypeORM Feature Parity — From 66% to 90%+

**Status**: Planning. 0% (0/40).
**Baseline**: 42/64 TypeORM features (66%)
**Target**: 58/64 features (90%+)

## Phase 1: Query Builder — GROUP BY, HAVING, Subqueries, Params (8 items)

- [ ] 1.1 `db/query/select.tml` — Add `group_by(column: Str)` to SelectQuery
- [ ] 1.2 `db/query/select.tml` — Add `having(condition: Str)` to SelectQuery
- [ ] 1.3 `db/query/expression.tml` — Add `Expr::param(index: I32)` for `?` placeholder
- [ ] 1.4 `db/query/expression.tml` — Add `Expr::named_param(name: Str)` for `:name` placeholder
- [ ] 1.5 `db/query/select.tml` — Add `join(clause: JoinClause)` to SelectQuery for integrated JOINs
- [ ] 1.6 `db/query/select.tml` — Add `subquery(sub: SelectQuery)` support in WHERE (IN subquery)
- [ ] 1.7 `db/query/expression.tml` — Add `Expr::in_subquery(col, subquery_sql)` for `col IN (SELECT ...)`
- [ ] 1.8 Tests: GROUP BY/HAVING/param/subquery rendering

## Phase 2: Entity Enhancements — Timestamps, Unique, Length (7 items)

- [ ] 2.1 `db/orm/entity.tml` — Add `Column::unique()` builder method
- [ ] 2.2 `db/orm/entity.tml` — Add `Column::varchar(name, length)` with length constraint in DDL
- [ ] 2.3 `db/orm/entity.tml` — Add `Column::decimal(name, precision, scale)` for numeric precision
- [ ] 2.4 `db/orm/entity.tml` — Add `Column::timestamp(name)` for TIMESTAMP columns
- [ ] 2.5 `db/orm/entity.tml` — Add `Column::created_at()` → `created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP`
- [ ] 2.6 `db/orm/entity.tml` — Add `Column::updated_at()` → `updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP`
- [ ] 2.7 Tests: unique DDL, varchar(N), decimal(P,S), auto-timestamp columns

## Phase 3: Repository CRUD — Upsert, FindBy, Count, SoftDelete (9 items)

- [ ] 3.1 `db/orm/repository.tml` — `save()` — INSERT OR REPLACE (upsert by PK)
- [ ] 3.2 `db/orm/repository.tml` — `find_by(table, col, val)` — SELECT WHERE any column = value
- [ ] 3.3 `db/orm/repository.tml` — `find_many(table, where_clause, limit)` — multi-row SELECT
- [ ] 3.4 `db/orm/repository.tml` — `count_where(conn, table, condition)` — conditional COUNT
- [ ] 3.5 `db/orm/repository.tml` — `soft_delete(conn, table, pk_col, pk_val)` — SET deleted_at = NOW
- [ ] 3.6 `db/orm/repository.tml` — `restore(conn, table, pk_col, pk_val)` — SET deleted_at = NULL
- [ ] 3.7 `db/orm/repository.tml` — `increment(conn, table, col, amount, pk_col, pk_val)` — atomic increment
- [ ] 3.8 `db/orm/repository.tml` — `exists(conn, table, col, val)` — quick existence check
- [ ] 3.9 Tests: upsert, findBy, count_where, soft_delete, restore, increment, exists

## Phase 4: Relations — Eager Loading, Cascade, JoinTable (8 items)

- [ ] 4.1 `db/orm/relation.tml` — Add `Relation::with_cascade(insert, update, delete)` options
- [ ] 4.2 `db/orm/relation.tml` — Add `Relation::join_table(pivot_table, local_col, foreign_col)` for M:N
- [ ] 4.3 `db/orm/relation.tml` — Add `Relation::inverse_side(field_name)` for bidirectional refs
- [ ] 4.4 `db/orm/eager.tml` — `eager_load_sql(table, relations)` → generates SELECT with LEFT JOINs
- [ ] 4.5 `db/orm/eager.tml` — `load_with(conn, table, pk, relations)` → executes eager query
- [ ] 4.6 `db/orm/cascade.tml` — `cascade_insert(conn, parent_table, child_relation, parent_pk, child_values)`
- [ ] 4.7 `db/orm/cascade.tml` — `cascade_delete(conn, parent_table, child_relation, parent_pk)`
- [ ] 4.8 Tests: eager loading SQL, cascade insert/delete, M:N join_table

## Phase 5: QuerySet Integration — JOIN, GROUP, Full Chain (4 items)

- [ ] 5.1 `db/orm/query_set.tml` — Add `join(relation: Relation)` → integrates JoinClause into query
- [ ] 5.2 `db/orm/query_set.tml` — Add `group_by(col)` and `having(condition)` delegating to SelectQuery
- [ ] 5.3 `db/orm/query_set.tml` — Add `with_relation(name, target_table, fk)` shorthand for eager join
- [ ] 5.4 Tests: QuerySet with JOIN + GROUP BY + HAVING + full chain

## Phase 6: Tests & Documentation (4 items)

- [ ] 6.1 Integration tests: full entity lifecycle (define → create → insert → query → update → delete → drop)
- [ ] 6.2 Integration tests: relations (create parent + child → eager load → cascade delete)
- [ ] 6.3 Update db/mod.tml doc comments with complete API overview
- [ ] 6.4 Update benchmarks/db-sqlite/RESULTS.md with feature parity table

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
