# Tasks: Database Library — MongoDB Driver (lib/mongodb/)

**Status**: Planning. 0% (0/16).
**Depends on**: phase8_db-foundation, phase8e_db-conditional-compilation

## Phase 1: libmongoc FFI

- [ ] 1.1 mongoc_init, mongoc_cleanup
- [ ] 1.2 mongoc_client_new, mongoc_client_destroy
- [ ] 1.3 Collection ops: insert_one, find, update_one, delete_one
- [ ] 1.4 BSON: bson_new, bson_append_*, bson_as_json, bson_destroy

## Phase 2: BSON Document Type

- [ ] 2.1 BsonValue enum
- [ ] 2.2 Document type (key-value pairs)
- [ ] 2.3 Document builder API

## Phase 3: Driver Implementation

- [ ] 3.1 MongoDriver
- [ ] 3.2 MongoConnection
- [ ] 3.3 Collection (find, insert, update, delete, aggregate)
- [ ] 3.4 Cursor iterator
- [ ] 3.5 mod.tml + tml.toml

## Phase 4: Tests + Benchmarks

- [ ] 4.1 BSON/Document tests
- [ ] 4.2 Integration tests
- [ ] 4.3 MongoDB benchmarks
- [ ] 4.4 SQL vs NoSQL comparison

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
