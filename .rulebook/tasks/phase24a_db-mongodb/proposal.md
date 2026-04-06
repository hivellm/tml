# Proposal: MongoDB Driver (lib/mongodb/)

**Task**: phase8g_db-mongodb
**Status**: Planning (0/16)
**Priority**: P2
**Estimated effort**: 3–5 days
**Risk**: Medium

## Problem

TML covers relational databases (SQLite, PostgreSQL, MySQL) but has no NoSQL driver.
MongoDB is the leading document database and is required by applications that store
flexible JSON-like records, time-series events, or hierarchical data without a fixed
schema. Without a driver, TML cannot target a large class of backend services that use
MongoDB as their primary store.

## Proposed Solution

A standalone `lib/mongodb/` library built on libmongoc and libbson FFI. The driver
exposes a `Collection[T]` type for CRUD operations, a `Cursor[T]` that implements the
`Iterator` behavior for streaming results, and a `BsonValue` enum for type-safe document
construction. `MongoConnection` implements `std::db::Connection` so the driver integrates
with the existing database abstraction layer. A `Document` builder API enables fluent
construction of filter/update/pipeline documents without raw string manipulation.

## Key Decisions

- **FFI to libmongoc/libbson, not pure wire protocol** — libmongoc handles server
  discovery, replica set monitoring, connection pooling, and authentication (SCRAM,
  X.509). Implementing those in TML would take months and duplicate battle-tested code.
- **BsonValue enum for type safety** — variants for Null, Bool, I32, I64, F64, Text,
  Binary, ObjectId, Datetime, Array, and Document. Avoids stringly-typed documents.
- **Document builder pattern** — `Document::new().set("name", BsonValue::Text("Alice"))`
  is readable and avoids raw BSON serialization in user code.
- **Cursor implements Iterator[T]** — allows `loop (row in cursor) { ... }` with lazy
  deserialization; documents are decoded from BSON only when the iterator advances.
- **Aggregation pipeline support** — `collection.aggregate(pipeline)` returns a
  `Cursor[Document]` for complex multi-stage queries.

## Files to Create/Modify

- `lib/mongodb/src/ffi.tml` — `@extern("c")` bindings for libmongoc and libbson
- `lib/mongodb/src/bson.tml` — BsonValue enum and serialization helpers
- `lib/mongodb/src/document.tml` — Document builder and BSON document wrapper
- `lib/mongodb/src/driver.tml` — MongoDriver implementing Driver behavior
- `lib/mongodb/src/connection.tml` — MongoConnection implementing Connection behavior
- `lib/mongodb/src/collection.tml` — Collection[T] with find/insert/update/delete/aggregate
- `lib/mongodb/src/cursor.tml` — Cursor[T] implementing Iterator behavior
- `lib/mongodb/src/mod.tml` — public re-exports
- `lib/mongodb/build.tml` — libmongoc/libbson discovery and linking
- `lib/mongodb/package.toml` — package metadata and native-lib declaration

## Success Criteria

- All 16 checklist items marked done
- CRUD round-trip: insert document, find by filter, update, delete
- `cursor` iteration yields all matching documents without loading all into memory
- Aggregation pipeline `$match` + `$group` + `$sort` returns correct results
- BSON ObjectId serializes/deserializes correctly across insert and find
- No memory leaks: libmongoc objects (client, collection, cursor) freed on drop
- Integration tests pass against a running mongod instance

## Dependencies

- **Depends on**: std::db abstraction (phase8d, complete), libmongoc ≥ 1.24 and
  libbson ≥ 1.24 headers and binaries available in build environment
- **Blocks**: applications that require MongoDB as a data store; future ODM (object
  document mapper) layer on top of this driver
