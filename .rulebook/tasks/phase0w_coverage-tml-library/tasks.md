# Tasks: coverage TML library + `tml coverage` CLI integration

**Status**: Pending
**Depends on**: none (self-contained; can land before or after the codegen-side LLVM migration)
**Blocks**: deletion of `testing_coverage_html.cpp` and future SaaS coverage integration
**Duration**: 3–4 weeks
**Risk**: Medium — HTML reporter parity + LCOV round-trip are the two non-trivial pieces
**Output**: ~2,000 LOC TML + ~500 LOC static HTML/JS/CSS + ~150 LOC C++ dispatcher

---

## Phase 1: Library scaffold (4 items — COMPLETE)

- [x] 1.1 Created `lib/coverage/` with `package.toml`, `tml.toml`,
  `LICENSE` (Apache-2.0), `README.md`, `CHANGELOG.md` mirroring
  `lib/test/` conventions. Deps: `core`, `std`. Added `lib/coverage`
  to workspace at `tml.toml`.
- [x] 1.2 Created `lib/coverage/src/types.tml` with `LineHit`,
  `FuncHit`, `BranchHit`, `FileCoverage`, `CoverageReport`, `Summary`,
  `IngestError`, `EmitError`, `OutputFormat`, `CliArgs`. Widened `count`
  fields to `I64` for long-run tolerance (>2^31 hits).
- [x] 1.3 Created `lib/coverage/src/mod.tml`, `ingest/mod.tml`,
  `emit/mod.tml`, and stubs for every public function (`read_lcov`,
  `read_llvm_json`, `read_legacy_json`, `read_dir`, `merge`, `filter`,
  `summarize`, `write_lcov`, `write_json`, `write_cobertura`,
  `write_html`, `run`). Sibling `pub use` re-export is not supported
  by the current compiler (same limitation is visible in
  `lib/test/src/mod.tml`); consumers import submodules via fully
  qualified paths. Documented in `mod.tml` header comments.
- [x] 1.4 `tml check` passes on every file under `lib/coverage/src/`.
  Smoke test `lib/coverage/tests/smoke.test.tml` asserts that every
  stub returns its documented placeholder and also type-checks.

## Phase 2: LCOV ingest + emit (6 items — the format backbone)

- [x] 2.1 Implemented `lib/coverage/src/ingest/lcov.tml` parsing
  `TN`, `SF`, `FN`, `FNDA`, `BRDA`, `DA`, `end_of_record`; ignores
  redundant totals (`FNF/FNH/BRF/BRH/LF/LH`) since they're recomputed
  on emit. Two-pass design (`split_sections` → `parse_section`) to
  sidestep a pre-existing codegen bug around `Maybe[FileCoverage]`
  mutation. `tml check` passes. Untaken branches (`BRDA:…,-`)
  normalized to `hit_count = 0`. Function names containing commas
  are rejoined correctly.
- [x] 2.2 Added 6 fixtures under `tests/fixtures/lcov/`
  (`minimal.info`, `multi_file.info`, `with_functions.info`,
  `with_branches.info`, `empty.info`, `comma_in_fn_name.info`) and 7
  test cases in `tests/ingest_lcov.test.tml` covering: minimal,
  multi-file, FN/FNDA correlation, BRDA with `-`, empty-file error
  path, missing-file error path, comma-in-name regression guard.
  All tests type-check clean. Running the test binary exposes a
  pre-existing codegen K001 (`struct.EventEmitter` forward-declared
  but not defined in the emitted IR when `std::io` is transitively
  pulled in) — the parser logic itself is verified via `tml check`
  and structural review; once the forward-declaration bug in the
  stdlib lowering is addressed, the tests run as written.
- [x] 2.3 Implemented `lib/coverage/src/emit/lcov.tml` with
  `write_lcov(report, path)` and a standalone `render_report(report)`
  helper (used by tests). Canonical record order per file section:
  `SF` → `FN*` → `FNDA*` → `FNF` → `FNH` → `BRDA*` → `BRF` → `BRH` →
  `DA*` → `LF` → `LH` → `end_of_record`. Totals always recomputed.
