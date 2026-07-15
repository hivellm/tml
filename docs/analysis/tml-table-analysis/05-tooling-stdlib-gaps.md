# 05 — Tooling and Stdlib Gaps

The smallest file in this analysis, deliberately: tooling and stdlib breadth are NOT what
blocks TML today. This file records what remains so it is not forgotten once files 02/03 are
resolved.

## F-010 — No production package registry; dependency management is partial

**Confidence: Medium. Impact: Medium-High.**

The UzDB letter's issue #6 (no registry → reimplement everything) is only partly closed:

- `tml install` + `tml.lock` TML-dependency resolver exists (CHANGELOG 0.3.26–0.3.36).
- But `phase38a_package-manager` / `phase38b_package-manager-alt` are still **pending** in
  `.rulebook/tasks/`.
- There is no crates.io/npm equivalent: no publishing flow, no versioned central index, no
  namespace/ownership model.

For the compiler team this is acceptable (the ecosystem is in-repo). For an external adopter
building a real application, "the ecosystem is you" is a real, compounding cost — every
utility library is a fork-or-rewrite decision.

## Async file I/O maturity (cross-ref F-012)

`lib/std/src/aio/async_file.tml` is explicitly an MVP (CHANGELOG 0.3.26–0.3.36 "async-file
MVP + design doc"). A WAL-driven database needs: guaranteed fsync ordering relative to
buffered writes, robust error propagation on partial writes, and cancellation semantics.
None of this is validated under load today.

## Stdlib strength (context, not a defect)

To be fair to the language — breadth is genuinely impressive:

- `lib/std/src/`: **356 modules** including crypto (RSA/ECDH/X509/HMAC/KDF), HTTP/2,
  WebSocket, TCP/UDP/DNS, SQLite bindings, msgpack, protobuf, zlib/gzip/zstd/brotli, JSON,
  search (BM25/HNSW), SIMD.
- `lib/core/src/`: **200 modules**.

Everything UzDB needed as primitives exists. **Breadth is not the problem; reliability under
the memory model is** (file 02). This is worth stating plainly because it changes the
corrective strategy: the project does not need more surface area, it needs the existing
surface to stop crashing.
