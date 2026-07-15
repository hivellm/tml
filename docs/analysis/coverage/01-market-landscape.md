# 01 — Market landscape of coverage tools

**Goal**: map, per language, the dominant code-coverage tools in 2026 and identify (i) collection technique, (ii) intermediate format, (iii) HTML quality. This document feeds the decisions in `04-hybrid-cpp-tml-architecture.md` and `05-recommendation.md`.

**Method**: official documentation for each project, complemented by inspection of recent releases known up to the model cutoff (Jan 2026). Items marked `[verify]` require cross-check before being cited as fact.

---

## Taxonomy of collection techniques

Three families with well-known trade-offs:

| Technique | Typical overhead | Granularity | Rebuild needed? | Examples |
|-----------|------------------|-------------|-----------------|----------|
| **Compiler-inserted counters** (source-based) | 5–20% | line, branch, MC/DC | yes | `gcov`, `llvm-cov`, `go test -cover`, JaCoCo offline, rustc `-Cinstrument-coverage` |
| **Bytecode / IR rewriting** | 20–50% | line, branch | no (usually via runner flag) | Istanbul, coverage.py (trace), JaCoCo on-the-fly, Coverlet |
| **Binary rewriting / debugger traps** | 50–500% | line (via line tables/PDB) | no (uses debug info) | OpenCppCoverage, BullseyeCoverage, tarpaulin Linux mode, DynamoRIO drcov |

Compiler-inserted is today the gold standard for compiled languages (Rust, Go, modern C/C++). Post-processing dominates interpreted ecosystems (JS, Python, legacy JVM). Binary rewriting is a last resort when the source/compiler is out of reach.

---

## C / C++

### `gcov` + `lcov` + `genhtml` (GCC)

- **Collection**: `gcc -fprofile-arcs -ftest-coverage` — compiler emits counters (`.gcno` for mapping + `.gcda` written at run time).
- **Technique**: compiler-inserted counters in an **arc-based CFG**; line granularity is derived from the arc→line map.
- **Intermediate format**: `.gcda` (binary, GCC-version-specific) → `gcov` produces `.gcov` (plain text) → `lcov` converts to `.info` (LCOV textual).
- **HTML**: `genhtml coverage.info -o html/` produces a directory with an `index.html` plus one HTML per source file. Classic table layout, green/red line highlighting, branch markers `+`/`-`. It is the reference everyone benchmarks against.
- **Overhead**: typically 10–15%. `.gcda` is flushed via `atexit()`; a crash can corrupt it.
- **Notes**: binary incompatibility across GCC versions; the same GCC that compiled must be available on the report host.

### `llvm-cov` (Clang/LLVM source-based)

- **Collection**: `clang -fprofile-instr-generate -fcoverage-mapping`. Run the binary (produces `default.profraw`) → `llvm-profdata merge -sparse *.profraw -o merged.profdata` → `llvm-cov show/report/export`.
- **Technique**: compiler-inserted counters **region-based** (each structured AST region gets a counter; nested regions subtract). Branch coverage since LLVM 14; **MC/DC** coverage since LLVM 18 (Jan 2024).
- **Intermediate format**: `.profraw` (raw profile) → `.profdata` (LLVM indexed binary) → exportable as `lcov`, `json`, or `text` via `llvm-cov export`.
- **HTML**: `llvm-cov show --format=html --output-dir=...` produces a modern report with directory navigation, inline highlighting, region counters. Since LLVM 15, branch counters are shown inline.
- **Overhead**: comparable to or slightly below gcov; continuous mode (LLVM 13+) allows merging without `atexit`.
- **Notes**: this is the **preferred path** for any project already using LLVM. Profile runtime lives in `compiler-rt/lib/profile`. Official docs: `clang/docs/SourceBasedCodeCoverage.rst`.

### OpenCppCoverage (Windows)

- **Collection**: **no rebuild**. Uses the Windows Debug API (`DebugActiveProcess`) plus PDB parsing to set a hardware breakpoint on every line with code; decrements the breakpoint when hit.
- **Technique**: debugger-driven binary rewriting. Windows-only. Requires PDB (MSVC or clang-cl).
- **Intermediate format**: internal binary; exports to HTML and Cobertura-like XML.
- **HTML**: plain file list + coloured source. Functional but dated. No branch coverage.
- **Overhead**: typically **100–300%** (debug traps are expensive); worse in tight loops.
- **Notes**: fine as a stop-gap for MSVC, does not scale to cross-platform pipelines. No native LCOV (converts to Cobertura XML).

### BullseyeCoverage (commercial)

- **Collection**: proprietary compiler wrapper that instruments C/C++ conditionals.
- **Differentiator**: C/C++ **condition/decision coverage** (MC/DC) for ~20 years; historical leader for safety-critical domains (avionics, automotive DO-178B).
- **Format**: proprietary `.cov`; emits HTML.
- **Notes**: expensive corporate licensing. Irrelevant to open-source TML.

### Coverity (Synopsys)

- Coverage is not the focus — it is a **static-analysis SaaS**. Optional coverage via integrators. Not relevant to TML.

---

## Rust

