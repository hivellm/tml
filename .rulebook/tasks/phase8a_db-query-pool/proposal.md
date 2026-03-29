# Proposal: DB Connection Pool + Query Builder

## Why
Connection pooling for performance and a fluent query builder for type-safe SQL construction. Core productivity features for database work.

## What Changes
- ConnectionPool[C] wrapping core::data::Pool with config (min/max/timeout)
- Expression system (Expr enum, col(), comparisons, logical ops)
- Query builders: SelectBuilder, InsertBuilder, UpdateBuilder, DeleteBuilder
- Dialect behavior for SQL rendering differences between databases
- SqliteDialect as first implementation

## Impact
- Affected code: lib/std/src/db/driver/pool.tml, lib/std/src/db/query/ (new)
- Breaking change: NO
- User benefit: Fluent query API, efficient connection reuse
