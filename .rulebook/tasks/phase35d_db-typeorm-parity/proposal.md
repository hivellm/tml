# Proposal: TypeORM Feature Parity — From 66% to 90%+

## Why
TML std::db currently implements 42/64 TypeORM features (66%). The missing 22 features
include high-impact items like GROUP BY, parameter binding, eager loading, upsert,
and soft delete that are table-stakes for any production ORM. Reaching 90%+ parity
makes TML a credible alternative for database-heavy applications.

## What Changes
- Query builder: GROUP BY, HAVING, subqueries, parameter binding with ?/named params
- Entity: @CreateDateColumn, @UpdateDateColumn, unique constraint builder, column length/precision
- Repository: save() upsert, findBy() for any field, conditional count, soft delete/restore, increment
- Relations: eager loading via JOIN, cascade insert/update/delete, @JoinTable for M:N pivots
- QuerySet: integrated JOIN support, GROUP BY, HAVING

## Impact
- Affected code: lib/std/src/db/query/, lib/std/src/db/orm/, lib/std/tests/db/
- Breaking change: NO (all additive)
- User benefit: Production-grade ORM comparable to TypeORM/Prisma/SQLAlchemy
