# Tasks: Database Library — Connection Pool + Query Builder

**Status**: Planning. 0% (0/22).
**Depends on**: phase8_db-foundation

## Phase 1: Connection Pool

- [ ] 1.1 `db/driver/pool.tml` — PoolConfig { min_connections, max_connections, idle_timeout_ms, connect_timeout_ms }
- [ ] 1.2 `db/driver/pool.tml` — ConnectionPool[C] wrapping core::data::Pool
- [ ] 1.3 acquire() -> DbResult[C], release(conn), close_all()
- [ ] 1.4 Pool stats (active, idle, total, wait count)
- [ ] 1.5 Tests: pool lifecycle, acquire/release, max connections, timeout

## Phase 2: Expression System

- [ ] 2.1 `db/query/expression.tml` — Expr enum (Column, Value, BinaryOp, UnaryOp, Function, Raw)
- [ ] 2.2 col() function, comparison methods (eq, ne, gt, lt, gte, lte)
- [ ] 2.3 Logical combinators (and, or, not)
- [ ] 2.4 Pattern matching (like, in_list, between, is_null)
- [ ] 2.5 Tests: expression building, rendering to SQL

## Phase 3: Query Builders

- [ ] 3.1 `db/query/dialect.tml` — Dialect behavior (quote_identifier, placeholder, render_limit, supports_returning)
- [ ] 3.2 `db/sqlite/dialect.tml` — SqliteDialect impl
- [ ] 3.3 `db/query/select.tml` — SelectBuilder (columns, where, order_by, limit, offset, group_by, having)
- [ ] 3.4 `db/query/insert.tml` — InsertBuilder (columns, values, returning)
- [ ] 3.5 `db/query/update.tml` — UpdateBuilder (set, where, returning)
- [ ] 3.6 `db/query/delete.tml` — DeleteBuilder (where, returning)
- [ ] 3.7 `db/query/join.tml` — JoinClause (Inner, Left, Right, Full, Cross)
- [ ] 3.8 `db/query/order.tml` — OrderBy, SortDirection
- [ ] 3.9 `db/query/aggregate.tml` — count, sum, avg, min, max
- [ ] 3.10 `db/query/raw.tml` — RawSql escape hatch
- [ ] 3.11 `db/query/mod.tml` — Query builder exports

## Phase 4: Integration Tests

- [ ] 4.1 Tests: SELECT builder rendering + execution against SQLite
- [ ] 4.2 Tests: INSERT/UPDATE/DELETE builder
- [ ] 4.3 Tests: JOIN and aggregate queries