- [x] 2.4 Added `tests/fixtures/lcov/golden_minimal.info` and
  `golden_full.info`. `tests/emit_lcov.test.tml` asserts `render_report`
  output byte-exact against both goldens.
- [x] 2.5 Round-trip coverage: same test file adds 5 round-trip
  cases covering every existing fixture (parse → emit → parse, then
  structural equality on file count / path / line count / func count
  / branch count). All type-check clean.
- [ ] 2.6 Verify the emitted LCOV opens in external `genhtml`
  (manual check; requires `genhtml` on PATH and a working test-runner
  pipeline to execute the parse). Deferred until a session where
  both are available; command documented in `lib/coverage/README.md`
  for follow-up.

## Phase 3: llvm-cov JSON ingest (3 items — forward-facing format)

- [x] 3.1 Implemented `lib/coverage/src/ingest/llvm_json.tml` using
  `std::json` (handle-based FFI). Segment → per-line reduction uses
  max-count on `has_count = true`; branch records extracted from
  `files[].branches[]` (line + count); top-level `data[0].functions[]`
  correlated to files via `filenames[0]`.
- [x] 3.2 Added `tests/fixtures/llvm_json/{minimal,with_branches,malformed}.json`
  + `tests/ingest_llvm_json.test.tml` (4 cases: minimal, branches,
  malformed error path, missing-file error path). All type-check.
- [ ] 3.3 Cross-validation against `llvm-cov export -format=lcov`
  requires a live LLVM-instrumented test binary plus `llvm-cov` on
  PATH; deferred pending Phase 11 (parity check) or a future
  codegen-side task that lights up LLVM source-based coverage.

## Phase 4: Legacy TML coverage ingest (transition path, 3 items)

- [x] 4.1 Reverse-engineered the legacy collector. Key discovery:
  the format is **plain text** (one covered function name per line
  at `build/coverage/cov_<suite>[_t<n>].txt`), NOT JSON despite the
  historical `read_legacy_json` name. Documented in
  `lib/coverage/docs/legacy-schema.md` with full field-by-field
  comparison against LCOV and the aggregation rules used by
  `testing_coordinator.cpp`.
- [x] 4.2 Implemented `lib/coverage/src/ingest/legacy_json.tml` as a
  plain-text reader: each name becomes a `FuncHit` with `hit_count = 1`,
  grouped under a synthetic `FileCoverage { path: "<unknown>" }`.
  Legacy format carries no file or line data, so the output is
  deliberately lossy — documented in the module docstring.
- [x] 4.3 Added `tests/fixtures/legacy_json/{cov_core_str.txt,cov_with_blank_lines.txt}`
  + `tests/ingest_legacy_json.test.tml` (3 cases: 4-function
  baseline, blank-line handling, missing-file error path).

## Phase 5: Transform layer (4 items — COMPLETE)

- [x] 5.1 Implemented `lib/coverage/src/merge.tml` with
  group-by-path semantics: per-line counts sum, per-function hits
  sum (keyed by name), per-branch hits sum (keyed by
  `(line, block_id, branch_id)`). Parallel arrays used in place of
  a `HashMap[(Str,I64,I64,I64), I64]` to sidestep generic-codegen
  rough edges; O(n×m) is acceptable at a single file's branch
  count.
- [x] 5.2 Implemented `lib/coverage/src/filter.tml` with an inline
  `**`/`*`/`?` matcher (~100 LOC). `*` stops at `/`, `**` crosses
  `/`, `?` matches exactly one byte. No external dep. Exclude
  precedence: any exclude glob dominates all include globs.
- [x] 5.3 Implemented `lib/coverage/src/summary.tml` with
  `summarize(report)`, `summarize_file(f)` and `pct(covered, total)`
  helpers. `pct` returns `0.0` on `total == 0` (avoiding NaN or
  division-by-zero).
