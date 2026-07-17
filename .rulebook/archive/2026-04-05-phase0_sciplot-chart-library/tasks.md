# phase0_sciplot-chart-library — COMPLETED (12/12)

## 1. Architecture & Setup
- [x] 1.1 Create `lib/chart/` package structure (`package.toml`, `src/mod.tml`, `build.tml`)
- [x] 1.2 Bundle sciplot headers in `native/sciplot/` (header-only C++17, MIT)
- [x] 1.3 Add `SCIPLOT-LICENSE` file with MIT license text

## 2. C++ FFI Layer (`native/chart_ffi.cpp`)
- [x] 2.1 Implement `chart_create(type)` — creates ChartState, returns int64 handle
- [x] 2.2 Implement `chart_set_title/xlabel/ylabel/size` — appearance configuration
- [x] 2.3 Implement `chart_add_bar(handle, label, value)` — bar chart data
- [x] 2.4 Implement `chart_add_series/add_point` — line/scatter series data
- [x] 2.5 Implement `chart_save(handle, path)` — renders via sciplot/gnuplot, format from extension
- [x] 2.6 Implement `chart_destroy(handle)` — frees ChartState memory

## 3. TML API (`src/mod.tml`)
- [x] 3.1 Define `Chart` type with handle-based FFI pattern
- [x] 3.2 Implement constructors: `Chart::bar()`, `Chart::line()`, `Chart::scatter()`
- [x] 3.3 Implement data/appearance/lifecycle methods wrapping FFI calls

## 4. Testing
- [x] 4.1 `chart_basic.test.tml` — bar chart, line chart, scatter plot (3 tests)
- [x] 4.2 `chart_advanced.test.tml` — SVG output, custom size, multiple series (3 tests)

## 5. Integration
- [x] 5.1 `visualize.tml` script for research paper chart generation
