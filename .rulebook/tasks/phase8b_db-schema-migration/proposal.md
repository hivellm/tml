# Proposal: DB Schema & Migration Engine

## Why
Schema introspection and migration engine for production database management. Auto-diff generation enables Prisma-like developer experience.

## What Changes
- Schema types (TableSchema, ColumnDef, IndexDef, ForeignKeyDef)
- DDL builders (CREATE TABLE, ALTER TABLE, DROP TABLE)
- Schema introspection via PRAGMA (SQLite) / information_schema (others)
- Migration engine: apply, rollback, status, diff generation, checksum verification
- _tml_migrations tracking table

## Impact
- Affected code: lib/std/src/db/schema/ (new), lib/std/src/db/migration/ (new)
- Breaking change: NO
- User benefit: Auto-generated migrations, schema versioning
