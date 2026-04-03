# Proposal: DB Conditional Compilation + Feature Flags

## Why
Enable optional driver support via #ifdef and tml.toml feature flags. Required for external drivers (PostgreSQL, MySQL, etc.) to be conditionally included.

## What Changes
- Verify --define CLI flag works for custom symbols
- Parse [features] section in tml.toml, generate symbols (postgres -> DB_POSTGRES)
- Update db/mod.tml with #ifdef blocks for optional drivers
- Driver registry and URL-based routing
- External driver scaffold template

## Impact
- Affected code: compiler (feature parsing), lib/std/src/db/mod.tml
- Breaking change: NO
- User benefit: Opt-in driver support, smaller binaries
