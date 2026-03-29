# Proposal: MongoDB Driver (lib/mongodb/)

## Why
First NoSQL driver. Separate library with libmongoc/libbson FFI, Document/BSON types, and collection-based API.

## What Changes
- lib/mongodb/ with FFI bindings to libmongoc + libbson
- BSON Document type compatible with std::json
- MongoDriver, MongoConnection, Collection, Cursor
- Integration tests and benchmarks

## Impact
- Affected code: lib/mongodb/ (new separate library)
- Breaking change: NO
- User benefit: MongoDB support with native BSON documents
