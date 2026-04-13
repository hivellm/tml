# Tasks: Database Library — Redis + MySQL Drivers

**Status**: Planning. 0% (0/18).
**Depends on**: phase8_db-foundation, phase8e_db-conditional-compilation

## Phase 1: Redis Driver (lib/redis/)

- [ ] 1.1 hiredis FFI: redisConnect, redisCommand, redisFree
- [ ] 1.2 RedisDriver + RedisConnection
- [ ] 1.3 Commands: GET, SET, DEL, HGET, HSET, LPUSH, RPOP, etc.
- [ ] 1.4 Pub/Sub: SUBSCRIBE, PUBLISH
- [ ] 1.5 mod.tml + tml.toml
- [ ] 1.6 Integration tests
- [ ] 1.7 Redis benchmarks

## Phase 2: MySQL Driver (lib/mysql/)

- [ ] 2.1 libmysqlclient FFI: mysql_init, mysql_real_connect, mysql_close
- [ ] 2.2 Query: mysql_query, mysql_store_result, mysql_fetch_row
- [ ] 2.3 Prepared: mysql_stmt_init, mysql_stmt_prepare, mysql_stmt_execute
- [ ] 2.4 MysqlDriver impl Driver
- [ ] 2.5 MysqlConnection impl Connection
- [ ] 2.6 MysqlStatement impl PreparedStatement
- [ ] 2.7 MysqlDialect (backtick quoting, ? placeholders)
- [ ] 2.8 mod.tml + tml.toml
- [ ] 2.9 Integration tests

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
