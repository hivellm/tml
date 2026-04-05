# Proposal: phase0_sciplot-chart-library

## Why
The LLM-IR debugging research paper requires data visualizations (bar charts, line charts, scatter plots) generated from TML scripts. Python/matplotlib is unacceptable since TML has its own language — all tooling should be self-hosted. A TML chart library wrapping sciplot (header-only C++17, MIT license) enables chart generation from TML code via gnuplot backend.

## What Changes
1. New `lib/chart/` package with TML API wrapping sciplot via C FFI
2. `chart_ffi.cpp` — C++ wrapper around sciplot providing 10 `@extern("c")` functions
3. `mod.tml` — `Chart` type with constructors (`bar()`, `line()`, `scatter()`), data methods (`add_bar`, `add_series`, `add_point`), appearance methods (`set_title`, `set_xlabel`, `set_ylabel`, `set_size`), and lifecycle (`save`, `destroy`)
4. `build.tml` — build script for native library linking
5. sciplot headers bundled in `native/sciplot/` (header-only, MIT license in `SCIPLOT-LICENSE`)
6. 6 tests across 2 test files covering all chart types, SVG output, custom sizes, and multiple series
7. `visualize.tml` script in `docs/papers/llm-ir-debugging/scripts/` using the library

## Impact
- Affected specs: None (new library, no language changes)
- Affected code: `lib/chart/` (new), `docs/papers/llm-ir-debugging/scripts/visualize.tml`
- Breaking change: NO
- User benefit: TML programs can generate publication-quality charts (PNG, SVG, PDF) without external Python/matplotlib dependencies