### `cargo-llvm-cov` (recommended, 2022–2025 growth leader)

- **Collection**: wrapper over `cargo test` + `rustc -Cinstrument-coverage` (which under the hood is LLVM source-based, same as clang's flag).
- **Technique**: LLVM source-based coverage (identical to clang).
- **Intermediate format**: `.profraw` → `.profdata` → LCOV/JSON/HTML via `llvm-cov`.
- **HTML**: the same `llvm-cov show` output (`cargo llvm-cov --html`).
- **Status**: became the **default recommendation** in the Rust community around 2023 after `cargo tarpaulin` showed limitations. Integrated in `rust-analyzer` and GitHub Actions.

### `cargo-tarpaulin`

- **Collection**: on Linux, uses `ptrace` + DWARF line tables to trap per-line (similar to OpenCppCoverage, via ptrace); on Windows/macOS it has switched to LLVM source-based in recent versions.
- **Technique**: binary rewriting on Linux; LLVM elsewhere.
- **Formats**: `Html`, `Lcov`, `Json`, `Xml` (Cobertura), `Stdout`.
- **HTML**: simple tabular layout using a Tera template [verify template engine exactly]. Less polished than Istanbul's.
- **Notes**: slower than `cargo-llvm-cov`, with worse precision on generic/async code. Still used by some legacy projects.

### `grcov`

- **Collection**: **converter only**. Consumes `.gcda` (GCC-style) or `.profraw` (LLVM, from `-Cinstrument-coverage`) and exports to LCOV, Cobertura, HTML, Markdown.
- **HTML**: Handlebars template. Clean, simple. Used by Mozilla/Firefox at scale.
- **Notes**: a **format multiplexer** — useful when the pipeline has to emit several reports. Does not instrument on its own.

---

## JavaScript / TypeScript

### Istanbul / `nyc` / `@istanbuljs/*`

- **Collection**: Babel/AST-based source-to-source transform on JS → inserts counters (`__coverage__["file.js"].s[3]++`). Supports line, statement, function and branch (`if/else`, `? :`, `&&`, `||`).
- **Technique**: source-to-source transform. Requires an `nyc` wrapper or `--experimental-vm-modules` for ESM.
- **Intermediate format**: an in-memory global `__coverage__`, flushed to `coverage/coverage-final.json` at exit. Reporters convert to LCOV, Cobertura, text, html, json-summary, text-summary, clover.
- **HTML**: **the quality reference**. File tree on the left, source view, per-line counters, branch highlight with `E` (else) / `I` (if). Expand/collapse for branch details. Client-side only.
- **Notes**: de-facto JS standard for 10+ years. Inspires almost every modern HTML reporter.

### `c8` (V8 built-in)

- **Collection**: uses V8/Node's `--inspect` API (`Profiler.startPreciseCoverage`) — **no source rewrite**; V8 keeps native per-basic-block counters.
- **Technique**: runtime counters in the engine (built-in).
- **Intermediate format**: V8 JSON → c8 converts to an Istanbul-compatible shape → reuses Istanbul's reporters, including the HTML.
- **Overhead**: very low (< 5%) because it is engine-native.
- **Notes**: **best performance**; depends on source maps for TS mapping. Requires Node 10.10+.

### Jest / Vitest coverage

- **Jest**: Istanbul underneath via Babel transform.
- **Vitest**: two providers — `istanbul` (compat) and `v8` (like c8, faster). `coverage.provider = 'v8'`.
- **HTML**: both generate the standard Istanbul reporter.

### Playwright coverage

- Uses Chromium DevTools Protocol APIs (`Profiler.startPreciseCoverage`, similar to c8). Focused on **browser/E2E coverage** (not unit tests). Exports JSON convertible to Istanbul.

---

## JVM (Java, Kotlin, Scala)

### JaCoCo

- **Collection**: two modes:
  - **On-the-fly**: Java agent that rewrites bytecode at class load time (`-javaagent:jacocoagent.jar`). Inserts probes per basic block.
  - **Offline**: static rewrite of classes before execution.
- **Technique**: bytecode rewriting, counters per basic block (mapped to lines via debug info).
- **Intermediate format**: `.exec` (proprietary binary) → `jacoco.xml` (de-facto standard in JVM CI) or HTML via `jacoco report`.
- **HTML**: heavy and tabular (full package/class tree). Five dimensions: instruction, line, branch, complexity, method. Less modern than Istanbul but very complete.
- **Notes**: **the JVM standard**. SonarQube, IntelliJ, GitLab and Jenkins consume `jacoco.xml` directly.

### Clover (Atlassian, open-source since 2017)

- Source-based instrumentation. Barely used today — JaCoCo won. `clover.xml` still accepted by a few services (older than Cobertura XML, less widespread).

---

## Go

### `go test -cover` / `go tool cover`

- **Collection**: the Go compiler supports `-cover` natively since Go 1.2 (2013). Instruments in the SSA/AST with counters. Since Go 1.20 (2023), supports **binary-wide coverage** (not only tests) and **transitive-package coverage** for integration tests.
- **Technique**: compiler-inserted counters (statement-level; no native branch coverage).
- **Intermediate format**: `coverage.out` (Go-specific textual format). `go tool cover -html=coverage.out` generates HTML; `-func=` dumps text.
- **HTML**: very simple — a single file with a dropdown for file selection, green/red source lines. No file tree, no branch coverage.
- **Notes**: minimalist. The ecosystem converts via `gcov2lcov` to feed Codecov/Coveralls.

---

## Python

### `coverage.py` (Ned Batchelder)

- **Collection**: via `sys.settrace` (Python 2/3) or `sys.monitoring` (Python 3.12+, much faster). Captures every executed line.
- **Technique**: runtime tracing (no rewriting).
- **Intermediate format**: SQLite `.coverage` (binary, but an open schema). Exports to LCOV (since 6.0), JSON, XML (Cobertura), HTML, text.
- **HTML**: light and efficient, **client-side only**. Left file tree, file view with toggles (executed / missed / both). Syntax highlighting via Pygments. Branch counters with `coverage run --branch`. Stable since ~2015, incremental UX improvements.
- **Overhead**: with `sys.settrace`: 2–5× slowdown. With `sys.monitoring` (3.12+): 1.3–1.5×.
- **Notes**: the absolute Python standard. `pytest-cov` wraps it for pytest.

---

## .NET

### Coverlet

- **Collection**: MSBuild task or data collector; IL runtime instrumentation (via `MonoCecil`) or source-based (via `Microsoft.CodeCoverage`).
- **Formats**: `lcov`, `cobertura`, `opencover`, `json`, `teamcity`.
- **HTML**: Coverlet itself does not emit HTML; **ReportGenerator** (a separate tool, near-standard in .NET) consumes any of the above and produces a very complete HTML (file tree, branch coverage, history tracking, risk hotspots).

### dotCover (JetBrains, commercial)

- Integrated into Rider/ReSharper. HTML via ReportGenerator or the internal viewer.

---

## Swift / Objective-C

### `llvm-cov` (embedded in Xcode)

- Xcode invokes `xcodebuild ... -enableCodeCoverage YES`, which underneath is `clang -fprofile-instr-generate -fcoverage-mapping`. Same LLVM toolchain.

### `xcov`, `slather`

- Ruby/Swift wrappers around `xccov` and `llvm-cov`. Export HTML, JSON, Cobertura.
- `slather` is the most popular in iOS open-source projects; direct Coveralls integration.

---

## Stand-alone HTML tools

These do not collect — they consume a format and render HTML. This is the most useful reference for TML:

| Tool | Input | Output | Quality |
|------|-------|--------|---------|
| `genhtml` (part of LCOV) | `.info` | Static multi-file HTML | Classic, ugly but complete. Branch + line + function. |
| Istanbul `html` reporter | Istanbul JSON | Client-side SPA-ish HTML | Excellent. The reference. |
| `llvm-cov show --format=html` | `.profdata` + binary | Static HTML | Modern and simple since LLVM 15. |
| ReportGenerator (.NET) | LCOV, Cobertura, OpenCover, JaCoCo, ... | Multi-theme HTML (`Html`, `HtmlSummary`, `HtmlChart`, `HtmlInline`, `HtmlInline_AzurePipelines`) | Extremely complete: diff vs baseline, risk hotspots, history graphs. |
| coverage.py HTML | SQLite `.coverage` | Client-side static HTML | Light, efficient, incremental. |
| JaCoCo report | `.exec` | HTML + package tree | Heavy but complete. |
| `grcov --html` | `.profraw` / `.gcda` | HTML | Handlebars-based, simple. |
| tarpaulin `--out Html` | internal | HTML | Tera template, simple. |

**Critical observation**: every modern HTML reporter follows the same architecture — **preprocessed JSON embedded in the HTML + client-side JS for interactivity**. None generates HTML via string concatenation in a compiled language. None re-highlights source — either reads source from disk at load time or embeds the source as a string in the JSON.

---

## Stable patterns in 2026

After 20 years of evolution, the market has converged on:

1. **Source-based instrumentation** won (LLVM, rustc, Go, clang). Trace-based (coverage.py) and bytecode rewriting (JaCoCo, Istanbul) remain only where the compiler is out of reach.
2. **LCOV `.info`** is the textual lingua franca. Cobertura XML is #2 (Jenkins, GitLab). Per-tool JSON exists but is internal.
3. **Static client-side HTML** (JSON + JS) is the standard. No serious project emits HTML server-side via templates at build time.
4. **Branch coverage is the floor.** MC/DC is a differentiator (LLVM 18+, Bullseye).
5. **SaaS integration** (Codecov, Coveralls, Sonar) is assumed — without LCOV, the project is invisible to PR reviewers.

---

## What this means for TML

The current TML model sits on the wrong side of every axis above:

- **Function-level** when the market is line + branch.
- **Proprietary JSON** when the market is LCOV.
- **HTML generated in C++** when the market is static SPAs.

The upside: TML's LLVM toolchain (via Zig CC) already provides the right substrate. The migration is about **removing the custom path**, not building a new one.

Continues in [`02-formats-and-standards.md`](./02-formats-and-standards.md).
