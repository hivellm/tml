# Proposal: DB ORM Layer

## Why
Data Mapper ORM with Repository pattern for productive database development. Maps TML structs to database tables with CRUD operations and relationship loading.

## What Changes
- Model behavior (table_name, columns, from_row, to_values)
- Repository[T, C] with insert, update, delete, find_by_id, find_all
- QuerySet[T] with filter, order_by, limit, offset, exec, first, count
- Relations: HasOne, HasMany, BelongsTo, ManyToMany with eager loading

## Impact
- Affected code: lib/std/src/db/orm/ (new)
- Breaking change: NO
- User benefit: Clean struct-to-table mapping, chainable queries
