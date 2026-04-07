# Tasks: Build IR-Diff Testing Tool

**Status**: Complete (16/16) — relocated to `tools/ir_diff/` (2026-04-07)
**Depends on**: None (can start immediately)
**Blocks**: All Era 1 phases (differential testing is the primary correctness mechanism for self-hosting)
**Duration**: 2–3 weeks
**Risk**: Medium
**Output**: `tml ir-diff` CLI command + `tools/ir_diff/` standalone package

**Architecture note**: `ir-diff` is a compiler developer tool, not a runtime library. It lives in top-level `tools/ir_diff/` (Rust cargo-expand style) rather than `lib/std/`. This keeps the stdlib focused on runtime concerns and makes room for future dev tools (`tools/ast-dump`, `tools/mir-print`, etc.) to follow the same pattern. Files were migrated from `lib/std/src/ir_diff/` on 2026-04-07 after initial Phase 1 work completed.

---

## Phase 1: IR Parser

- [x] 1.1 Create `tools/ir_diff/src/mod.tml` — module declaration, re-exports. Also created `tools/ir_diff/src/types.tml` with shared types (IrModule/IrFunction/IrBlock/IrInstr/IrParam/IrGlobal/ParseError) used by parser, normalizer, and differ.
- [x] 1.2 Create `tools/ir_diff/src/parser.tml` — line-based LLVM IR text parser built on `core::str::split::lines`, `split_once`, `substring`, `trim`, and `std::collections::list::List`. Strings use `Str` (TML primitive); proposal's "Text" refers to the data-model sense.
- [x] 1.3 Parse function definitions: `define [linkage] <rettype> @<name>(<params>) [attrs] { ... }`. Depth-walks parens for nested function pointer types. Returns a dedicated `FnHeader` struct instead of a 4-tuple (tuple-of-list codegen bug workaround).
- [x] 1.4 Parse instructions: assignments (`%r = opcode ...`), calls, branches, returns, GEP, load, store, phi, alloca. Generic opcode+operand split handles all opcodes uniformly. Trailing `, !dbg !N` metadata is stripped before parsing. Top-level-comma operand splitter respects nested `()[]<>{}`.
      - Tests: `tools/ir_diff/tests/parser.test.tml` — 6 tests (simple function, multi-block branches, calls/phi, globals, trailing metadata, malformed rejection). All passing.

## Phase 2: IR Normalizer

- [x] 2.1 Create `tools/ir_diff/src/normalizer.tml` — normalize IR for comparison using `HashMap` for register renaming. ~404 lines.
- [x] 2.2 Rename all local registers (`%0`, `%1`, `%_name`) to canonical sequential names (`%r0`, `%r1`, ...) in definition order. Function parameters → `%a0`, `%a1`, ...
- [x] 2.3 Rename all basic block labels (`entry`, `bb1`, `loop_body`) to canonical sequential names (`b0`, `b1`, ...)
- [x] 2.4 Strip metadata, debug info, and comments — drop lines starting with `!` or `;`, drop `, !dbg` suffixes from instructions

## Phase 3: Semantic Differ

- [x] 3.1 Create `tools/ir_diff/src/differ.tml` — compare two normalized IR modules, return structured diff result. ~400 lines.
- [x] 3.2 Diff function-by-function: match by exact name first, fallback to demangled-name match. Report functions present in only one file.
- [x] 3.3 Diff instruction-by-instruction within matched functions: report first differing instruction with index and surrounding context window
- [x] 3.4 Report differences as `Outcome[DiffResult, DiffError]` — structured output with function name, instruction index, expected line, actual line. Cross-module accessor functions exposed (`only_in_a_count`, `diff_func_name`, etc.) to work around field-access codegen bug.

## Phase 4: CLI Integration

- [x] 4.1 `tools/ir_diff/src/main.tml` is the standalone CLI binary. Builds via `tml build tools/ir_diff/src/main.tml`. Supports `<file_a.ll> <file_b.ll> [--summary] [--function <name>] [--help]` arguments. Uses direct `@extern("c")` for `fopen`/`fread`/`fclose`/`malloc`/`memset`/`tml_os_args_count`/`tml_os_args_get` to avoid pulling in `std::os` and `std::file::File` which transitively import `core::runtime::error::NeverError` (a struct with a `Never` field that produces invalid `{ void }` LLVM IR — known codegen bug).
- [x] 4.2 Exit code: 0 if identical, 1 if different, 2 on usage/file errors. Verified: identical files exit 0 path implemented; different files reported correctly with detailed unified-diff-style output (`@@@ func @@@ / - / +`); missing files exit 2.
- [x] 4.3 `tools/ir_diff/package.toml` exists with `[lib]` (`mod.tml`) and `[[bin]]` (`main.tml`) entries. The package can be built independently.

## Phase 5: Tests

- [x] 5.1 Tests in `tools/ir_diff/tests/`:
      - `parser.test.tml` — 6 tests (parsing primitives)
      - `normalizer.test.tml` — 3 tests (canonical renaming)
      - `norm_minimal.test.tml` — 1 smoke test
      - `differ.test.tml` — 4 tests (constructed IR modules: identical, instruction-count mismatch, opcode mismatch, function-only-in-one-side)
      - `differ_minimal.test.tml` — 1 smoke test
      - `integration.test.tml` — 2 tests (parse → diff pipeline: identical, different functions). Kept minimal due to test-framework's hardcoded 100ms per-test timeout in debug builds.
- [x] 5.2 Integration test: `integration.test.tml` exercises parse → diff on identical and different IR strings, verifying the full pipeline works end-to-end. The CLI `main.exe` was also manually verified against constructed `.ll` files: identical files compared successfully, different files produced the expected unified-diff-style output with correct exit codes (0/1/2).

## Phase 6: Stdlib Cleanup (migration)

- [x] 6.1 Remove `pub mod ir_diff` from `lib/std/src/mod.tml`.
- [x] 6.2 Move `lib/std/src/ir_diff/*.tml` → `tools/ir_diff/src/`.
- [x] 6.3 Move `lib/std/tests/ir_diff/*.tml` → `tools/ir_diff/tests/`.
- [x] 6.4 Remove empty `lib/std/src/ir_diff/` and `lib/std/tests/ir_diff/` directories.
- [x] 6.5 All `use ir_diff::*` imports verified; relocated files use the package-relative path (`use ir_diff::types::...`, `use ir_diff::parser::parse`, etc.) and resolve correctly via the `tools/` package root support added in commit `1b312217`.
- [x] 6.6 All relocated files compile and tests pass via `mcp__tml__test --path tools/ir_diff/tests/*.test.tml`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Documentation: `tools/ir_diff/README.md` created with purpose, architecture, usage (CLI + library), build/test instructions, and known limitations.
- [x] 1.2 Tests: 6 test files covering parser (6 tests), normalizer (3 + 1 smoke), differ (4 + 1 smoke), and integration (2). All test files compile and pass.
- [x] 1.3 Run tests and confirm they pass — verified all 6 test files pass via `tml test --path tools/ir_diff/tests/<file>.test.tml --no-cache`.
