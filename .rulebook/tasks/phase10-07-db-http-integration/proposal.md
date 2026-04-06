# Proposal: Database + HTTP Integration — REST APIs + TechEmpower Benchmarks

**Task**: phase10-07-db-http-integration
**Status**: In Progress — 10% (1/10)
**Priority**: P1
**Estimated effort**: 4–5 days
**Risk**: Medium — TFB compliance requires exact response formats; PostgreSQL integration
depends on phase8f_db-postgres being stable

## Problem

TML currently has no end-to-end examples combining the HTTP server with database access.
This leaves a gap for developers evaluating TML for full-stack applications: there is no
reference to show how routing, request parsing, SQL queries, and JSON serialization compose
together. Additionally, without TechEmpower Framework Benchmarks (TFB) results, TML cannot
be objectively compared against other full-stack languages on industry-standard workloads.

## Proposed Solution

**Phase 1 — REST API examples**: Three progressively complete examples showing TML's full-stack
capabilities. The SQLite CRUD API is done (1.1). The PostgreSQL REST API and blog application
with migrations remain.

**Phase 2 — TechEmpower Benchmarks**: Implement all four TFB test types using TML's
`std::http::App` router and `std::db` abstraction layer. TFB has strict response format
requirements (exact JSON keys, Content-Type headers, sorted fortunes by message text).

**Phase 3 — Cross-language and cross-database comparison**: Run the TFB suite against SQLite,
PostgreSQL, and in-memory backends. Compare TML results against published TFB Round 22 numbers
for Go, Rust, and Node.js. Publish a final report.

## Key Decisions

- Use `std::http::App` for routing (not raw socket handling): consistent with the HTTP server
  design and allows the router's optimization work to benefit these benchmarks.
- Use `std::db` abstraction layer: queries run against any backend without changing application
  code. This enables the cross-database comparison in Phase 3 without rewriting the app.
- TFB fortunes must sort by message: the TFB spec requires fortune rows sorted by message
  text after adding the server-generated "additional fortune" row.
- JSON serialization via `std::json`: avoids manual string building that would introduce
  correctness bugs and make the benchmark unfairly fast (skipping real serialization).

## Files to Create/Modify

- `examples/db/rest_postgres.tml` — CRUD API (GET/POST/PUT/DELETE /api/users) with PostgreSQL
- `examples/db/blog_app.tml` — Blog with posts, comments, migration runner, pagination
- `benchmarks/tfb/single.tml` — TFB single query: SELECT one row, return JSON object
- `benchmarks/tfb/multiple.tml` — TFB multiple queries: N random SELECTs (N from query param)
- `benchmarks/tfb/fortunes.tml` — TFB fortunes: SELECT all, add server fortune, sort, render HTML
- `benchmarks/tfb/updates.tml` — TFB updates: N random SELECT + UPDATE pairs

## Success Criteria

- `rest_postgres.tml` handles all four HTTP verbs, returns correct JSON, passes type-check
- `blog_app.tml` runs migrations on first start, supports pagination (LIMIT/OFFSET)
- All four TFB implementations return responses that pass TFB validation tool
- TFB fortunes response: rows sorted alphabetically by message, HTML table format
- Phase 3 report includes req/s comparison table: TML vs Round 22 Go/Rust/Node.js numbers
- Zero query errors under sustained TFB load (100 concurrent connections, 30s)

## Dependencies

- Depends on: phase8c_db-orm, phase8f_db-postgres, std::http (all existing)
- Depends on: phase10-06-http-benchmark (cross-language comparison reuses benchmark harness)
- Blocks: nothing (leaf task, though report feeds into overall TML performance narrative)
