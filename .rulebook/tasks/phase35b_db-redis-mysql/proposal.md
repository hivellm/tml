# Proposal: Redis + MySQL Drivers

**Task**: phase8h_db-redis-mysql
**Status**: Planning (0/18)
**Priority**: P2
**Estimated effort**: 4–6 days
**Risk**: Low

## Problem

Two of the most prevalent open-source data stores in production stacks — Redis and MySQL
— have no TML drivers. Redis is the de-facto standard for caching, session storage, rate
limiting, and pub/sub messaging. MySQL is the most deployed open-source relational
database (used by WordPress, Shopify, GitHub, and countless others). Without these two
drivers, TML cannot support the majority of existing backend stacks that combine a
relational SQL database with a fast in-memory cache.

## Proposed Solution

Two independent libraries implemented in the same task phase, sharing the same FFI
pattern already proven by the SQLite and PostgreSQL drivers.

**Redis (`lib/redis/`)**: Built on hiredis FFI. Exposes a typed command API covering
strings, lists, hashes, sets, sorted sets, streams, and HyperLogLog. A `PubSub` type
handles SUBSCRIBE/PSUBSCRIBE with a callback-based message loop. Pipeline support batches
multiple commands into a single round-trip.

**MySQL (`lib/mysql/`)**: Built on libmysqlclient FFI. MysqlConnection implements
`std::db::Connection`. MysqlDialect uses backtick identifier quoting and `?` positional
parameters. MysqlStatement wraps prepared statement handles with parameter binding and
result set streaming via `MysqlCursor`.

## Key Decisions

- **hiredis for Redis** — thin, stable, widely tested C client; avoids reimplementing the
  RESP2/RESP3 protocol in TML.
- **libmysqlclient for MySQL** — the official C client maintained by Oracle; handles
  TLS, auth plugins (caching_sha2_password, ed25519), and character set negotiation.
- **Both use `build.tml`** — platform-specific library discovery (pkg-config / Registry)
  keeps user code free of platform ifdefs.
- **Redis pipeline API** — `conn.pipeline().set("k","v").incr("counter").exec()` sends
  all commands in one TCP write and reads all replies together.
- **MySQL `?` placeholders** — matches libmysqlclient's native prepared statement API;
  MysqlDialect translates if the std::db layer passes named parameters.

## Files to Create/Modify

**Redis library:**
- `lib/redis/src/ffi.tml` — `@extern("c")` hiredis declarations
- `lib/redis/src/driver.tml` — RedisDriver, RedisConnection
- `lib/redis/src/connection.tml` — connect/disconnect, AUTH, SELECT database
- `lib/redis/src/commands.tml` — typed GET/SET/DEL/INCR/LPUSH/HSET/ZADD/… API
- `lib/redis/src/pubsub.tml` — PubSub type with subscribe/psubscribe/message loop
- `lib/redis/src/mod.tml` — public re-exports
- `lib/redis/build.tml` + `lib/redis/package.toml`

**MySQL library:**
- `lib/mysql/src/ffi.tml` — `@extern("c")` libmysqlclient declarations
- `lib/mysql/src/driver.tml` — MysqlDriver implementing Driver behavior
- `lib/mysql/src/connection.tml` — MysqlConnection implementing Connection behavior
- `lib/mysql/src/statement.tml` — MysqlStatement with parameter binding
- `lib/mysql/src/dialect.tml` — MysqlDialect (backtick quoting, ? placeholders)
- `lib/mysql/src/mod.tml` — public re-exports
- `lib/mysql/build.tml` + `lib/mysql/package.toml`

## Success Criteria

- All 18 checklist items marked done
- Redis: GET/SET/DEL round-trip, INCR atomicity, LPUSH+LRANGE, HSET+HGET, pipeline
  executes all commands in a single round-trip, PubSub receives published messages
- MySQL: INSERT + SELECT + prepared statement re-use, MysqlDialect produces valid SQL,
  result set streaming does not load entire table into memory
- Both: no memory leaks, native handles freed on connection drop
- Integration tests pass against live Redis ≥ 7 and MySQL ≥ 8 instances

## Dependencies

- **Depends on**: std::db abstraction (phase8d, complete); hiredis headers/binaries;
  libmysqlclient headers/binaries (MySQL Connector/C or MariaDB Connector/C)
- **Blocks**: caching layer, session middleware, and any application targeting MySQL
  as its primary RDBMS
