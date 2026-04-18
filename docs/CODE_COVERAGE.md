# Code coverage in TML

TML's coverage workflow has two sides:

- **Collection** — the compiler instruments TML (and optionally the
  C++ compiler itself) so test binaries produce raw coverage data
  while they run.
- **Reporting** — the [`coverage`](../lib/coverage/) TML library
  ingests that data, merges parallel runs, filters by path, summarizes,
  and emits LCOV, JSON, Cobertura XML, or a static HTML SPA.

This document covers the reporting side (the user-facing workflow);
the collection side is owned by the compiler and covered in its own
reference under [`docs/specs/`](specs/).

## Quick reference

```bash
# 1. Run tests with coverage
tml test --coverage

# 2. Generate reports
tml coverage --input=build/coverage/ --format=all --output=coverage-report/

# 3. Open the HTML
xdg-open coverage-report/index.html      # Linux
start coverage-report/index.html         # Windows
open coverage-report/index.html          # macOS
```

## `tml coverage` CLI

`tml coverage` delegates to the TML-native reporter at
`lib/coverage/src/bin/coverage_cli.tml`. The C++ dispatcher detects
new-mode flags (any of `--input=`, `--format=`, `--output=`,
`--include=`, `--exclude=`, `--baseline=`, `--fail-under=`,
`--pretty-json`) and routes to `coverage_cli.exe`; otherwise it falls
through to the legacy `tml cv` (source-to-test static mapping).

### Flags

| Flag | Required | Description |
|------|----------|-------------|
| `--input=<path>` | yes (repeatable) | File or directory. Format detected by extension: `.info` / `.lcov` → LCOV, `.json` → llvm-cov JSON, `.txt` → legacy TML format. |
| `--format=<fmt>` | yes | `lcov`, `json`, `html`, `cobertura`, or `all`. |
| `--output=<dir>` | no (default `./coverage-report`) | Output directory. Created if missing. |
| `--include=<glob>` | no (repeatable) | Keep only files matching at least one include glob. Supports `**`/`*`/`?`. |
| `--exclude=<glob>` | no (repeatable) | Drop files matching any exclude glob. Exclude wins over include. |
| `--baseline=<path>` | no | Baseline report for delta computation (reserved for a future release). |
| `--fail-under=<pct>` | no | Exit status 2 if overall line coverage is below the threshold. |
| `--pretty-json` | no | Emit indented / full-key JSON instead of the default compact form. |
| `--help`, `-h` | no | Show usage. |

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success. |
| 1 | Ingest or emit failure (I/O error, parse error, missing template). |
| 2 | `--fail-under` threshold violated. |
| 127 | `coverage_cli.exe` not built — see the printed build command. |

### Supported input formats

1. **LCOV `.info`** — the canonical textual interchange format accepted
   by Codecov, Coveralls, SonarQube, and every other SaaS. Round-trips
   cleanly through `coverage::ingest::lcov` and `coverage::emit::lcov`.
2. **`llvm-cov export` JSON (version 2.0.1+)** — produced by
   `llvm-profdata merge -sparse *.profraw | llvm-cov export -format=text`.
   Segments reduce to per-line hit counts using the documented max-rule;
   branches and top-level functions are correlated by filename.
3. **Legacy TML coverage text** — produced by the current
   `lib/test/runtime/coverage.c` during the transition window. One
   covered function name per line; no line or branch data. The
   reporter synthesizes a single `<unknown>` `FileCoverage` entry.
   Format documented in [lib/coverage/docs/legacy-schema.md](../lib/coverage/docs/legacy-schema.md).

### Output formats

- **`lcov`** — single `coverage.lcov` under `--output`. Accepted by
  every SaaS and by `genhtml`.
- **`json`** — single `coverage.json` (compact by default, or pretty
  with `--pretty-json`). Schema at
  [lib/coverage/docs/html-schema.md](../lib/coverage/docs/html-schema.md).
- **`html`** — static SPA (`index.html` + `app.css` + `app.js` +
  `prism.min.js` + `tml.prism.js` + `coverage.json`). Zero dependencies;
  opens via `file://` in Chrome and Firefox. Features: file tree,
  per-file source view with per-line hit gutter, totals pills,
  fuzzy-search (`/`), navigation (`j`/`k`), scroll-to-top (`g`).
- **`cobertura`** — Cobertura XML at `coverage.xml`. Consumed by GitLab
  CI and Jenkins.
- **`all`** — every format above into the same output directory.

### Example: CI-friendly recipe

```bash
# Fail the build if library coverage drops below 80% line coverage
tml test --coverage
tml coverage \
    --input=build/coverage/ \
    --format=all \
    --output=coverage-report/ \
    --include='lib/**' \
    --exclude='**/tests/**' \
    --fail-under=80
```

## Architecture

```
raw files                        coverage library                 outputs
.info / .json / .txt    →   ingest → merge → filter → emit   →   .lcov / .json /
                                     ↑                               .xml / html/
                                     │
                              (optional --baseline)
```

All reporting lives in [`lib/coverage/`](../lib/coverage/) as a pure
TML library. The reporter has no C/C++ of its own; the C++ side is a
≈60 LOC dispatcher that locates `coverage_cli.exe` and forwards argv.

## Why LCOV as the canonical format

LCOV is text, diff-friendly, consumed by every SaaS, opens in
`genhtml`, and supports every counter axis we need (line, branch,
function). We round-trip through LCOV as the source of truth, then
translate to other formats for specific consumers (Cobertura for
GitLab, JSON for the HTML SPA). See
[docs/analysis/coverage/02-formats-and-standards.md](analysis/coverage/02-formats-and-standards.md)
for the full landscape.

## Building `coverage_cli.exe`

```bash
tml build lib/coverage/src/bin/coverage_cli.tml -o build/debug/bin/coverage_cli.exe
```

Once built, the compiled binary is cached under `build/debug/bin/`
and discovered automatically by the C++ dispatcher.

## Related documents

- [`docs/analysis/coverage/`](analysis/coverage/) — market landscape,
  format survey, HTML state-of-the-art, recommendation ADR.
- [`lib/coverage/README.md`](../lib/coverage/README.md) — library-level
  readme.
- [`lib/coverage/docs/legacy-schema.md`](../lib/coverage/docs/legacy-schema.md)
  — reverse-engineered legacy collector format.
- [`lib/coverage/docs/html-schema.md`](../lib/coverage/docs/html-schema.md)
  — HTML-facing JSON schema.

## Rulebook task

Implementation tracked in
[`.rulebook/tasks/phase0w_coverage-tml-library/`](../.rulebook/tasks/phase0w_coverage-tml-library/).