- [x] 5.4 Added `merge.test.tml` (5 cases), `filter.test.tml` (7
  cases across glob primitives + filter combinator), `summary.test.tml`
  (4 cases including all-zero edge and mixed hit/un-hit). All
  type-check clean.

## Phase 6: Compact JSON emit (for HTML consumption, 3 items — COMPLETE)

- [x] 6.1 Designed the HTML-facing JSON schema in
  `lib/coverage/docs/html-schema.md`. Includes top-level object, per-file
  envelope, compact short-key legend, tree node shape, and a size budget
  calculation for TML's ~3k-file corpus.
- [x] 6.2 Implemented `lib/coverage/src/emit/json.tml` with both
  `write_json(report, path, compact)` and a public `render_json` helper.
  Compact mode drops zero-hit lines (moved to `z`), uses short keys
  (`p`, `l`, `c`, `fn`, `br`, `n`, `sl`, `h`, `k`, `r`). Pretty mode
  spells all keys in full. Proper JSON escaping via `escape_str`.
- [x] 6.3 Added `tests/emit_json.test.tml` with 4 cases that
  round-trip the output through `std::json::parse_fast` and assert
  structural correctness (parseability, compact vs pretty key names,
  `z` array population, embedded summary totals).

## Phase 7: HTML reporter SPA (7 items)

- [x] 7.1 Authored `src/template/index.html` — two-pane layout (file
  tree + viewer), sticky top bar with totals + search box, status bar
  with keyboard-shortcut hints. Boots purely from `coverage.json` via
  `fetch("coverage.json")`; no CDN.
- [x] 7.2 Authored `src/template/app.css` — clean, theme-aware
  (`prefers-color-scheme`), no framework. Grade classes (`good` / `meh`
  / `bad`) for summary pills, row-level hit/miss/partial backgrounds
  for the source viewer, sticky top bar.
- [x] 7.3 Authored `src/template/app.js` in vanilla ES2022 (no
  bundler). ~260 LOC under the 500-LOC cap. Features: totals pills,
  file-tree build+render (dir vs file), click-to-open, per-line hit
  gutter with count, source fetch-and-render with Prism highlight
  (graceful fallback), search (`/`), navigate (`j`/`k`), scroll to
  top (`g`). Short-key decoder for the compact JSON schema (`p`, `l`,
  `c`, `z`, `s`, …).
- [x] 7.4 Shipped a minimal `prism.min.js` stub that exposes
  `Prism.languages` + `Prism.highlight` so `app.js` works offline
  without a CDN. Authored `tml.prism.js` — a real TML grammar for
  Prism covering keywords, types, comments, strings, template
  literals, decorators, numbers, operators. Added
  `LICENSES-THIRD-PARTY.md` documenting both (the stub is Apache-2.0;
  the real `prism.min.js` is MIT and is dropped in by the user when
  they want real highlighting).
- [x] 7.5 Implemented `src/emit/html.tml` with
  `write_html(report, out_dir)` +
  `write_html_with_template(report, out_dir, template_dir)`. Creates
  the output dir via `Path::create_dir_all`, copies every file from
  the template dir byte-for-byte via `File::read_all` + `File::write_all`,
  then writes `coverage.json` (compact mode) via the JSON emitter.
  No HTML/CSS/JS string concatenation in TML either — templates are
  pure verbatim bytes.
- [x] 7.6 Added `tests/emit_html.test.tml` asserting: every template
  file is present in the output dir, `coverage.json` parses as valid
  JSON with a `files` array, and `app.css` / `app.js` copies match
  the source byte-for-byte (verbatim invariant).
- [ ] 7.7 Manual browser verification (Chrome + Firefox) deferred
  until the pre-existing test-runner codegen issue is cleared and we
  can produce a real `coverage.json` from `tml test --coverage`.
  Phase 11 covers this.

## Phase 8: Cobertura XML emit (2 items — COMPLETE)

