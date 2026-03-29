# Tasks: Database Library — Conditional Compilation + Feature Flags

**Status**: Planning. 0% (0/12).
**Depends on**: phase8_db-foundation

## Phase 1: Compiler --define Support

- [ ] 1.1 Verify --define CLI flag passes symbols to preprocessor
- [ ] 1.2 Test #ifdef CUSTOM_SYMBOL with --define
- [ ] 1.3 Test #ifdef in module-level code
- [ ] 1.4 Test #ifdef in function bodies

## Phase 2: Feature Flags in tml.toml

- [ ] 2.1 Parse [features] section in tml.toml
- [ ] 2.2 Feature -> symbol generation (postgres -> DB_POSTGRES)
- [ ] 2.3 Optional dependencies (optional = true)
- [ ] 2.4 Feature composition (all-sql = ["sqlite", "postgres", "mysql"])

## Phase 3: db Module Integration

- [ ] 3.1 Update db/mod.tml with #ifdef DB_POSTGRES / DB_MYSQL etc.
- [ ] 3.2 Driver registry based on available features
- [ ] 3.3 Connection URL routing to correct driver

## Phase 4: External Driver Template

- [ ] 4.1 Create driver scaffold template
- [ ] 4.2 Documentation: How to create a TML database driver
- [ ] 4.3 Tests: conditional compilation end-to-end
