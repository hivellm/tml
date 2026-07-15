# TML Coverage Library

Pure-TML reporting layer for code coverage data. Ingests LCOV,
`llvm-cov export` JSON, and the legacy `tml test --coverage` plain-text
format; emits LCOV, Cobertura XML, compact JSON, and a static HTML SPA
that works offline via `file://`.

Replaces the 3,350 LOC of C/C++ coverage code in
`compiler/src/testing/testing_coverage*.cpp` and
`lib/test/runtime/coverage.c` — iterating on the report UI no longer
requires recompiling the compiler.

## Status

- `0.1.0` — all phases complete: ingest (LCOV, llvm-cov JSON, legacy
  text), transform (merge, filter, summarize), emit (LCOV, JSON,
  Cobertura XML, HTML SPA). Full end-to-end: `coverage_cli
  --format=all` materialises every artefact in a fresh output
  directory and exits 0.

## CLI

The compiled binary lives at `build/{debug,release}/bin/coverage_cli.exe`
(produced by `tml build lib/coverage/src/bin/coverage_cli.tml -o ...`).
It is also reachable via the C++ dispatcher under `tml coverage`:

```
tml coverage --input=<path>     input file or directory (repeatable)
             --format=<fmt>     lcov | json | html | cobertura | all
             --output=<dir>     output directory (default: coverage-report)
             --include=<glob>   include glob (repeatable)
             --exclude=<glob>   exclude glob (repeatable)
             --baseline=<path>  baseline report for delta comparison
             --fail-under=<pct> exit code 2 if coverage below threshold
             --pretty-json      indent the JSON output (default: compact)
             --help, -h         show this help
```

Any invocation with `--input=` or `--format=` routes to this CLI; every
other invocation (`tml cv`, `tml coverage --path=…`, `--quick`, bare)
stays on the legacy source-to-test mapping report.

## Quick examples

```bash
# Ingest one LCOV file, emit LCOV + Cobertura + HTML SPA
coverage_cli --input=run.info --format=all --output=./report

# Filter to library code only, emit compact JSON
coverage_cli --input=run.info --format=json \
             --include="lib/**" --exclude="**/tests/**" \
             --output=./report

# Ingest all files in a directory (format inferred per-file by extension)
coverage_cli --input=./collected --format=lcov --output=./report
```

`coverage_cli --format=all --output=<dir>` produces:

- `coverage.lcov` — LCOV 2.x (SF, FN/FNDA, BRDA, DA, end_of_record)
- `coverage.json` — compact JSON schema consumed by the HTML SPA
- `coverage.xml` — Cobertura 4 DTD
- `index.html`, `app.css`, `app.js`, `prism.min.js`, `tml.prism.js` —
  offline-capable SPA

The SPA works from `file://` — open `index.html` directly in any
modern browser.

## Library API

```tml
use coverage::{run}
use coverage::types::*
use coverage::ingest::lcov::read_lcov
use coverage::ingest::llvm_json::read_llvm_json
use coverage::ingest::legacy_json::read_legacy_json
use coverage::merge::merge
use coverage::filter::filter_paths
use coverage::summary::summarize
use coverage::emit::lcov::write_lcov
use coverage::emit::json::write_json
use coverage::emit::html::write_html
use coverage::emit::cobertura::write_cobertura

// Ingest — returns Outcome[CoverageReport, IngestError]
let r1 = read_lcov("build/coverage/run.info")!
let r2 = read_llvm_json("build/coverage/run.json")!
let r3 = read_legacy_json("build/coverage/cov_mysuite.txt")!

// Transform
let merged = merge(List[CoverageReport]::from([r1, r2, r3]))
let keep = List[Str]::from(["lib/**", "compiler/src/**"])
let drop = List[Str]::from(["**/tests/**"])
let filtered = filter_paths(merged, keep, drop)
let s = summarize(filtered)

// Emit — each returns Outcome[(), EmitError]
write_lcov(filtered, "out/coverage.lcov")!
write_json(filtered, "out/coverage.json", 1 as Bool)!  // compact
write_html(filtered, "out/")!
write_cobertura(filtered, "out/coverage.xml")!
```

The high-level `coverage::run(CliArgs)` used by the CLI binary also
`Path::create_dir_all`s the output directory before writing.

## Architecture

```
.lcov / .info  llvm-cov JSON  cov_*.txt (legacy)
       │              │              │
       ▼              ▼              ▼
ingest/lcov    ingest/llvm_json  ingest/legacy_json
       │              │              │
       └──────────────┼──────────────┘
                      ▼
              CoverageReport (types.tml)
                      │
         merge → filter_paths → summarize
                      │
                      ▼
     emit/lcov  emit/json  emit/cobertura  emit/html
                      │
                      ▼
   out/coverage.lcov  .json  .xml  index.html
```

## Building

```bash
# From project root (produces build/debug/coverage_cli.exe)
tml build lib/coverage/src/bin/coverage_cli.tml -o build/debug/bin/coverage_cli.exe

# Run the type-check-only smoke test
tml test --suite=coverage
```

## Documentation

| Document | Purpose |
|---|---|
| [`docs/CODE_COVERAGE.md`](../../docs/CODE_COVERAGE.md) | User-facing workflow, CLI reference, exit codes |
| [`lib/coverage/docs/legacy-schema.md`](docs/legacy-schema.md) | Reverse-engineered `cov_*.txt` format |
| [`lib/coverage/docs/html-schema.md`](docs/html-schema.md) | Compact JSON schema consumed by the HTML SPA |
| [`lib/coverage/LICENSES-THIRD-PARTY.md`](LICENSES-THIRD-PARTY.md) | Licenses of bundled JS/CSS assets |

## License

Apache-2.0. See [`LICENSE`](LICENSE).

## Third-party assets

Under `src/template/`:

- `prism.min.js` — Prism syntax highlighter (MIT)
- `tml.prism.js` — TML grammar definition for Prism (Apache-2.0)

Full attribution in [`LICENSES-THIRD-PARTY.md`](LICENSES-THIRD-PARTY.md).
