# TML Coverage Library

Pure-TML reporting layer for code coverage data. Ingests LCOV,
`llvm-cov export` JSON and the legacy `tml test --coverage` JSON
format; emits LCOV, Cobertura XML, compact JSON for the HTML viewer,
and a static HTML SPA that works offline via `file://`.

Designed to replace the 3,350 LOC of C/C++ coverage code in
`compiler/src/testing/testing_coverage*.cpp` and
`lib/test/runtime/coverage.c` — without requiring recompilation of
the TML compiler to iterate on the report UI.

## Status

- `0.1.0` — scaffold only. API surface declared, no implementations
  yet. Tracked in rulebook task `phase0w_coverage-tml-library`.

## Public API (work in progress)

```tml
use coverage::*

// Ingest
let report = read_lcov("build/coverage/run.info")?
let report2 = read_llvm_json("build/coverage/run.json")?
let legacy = read_legacy_json(".sandbox/coverage-core.json")?

// Transform
let merged = merge([report, report2, legacy])
let filtered = filter(merged, include: ["lib/**", "compiler/src/**"], exclude: ["**/tests/**"])
let summary = summarize(filtered)

// Emit
write_lcov(filtered, "out/coverage.lcov")?
write_json(filtered, "out/coverage.json", compact: true)?
write_html(filtered, "out/")?
write_cobertura(filtered, "out/coverage.xml")?
```

## CLI

Invoked via `tml coverage` once the C++ dispatcher lands (phase 9):

```
tml coverage --input=<path-or-glob> --format=lcov|json|html|cobertura|all --output=<dir>
```

## Architecture

```
.lcov / .json / .sandbox/coverage-*.json
             │
             ▼
   ingest/ (lcov, llvm_json, legacy_json)
             │
             ▼
     CoverageReport (types.tml)
             │
  merge → filter → summarize
             │
             ▼
      emit/ (lcov, json, html, cobertura)
             │
             ▼
   out/coverage.lcov / coverage.json / index.html / coverage.xml
```

## License

Apache-2.0. See `LICENSE`.

## Third-party

Vendored under `src/template/` once phase 7 lands:

- `prism.min.js` — Prism syntax highlighter (MIT)

Recorded in `LICENSES-THIRD-PARTY.md` when it ships.
