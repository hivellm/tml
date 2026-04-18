# Coverage library — technical spec

## ADDED Requirements

### Requirement: library package identity

The system SHALL ship a new TML library at `lib/coverage/` with
`package.toml` declaring `name = "coverage"`, `version = "0.1.0"`,
`edition = "2025"`, `license = "Apache-2.0"`, and dependencies on
`core` and `std` only.

#### Scenario: package resolves

Given `tml coverage --help` is invoked from a workspace checkout
When the CLI dispatcher resolves the `coverage` module
Then resolution succeeds without requiring any path outside `lib/coverage/`
And no dependency is introduced on `lib/test/` or any C/C++ artifact.

---

### Requirement: public ingest API

The module SHALL expose four ingest entry points returning
`Outcome[CoverageReport, IngestError]`:

- `read_lcov(path: Text)`
- `read_llvm_json(path: Text)`
- `read_legacy_json(path: Text)`
- `read_dir(path: Text)` — dispatches by file extension and magic bytes

#### Scenario: LCOV round-trip preserves report

Given a valid LCOV `.info` fixture at `tests/fixtures/lcov/full.info`
When the caller does `write_lcov(read_lcov(fixture)?, out)?`
And reads the output back with `read_lcov(out)?`
Then the second parse MUST equal the first `CoverageReport` (field-by-field,
including line order, branch order and function order).

#### Scenario: llvm-cov JSON ingest matches reference

Given a `llvm-cov export -format=text` JSON captured from a real C++ test
When `read_llvm_json` ingests it
Then the resulting per-line hit counts MUST match
`llvm-cov show -line-coverage` output for the same inputs
(tolerance: zero — hit counts must be identical).

#### Scenario: legacy JSON ingest is lossy but consistent

Given a `.sandbox/coverage-*.json` fixture produced by the current collector
When `read_legacy_json` ingests it
Then every function marked covered MUST contribute line hits of `1` for
every declared line in that function
And every function marked not covered MUST contribute line hits of `0`
And branch coverage MUST report zero-covered-zero-total across the report.

---

### Requirement: merge semantics

The function `merge(reports: List[CoverageReport]) -> CoverageReport`
SHALL sum per-line hit counts, sum per-function hit counts, and
union branch records keyed by `(file, line, block_id, branch_id)`.

#### Scenario: merging two disjoint runs sums hits

Given report A covers `file1.tml:3` with hit count 5
And report B covers `file1.tml:3` with hit count 2
When the caller calls `merge([A, B])`
Then the resulting report MUST record `file1.tml:3` with hit count 7.

#### Scenario: merging preserves source_roots union

Given report A has `source_roots = ["lib/core"]`
And report B has `source_roots = ["compiler/src"]`
When the caller calls `merge([A, B])`
Then the result MUST have `source_roots = ["lib/core", "compiler/src"]`
(order preserving, deduplicated).

---

### Requirement: LCOV emit conforms to geninfo

The function `write_lcov(report, path)` SHALL produce output that
`geninfo`/`genhtml` (the reference tool) accepts without warnings.

#### Scenario: external genhtml consumes output

Given a non-empty `CoverageReport`
When `write_lcov` writes `out.info`
And the test invokes `genhtml --quiet --no-prolog -o html/ out.info`
Then `genhtml` MUST exit with status 0
And `html/index.html` MUST exist and be non-empty.

(This scenario is executed in CI when `genhtml` is present; otherwise
it is skipped with a documented reason.)

---

### Requirement: HTML emit is template-based

The function `write_html(report, out_dir)` MUST NOT construct HTML,
CSS or JavaScript via string concatenation. It MUST copy every file
from `lib/coverage/src/template/` into `out_dir` byte-for-byte and
write a single `coverage.json` data file consumed by the SPA at
runtime.

#### Scenario: template files are copied verbatim

Given the template `src/template/app.css` is 1234 bytes
When `write_html(report, "out/")` runs
Then `out/app.css` MUST be exactly 1234 bytes
And its SHA-256 MUST equal the template's SHA-256.

#### Scenario: HTML works offline

