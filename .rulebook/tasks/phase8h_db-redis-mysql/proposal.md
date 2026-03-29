# Proposal: Redis + MySQL Drivers

## Why
Complete the driver ecosystem. Redis for key-value/pub-sub, MySQL for the most popular open-source SQL database.

## What Changes
- lib/redis/ with hiredis FFI, command API, pub/sub
- lib/mysql/ with libmysqlclient FFI, MysqlDialect (backtick quoting, ? placeholders)
- Both implement std::db Driver behaviors

## Impact
- Affected code: lib/redis/ (new), lib/mysql/ (new)
- Breaking change: NO
- User benefit: Redis + MySQL support
