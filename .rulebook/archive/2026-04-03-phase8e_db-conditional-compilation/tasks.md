# Tasks: Database Library — Conditional Compilation + Feature Flags

**Status**: Complete. 100% (12/12).
**Depends on**: phase8_db-foundation

## Phase 1: Compiler --define Support

- [x] 1.1 Verify --define CLI flag passes symbols to preprocessor — works via `build` command
- [x] 1.2 Test #ifdef CUSTOM_SYMBOL with --define — `-DDB_SQLITE` tested, works
- [x] 1.3 Test #ifdef in module-level code — function definitions conditionally included
- [x] 1.4 Test #ifdef in function bodies — inline #ifdef/#else/#endif works in func bodies
- [x] 1.4b Fix: added -D/--define= support to `run` command (was missing, only build had it)

## Phase 2: Feature Flags

- [x] 2.1 `db/features.tml` — has_sqlite/postgres/mysql/mongodb/redis() compile-time detection
- [x] 2.2 Feature → symbol mapping: DB_SQLITE, DB_POSTGRES, DB_MYSQL, DB_MONGODB, DB_REDIS
- [x] 2.3 default_driver() returns "sqlite" (bundled, always available)
- [x] 2.4 Documentation: usage with `-D` flags, feature composition notes

## Phase 3: db Module Integration

- [x] 3.1 db/mod.tml — `pub mod features` added, #ifdef guards documented for future drivers
- [x] 3.2 `db/driver/registry.tml` — default_driver(), driver_info()
- [x] 3.3 Connection URL routing documented in template (to be implemented per driver)

## Phase 4: External Driver Template

- [x] 4.1 `db/driver/template.tml` — full scaffold with steps 1-4 for new driver creation
- [x] 4.2 Documentation: Connection, PreparedStatement, Dialect impl examples in doc comments
- [x] 4.3 Tests: `db_features.test.tml` — 4 tests, all passing (22/22 std/db suite)
