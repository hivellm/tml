# Tasks: CLI, Testing, and Formatter — Rewrite in TML

**Status**: Planned (0/24)
**Depends on**: phase17a (query system available for compilation)
**Blocks**: phase17c (bootstrap needs CLI to invoke compiler)
**Duration**: 6–8 weeks
**Risk**: Medium — large LOC but mostly mechanical (CLI parsing, output formatting)
**C++ reference**: ~37,940 LOC → ~24,660 TML (partial — build system stays C++ initially)

---

## Phase 1: CLI Dispatcher (5 items)

- [ ] 1.1 Create `compiler-tml/src/cli/mod.tml` — CLI entry point, argument parsing
- [ ] 1.2 Implement subcommand dispatch: `build`, `run`, `test`, `check`, `fmt`, `lint`
- [ ] 1.3 Implement `build` command: parse args → create QueryContext → force(CodegenUnit) → link
- [ ] 1.4 Implement `run` command: build → execute binary (or JIT)
- [ ] 1.5 Implement `check` command: parse args → force(Typecheck) only (no codegen)

## Phase 2: Diagnostics (4 items)

- [ ] 2.1 Create `compiler-tml/src/cli/diagnostic.tml` — error/warning formatting
- [ ] 2.2 Implement source-span display: show file, line, column, underline with ^^^
- [ ] 2.3 Implement error code formatting: `error[T001]: type mismatch` with explanation
- [ ] 2.4 Implement color output: ANSI escape codes for terminal (error=red, warning=yellow, note=blue)

## Phase 3: Test System (6 items)

- [ ] 3.1 Create `compiler-tml/src/testing/mod.tml` — test coordinator
- [ ] 3.2 Implement test discovery: find `*.test.tml` files, group into suites by directory
- [ ] 3.3 Implement suite compilation: compile each suite to .exe via query pipeline
- [ ] 3.4 Implement subprocess execution: launch test .exe, parse NDJSON result stream
- [ ] 3.5 Implement coverage tracking: `TML_COVERAGE_FILE` env var, merge coverage data
- [ ] 3.6 Implement result reporting: pass/fail counts, failure details, timing

## Phase 4: Formatter (3 items)

- [ ] 4.1 Create `compiler-tml/src/format/mod.tml` — code formatter entry point
- [ ] 4.2 Implement: parse AST → pretty-print with canonical indentation, spacing, line breaks
- [ ] 4.3 Implement: `tml fmt --check` (report unformatted) and `tml fmt` (rewrite in place)

## Phase 5: Build System Core (4 items)

- [ ] 5.1 Create `compiler-tml/src/cli/builder.tml` — build orchestration (file discovery, dependency order)
- [ ] 5.2 Implement object compilation: query pipeline → LLVM IR → invoke C++ backend shim → .obj
- [ ] 5.3 Implement linking: collect .obj files → invoke C++ LLD shim → .exe
- [ ] 5.4 Implement build.tml support: detect and run build scripts for native dependencies

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Test: `tml-stage1 build hello.tml` produces identical binary to `tml.exe build hello.tml`
- [ ] 6.2 Test: `tml-stage1 test --suite core/str` produces identical results to C++ compiler

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
