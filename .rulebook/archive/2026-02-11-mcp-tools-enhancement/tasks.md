# Tasks: MCP Tools Enhancement

**Status**: Complete (100%)

## Phase 1: P0 — Critical Tools

- [x] 1.1.1 Implement `project/build` tool (wrap `scripts/build.bat` via `_popen`)
- [x] 1.1.2 Add `find_tml_root()` based working directory for build
- [x] 1.1.3 Return structured JSON: `{ success, duration_ms, output_path, errors[] }`
- [x] 1.1.4 Handle build timeout (default 300s)
- [x] 1.1.5 Add `target` parameter to build tml.exe vs tml_mcp.exe separately
- [x] 1.2.1 Add `no_cache` parameter to existing `test` tool
- [x] 1.2.2 Add `fail_fast` parameter to existing `test` tool
- [x] 1.2.3 Add `structured` output mode to `test` tool
- [x] 1.2.4 Parse test results into `{ total, passed, failed, failures[] }`
- [x] 1.3.1 Implement `strip_ansi()` helper function
- [x] 1.3.2 Apply ANSI stripping to all `execute_command()` output

## Phase 2: P1 — High Impact

- [x] 2.1.1 Implement `project/coverage` tool
- [x] 2.1.2 Parse coverage report into per-module breakdown
- [x] 2.1.3 Add `module`, `sort`, `limit` filter parameters
- [x] 2.1.4 Dynamic lib/ scanning for coverage (all subdirs, not just core/std)
- [x] 2.2.1 Implement `explain` tool (via `tml.exe explain` subprocess)
- [x] 2.2.2 Return error description, causes, and fix examples
- [x] 2.3.1 Dynamic lib/ scanning for doc indexing (all subdirs)
- [x] 2.3.2 Implement `extract_hpp_docs()` for C++ header documentation
- [x] 2.3.3 Add compiler headers to doc search index (compiler::* modules)
- [x] 2.3.4 Include hpp files in doc cache fingerprint for invalidation

## Phase 2.5: Bug Fixes

- [x] 2.5.1 Fix `format` tool: used `tml format` but CLI command is `tml fmt`
- [x] 2.5.2 Fix `format --check` returning error for informational "needs formatting" result
- [x] 2.5.3 Fix compiler doc module paths (`compilerquery::` -> `compiler::query::`)

## Phase 3: P2 — Quality of Life

- [x] 3.1.1 Implement `project/structure` tool with `std::filesystem`
- [x] 3.1.2 Module tree with file counts, test file counts, metadata
- [x] 3.2.1 Implement `project/affected-tests` with git diff detection
- [x] 3.2.2 Map changed files to affected test files
- [x] 3.2.3 Optional auto-run of affected tests

## Phase 4: P3 — Nice to Have

- [x] 4.1.1 Implement `project/artifacts` tool
- [x] 4.1.2 List executables, cache dirs, coverage files with metadata

## Validation

- [x] V.1 `project/build` builds compiler without Bash fallback
- [x] V.2 `test` structured mode returns parsed pass/fail counts
- [x] V.3 No ANSI escape codes in any MCP tool output
- [x] V.4 `project/coverage` returns per-module coverage percentages
- [x] V.5 `explain` returns useful error descriptions
- [x] V.6 `project/structure` shows module tree without shell commands
- [x] V.7 Doc search returns compiler::* modules from .hpp headers
- [x] V.8 Coverage and docs scan all lib/ subdirs dynamically
- [x] V.9 `format` tool works (calls `tml fmt`, not `tml format`)
- [x] V.10 `format --check` returns green for "needs formatting" (informational, not error)
- [x] V.11 All 14 MCP-exposed tools tested and passing via native MCP
- [x] V.12 `project/structure` and `project/affected-tests` registered and indexed in doc search
- [x] V.13 `project/artifacts` registered and indexed in doc search (8372 items total)
