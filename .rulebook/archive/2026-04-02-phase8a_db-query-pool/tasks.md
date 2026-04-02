# Tasks: Database Library — Connection Pool + Query Builder

**Status**: Complete. 100% (22/22).
**Depends on**: phase8_db-foundation

## Phase 1: Connection Pool

- [x] 1.1 `db/driver/pool.tml` — PoolConfig { min_connections, max_connections, idle_timeout_ms, connect_timeout_ms }
- [x] 1.2 `db/driver/pool.tml` — ConnectionPool with acquire/release/close_all/stats
- [x] 1.3 acquire() -> Outcome[Bool, DbError], release(), close_all()
- [x] 1.4 Pool stats (active, idle, total, wait count) — PoolStats type
- [x] 1.5 Tests: pool lifecycle, acquire/release, max connections, timeout — db_pool.test.tml (4 tests)

## Phase 2: Expression System

- [x] 2.1 `db/query/expression.tml` — Expr flat struct (Column, Integer, Float, Str, Null, Compare, Logic, IsNull, IsNotNull, Raw)
- [x] 2.2 col() function, comparison methods (eq, ne, gt, lt, gte, lte, like)
- [x] 2.3 Logical combinators (and_expr, or_expr)
- [x] 2.4 Pattern matching (like, is_null_expr, is_not_null)
- [x] 2.5 Tests: expression building, rendering to SQL — covered in query builder tests

## Phase 3: Query Builders

- [x] 3.1 `db/query/dialect.tml` — Dialect behavior (quote_identifier, placeholder, render_limit, supports_returning)
- [x] 3.2 `db/sqlite/dialect.tml` — SqliteDialect impl (quote_identifier, placeholder, supports_returning)
- [x] 3.3 `db/query/select.tml` — SelectQuery (columns, where_expr, limit, offset)
- [x] 3.4 `db/query/insert.tml` — InsertQuery (value, to_sql with DEFAULT VALUES)
- [x] 3.5 `db/query/update.tml` — UpdateQuery (set, where_expr)
- [x] 3.6 `db/query/delete_query.tml` — DeleteQuery (where_expr)
- [x] 3.7 `db/query/join.tml` — JoinClause (Inner, Left, Right, Full, Cross)
- [x] 3.8 `db/query/order.tml` — OrderByClause (asc, desc)
- [x] 3.9 `db/query/aggregate.tml` — count, count_all, sum, avg, min_val, max_val
- [x] 3.10 `db/query/raw.tml` — RawSql type with new() and to_sql()
- [x] 3.11 `db/query/mod.tml` — Query builder exports

## Phase 4: Integration Tests

- [x] 4.1 Tests: SELECT builder rendering — `lib/std/tests/db/db_query_select.test.tml` (7 tests)
- [x] 4.2 Tests: INSERT/UPDATE/DELETE builder — `db_query_insert.test.tml` (6), `db_query_delete.test.tml` (5)
- [x] 4.3 Tests: JOIN and aggregate queries — `db_query_misc.test.tml` (12 tests)
- Note: Fixed `.as_str()` codegen bug in select.tml + expression.tml (use `.to_string()` instead of `` `{val}`.as_str() ``)