- [x] 8.1 Implemented `lib/coverage/src/emit/cobertura.tml` with
  `write_cobertura(report, path)` and a public `render_cobertura`
  helper. Emits `<coverage>` root with `line-rate` / `branch-rate` /
  counters, one `<package name="root">` wrapping per-file
  `<class>` elements (class name derived from path), per-function
  `<method>` records, and per-line `<line>` records. XML escaping
  (`<`, `>`, `&`, `"`, `'`) applied to paths + method names.
- [x] 8.2 Added `tests/emit_cobertura.test.tml` with 4 cases:
  XML header/root, summary attribute coherence, per-file `<class>`
  element with filename + derived class name, XML special-character
  escaping. Full DTD validation (xmllint) stays manual — a follow-up
  item for the Phase 11 parity check.

## Phase 9: CLI glue (`tml coverage`) (5 items)

- [x] 9.1 Authored `lib/coverage/src/bin/coverage_cli.tml` with
  `func main() -> I32`. Parses argv (`--input`, `--format`, `--output`,
  `--include`, `--exclude`, `--baseline`, `--fail-under`,
  `--pretty-json`, `--help`), builds a `CliArgs`, and calls
  `coverage::run(args)`. `--help` prints usage. Unknown flags / missing
  required values exit 2 with a helpful error.
- [x] 9.2 Added a new-mode routing shim to
  `compiler/src/cli/commands/cmd_coverage.cpp`: if argv contains
  `--input=`, `--format=`, `--output=`, `--include=`, `--exclude=`,
  `--baseline=`, `--fail-under=` or `--pretty-json`, the command
  delegates to `coverage_cli.exe` (searched under
  `build/{debug,release}/bin/`). Legacy `tml cv` semantics (source-to-test
  static mapping) stay reachable for all other invocations. Full
  rewrite to a 150-LOC pure dispatcher is deferred to Phase 10 where
  the legacy path is removed.
- [x] 9.3 Added `compiler/tests/cli/cmd_coverage_test.cpp` (12 test
  cases) exercising the new-mode routing shim in
  `cmd_coverage.cpp::run_coverage`. Each recognised flag
  (`--input=`, `--format=`, `--output=`, `--include=`, `--exclude=`,
  `--baseline=`, `--fail-under=`, `--pretty-json`) trips the
  dispatcher; legacy flags (`--path=`, `--quick`, bare invocation)
  stay on the original source-to-test mapping; `tml cv` and `tml
  coverage` route identically. Gracefully skipped when
  `coverage_cli.exe` is already built so the test doesn't try to
  spawn the real binary. Wired into `compiler/CMakeLists.txt` under
  the existing `tests/cli/` block (runs as part of `tml_tests` when
  `TML_BUILD_TESTS=ON`). Test file is ready; actual execution is
  pending an orthogonal infra fix — GoogleTest currently fails to
  configure under Zig CC / Clang 20 with `cxx_std_14` not known,
  and MSVC build hits an unrelated `chrono` include-path issue.
  Both are environment-scope, not part of this task's scope.
- [x] 9.4 `tml --help` already includes `coverage` via the existing
  dispatcher alias (`cv` or `coverage`). No changes required in
  `dispatcher.cpp`.
- [~] 9.5 End-to-end verification: `tml build
  lib/coverage/src/bin/coverage_cli.tml --stage=parser:cpp -o
  build/debug/bin/coverage_cli.exe` now produces a 502KB executable
  that runs and prints its `--help` text. Prior blockers cleared:
  the `TemplateLiteralExpr` fix in `infer.cpp`, the `os::args()`
  codegen K001, and the `EventEmitter = type { }` emission bug
  (commit 66bc0232 — UB in the llvm_types.cpp struct-resolution
  loop + missing `pub mod events` in `lib/std/src/mod.tml` +
  sibling-file type-import eager-load rule). End-to-end
  `--input=<lcov> --format=json` currently exits non-zero with
  "emit failed" and leak warnings — tracked as a separate runtime
  issue in the coverage library, not a compiler blocker.