Given the output directory from `write_html`
When the user opens `out/index.html` via `file://`
Then the file tree, source view, search, and summary pane MUST render
Without any network request (verified by running Chrome headless with
`--disable-network`).

---

### Requirement: compact JSON schema

The JSON emitter MUST produce output under 5 MB for the TML project's
full coverage dataset (~3,000 files) in compact mode.

#### Scenario: compact mode drops zero-hit lines

Given a file with 1000 lines of which 300 were never executed
When emitted in compact mode
Then the output MUST NOT include `line` entries with `count = 0`
And the file MUST carry a `zero_lines: List[I64]` array listing them.

#### Scenario: full mode preserves all lines

Given the same file
When emitted in full (non-compact) mode
Then every reachable line MUST appear in the output array.

---

### Requirement: CLI contract

The binary `coverage_cli.exe` — produced from
`lib/coverage/src/bin/coverage_cli.tml` — MUST accept the following
argv and nothing else:

- `--input=<path-or-glob>` (required, repeatable)
- `--format=lcov|json|html|cobertura|all` (required)
- `--output=<dir>` (default `./coverage-report`)
- `--include=<glob>` (repeatable, default `[]`)
- `--exclude=<glob>` (repeatable, default `[]`)
- `--baseline=<path>` (optional)
- `--fail-under=<pct>` (optional, 0–100)
- `--help`

Unknown flags MUST exit with status 2 and print a helpful error.

#### Scenario: `--fail-under` exits non-zero on low coverage

Given a report whose overall line coverage is 40%
When the user runs `coverage_cli --fail-under=80 --input=… --format=lcov --output=…`
Then the binary MUST write the report to disk
And exit with status 2.

#### Scenario: `--format=all` produces every artifact

Given a valid input
When `--format=all --output=out/` is used
Then `out/coverage.lcov`, `out/coverage.json`, `out/coverage.xml` (Cobertura),
`out/index.html`, `out/app.css`, `out/app.js`, `out/prism.min.js`,
`out/tml.prism.js` MUST all exist and be non-empty.

---

### Requirement: test dispatcher wires `tml coverage` to the library

The C++ CLI dispatcher SHALL resolve `tml coverage` by locating the
cached `coverage_cli.exe` (or building it on first call) and
forwarding argv, stdin, stdout, stderr, and the process exit code.

#### Scenario: dispatcher forwards exit code

Given `coverage_cli.exe` exits with status 2
When the user runs `tml coverage …` and the child returns 2
Then `tml.exe` MUST exit with status 2 (not re-mapped, not swallowed).

---

### Requirement: removal of the C++ HTML generator

After this task lands, `compiler/src/testing/testing_coverage_html.cpp`
MUST NOT exist in the repository. The `tml_compiler` plugin MUST
build without linking an HTML generator.

#### Scenario: no HTML generator in the plugin

Given a fresh build of `tml_compiler_plugin`
When the build completes
Then `nm`/`dumpbin` of the plugin MUST NOT list any symbol containing
`coverage_html` or `emit_html_report` from the C++ side
(only the TML-side library provides HTML emission).

---

### Requirement: backward-compatible input ingest

During the transition period (while the codegen still produces
legacy JSON), the library MUST continue to accept the legacy format
so the new reporter remains usable.

#### Scenario: legacy JSON → LCOV produces non-empty output

Given a real `.sandbox/coverage-*.json` captured from a live run of
`tml test --coverage`
When the user runs `tml coverage --input=.sandbox/ --format=lcov
--output=./out`
Then `out/coverage.lcov` MUST be non-empty
And MUST contain at least one `SF:` record per covered file.

---

## Non-goals

The following are explicitly out of scope for this task:

- Emitting `llvm.instrprof.increment` from TML codegen (separate task)
- Linking `compiler-rt` profile runtime into test binaries (separate task)
- `.profraw` → `.profdata` merging inside the library (shell out to
  `llvm-profdata` from the CLI instead)
- MC/DC coverage support (depends on codegen work)
- SaaS uploader (Codecov / Coveralls integration is a workflow
  concern, not a library concern)
- Historical trend tracking (would require a database)
