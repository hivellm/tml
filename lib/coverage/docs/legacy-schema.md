# Legacy TML coverage format (reverse-engineered)

The current `tml test --coverage` collector does **not** emit JSON despite the
working file name suggesting otherwise. It emits a plain text file, one
covered function name per line, encoded UTF-8, no header and no counts.

## On-disk layout

Files are written to `build/coverage/` by the test-harness epilogue when
the environment variable `TML_COVERAGE_FILE` is set. The coordinator
points each subprocess at a distinct file:

```
build/coverage/cov_<suite>.txt
build/coverage/cov_<suite>_t<thread_index>.txt    # when thread-sharded
```

## File format

```text
<mangled_or_source_function_name>\n
<mangled_or_source_function_name>\n
...
```

Rules observed in [`lib/test/runtime/coverage.c`](../../test/runtime/coverage.c) and
[`compiler/src/testing/testing_coordinator.cpp`](../../../compiler/src/testing/testing_coordinator.cpp):

- Only functions with `hit_count > 0` are written (uncovered functions are
  absent entirely — there is no per-function hit count).
- Trailing `\r` is stripped by the coordinator before union.
- Empty lines are tolerated.
- Multiple files for the same suite are unioned by the coordinator into
  the set passed to the HTML reporter.

## What gets lost

Compared to LCOV / llvm-cov:

| Data | Legacy format | LCOV |
|------|---------------|------|
| Per-function hit count | absent (implicit `> 0`) | present (`FNDA`) |
| Uncovered functions | absent | present (`FNDA:0`) |
| Per-line hit count | absent | present (`DA`) |
| Per-branch hit count | absent | present (`BRDA`) |
| Source path | absent | present (`SF`) |

This is why the new library exists: the legacy format cannot express
what modern coverage tooling expects.

## Ingest strategy

`coverage::ingest::legacy_json::read_legacy_json` reads this plain-text
format (name kept for backward compatibility with the task proposal).
It synthesizes a `CoverageReport` by:

1. Splitting the file by `\n` and trimming `\r`.
2. Mapping each line (function name) to a `FuncHit` with
   `hit_count = 1`, `start_line = 0`, and leaving the file path empty.
3. Grouping all recovered functions under a synthetic
   `FileCoverage { path: "<unknown>", ... }` entry since the legacy
   format carries no path information.

Downstream merging / summarizing is lossy at the file level; the only
signal retained is "this function ran at least once during the suite".

## Example input (verbatim)

```
lib::core::str::basic::substring_from
lib::core::str::split::split
coverage::types::CoverageReport_new
```

There is no JSON syntax anywhere in the file.