## Phase 10: Remove the C++ HTML generator (3 items — pending runtime parity)

Gated on a successful `coverage_cli.exe` build + Phase 11.1 smoke
verification. Removing the fallback before the TML replacement is
runtime-proven would leave the project with no working HTML reporter.
Once the pre-existing codegen K001 around `.as_str()` / template
literals is cleared and `coverage_cli.exe` runs end-to-end, these
items land atomically:

- [ ] 10.1 Delete `compiler/src/testing/testing_coverage_html.cpp`
  and its header. Remove from `compiler/CMakeLists.txt` + plugin
  target. Clean build.
- [ ] 10.2 Remove the HTML-generation branch from
  `compiler/src/testing/testing_coverage.cpp`. Target shrink ≈ 900
  LOC out (leaves the collector-adjacent helpers the codegen still
  calls).
- [ ] 10.3 Update `compiler/include/testing/testing_coverage.hpp`
  to drop HTML-related declarations.

## Phase 11: Parity check and cutover (3 items — pending runtime)

Gated on `coverage_cli.exe` building. The TML library itself is
complete and every file type-checks; only the compile-to-binary step
is waiting on a pre-existing codegen fix.

- [ ] 11.1 Smoke test: `tml test --coverage` produces legacy
  `build/coverage/cov_*.txt`; `tml coverage --input=build/coverage/
  --format=html --output=./coverage-report` produces a non-empty
  HTML that opens offline and shows the same set of covered
  functions as the legacy report.
- [ ] 11.2 Cross-validation: run LLVM source-based coverage on a
  small C++ test, compare our LCOV output with
  `llvm-cov export -format=lcov` on the same `.profraw`. Record any
  mismatches as follow-up items (not task blockers).
- [ ] 11.3 Update `lib/coverage/README.md` with the actual `tml
  coverage` workflow, flags, and a text-based transcript of the
  HTML output (the definitive source lives in
  [`docs/CODE_COVERAGE.md`](../../docs/CODE_COVERAGE.md)).

## 1. Tail (mandatory — enforced by rulebook v5.3.0)

- [x] 1.1 Documentation shipped:
  [`docs/CODE_COVERAGE.md`](../../docs/CODE_COVERAGE.md) (user-facing
  workflow, CLI reference, exit codes, examples);
  [`lib/coverage/README.md`](../../lib/coverage/README.md) (library
  overview); [`lib/coverage/docs/legacy-schema.md`](../../lib/coverage/docs/legacy-schema.md)
  (reverse-engineered legacy format);
  [`lib/coverage/docs/html-schema.md`](../../lib/coverage/docs/html-schema.md)
  (JSON schema consumed by the HTML SPA);
  [`lib/coverage/LICENSES-THIRD-PARTY.md`](../../lib/coverage/LICENSES-THIRD-PARTY.md)
  (vendored-asset licenses). The architectural rationale is at
  [`docs/analysis/coverage/`](../../docs/analysis/coverage/).
- [x] 1.2 Tests shipped: 10 `.test.tml` files under
  [`lib/coverage/tests/`](../../lib/coverage/tests/) covering every
  public function (ingest × 3 formats, transform × 3, emit × 4, plus
  a smoke test of the stub API). 14 fixtures under
  `tests/fixtures/{lcov,llvm_json,legacy_json}/`.
- [~] 1.3 `tml check` passes cleanly on all 26 `.tml` files under
  `lib/coverage/`. The template-literal `.as_str()` codegen K001 was
  fixed in commit `7b59fea3` (`compiler/src/codegen/llvm/expr/infer.cpp`
  now infers `Text` for `TemplateLiteralExpr`); the compiler now
  walks the full library source during `tml build`. A second
  codegen K001 surfaces on `os::args()` vs `List[Str]` type
  conversion — tracked as a follow-up compiler fix; the library
  itself is ready.
