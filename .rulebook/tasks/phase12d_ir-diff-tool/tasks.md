# Tasks: Build IR-Diff Testing Tool

**Status**: In Progress (4/16) — relocated to `tools/ir-diff/` (2026-04-07)
**Depends on**: None (can start immediately)
**Blocks**: All Era 1 phases (differential testing is the primary correctness mechanism for self-hosting)
**Duration**: 2–3 weeks
**Risk**: Medium
**Output**: `tml ir-diff` CLI command + `tools/ir-diff/` standalone package

**Architecture note**: `ir-diff` is a compiler developer tool, not a runtime library. It lives in top-level `tools/ir-diff/` (Rust cargo-expand style) rather than `lib/std/`. This keeps the stdlib focused on runtime concerns and makes room for future dev tools (`tools/ast-dump`, `tools/mir-print`, etc.) to follow the same pattern. Files were migrated from `lib/std/src/ir_diff/` on 2026-04-07 after initial Phase 1 work completed.

---

## Phase 1: IR Parser

- [x] 1.1 Create `tools/ir-diff/src/mod.tml` — module declaration, re-exports. Also created `tools/ir-diff/src/types.tml` with shared types (IrModule/IrFunction/IrBlock/IrInstr/IrParam/IrGlobal/ParseError) used by parser, normalizer, and differ.
- [x] 1.2 Create `tools/ir-diff/src/parser.tml` — line-based LLVM IR text parser built on `core::str::split::lines`, `split_once`, `substring`, `trim`, and `std::collections::list::List`. Strings use `Str` (TML primitive); proposal's "Text" refers to the data-model sense.
- [x] 1.3 Parse function definitions: `define [linkage] <rettype> @<name>(<params>) [attrs] { ... }`. Depth-walks parens for nested function pointer types. Returns a dedicated `FnHeader` struct instead of a 4-tuple (tuple-of-list codegen bug workaround).
- [x] 1.4 Parse instructions: assignments (`%r = opcode ...`), calls, branches, returns, GEP, load, store, phi, alloca. Generic opcode+operand split handles all opcodes uniformly. Trailing `, !dbg !N` metadata is stripped before parsing. Top-level-comma operand splitter respects nested `()[]<>{}`.
      - Tests: `tools/ir-diff/tests/parser.test.tml` — 6 tests (simple function, multi-block branches, calls/phi, globals, trailing metadata, malformed rejection). All passing at time of migration.
      - Codegen workarounds encountered: (a) `if-else` expression returning `List[T]` caused invalid `store i32 → List` IR; replaced with `var` + mutation. (b) `use core::str::basic::len as str_len` emitted `@tml_str_len` undefined symbol; replaced with local `@extern("strlen")` declaration. (c) Tuple-of-List return type triggered similar issue; replaced with a dedicated `FnHeader` struct. (d) `{` in string literals must be escaped as `\{` — TML double-quoted strings support `{expr}` interpolation.
      - NOTE: these codegen bugs should be filed as individual tasks in follow-up. For now, workarounds stay in place.

## Phase 2: IR Normalizer

- [ ] 2.1 Create `tools/ir-diff/src/normalizer.tml` — normalize IR for comparison using `HashMap` for register renaming. (File moved from `lib/std/src/ir_diff/normalizer.tml`; ~389 lines exist but need verification after relocation.)
- [ ] 2.2 Rename all local registers (`%0`, `%1`, `%_name`) to canonical sequential names (`%r0`, `%r1`, ...) in definition order
- [ ] 2.3 Rename all basic block labels (`entry`, `bb1`, `loop_body`) to canonical sequential names (`%b0`, `%b1`, ...)
- [ ] 2.4 Strip metadata, debug info, and comments — drop lines starting with `!` or `;`, drop `!dbg` suffixes from instructions

## Phase 3: Semantic Differ

- [ ] 3.1 Create `tools/ir-diff/src/differ.tml` — compare two normalized IR modules, return structured diff result. (File moved from `lib/std/src/ir_diff/differ.tml`; ~334 lines exist but need verification after relocation.)
- [ ] 3.2 Diff function-by-function: match by demangled name, report functions present in one file but not the other
- [ ] 3.3 Diff instruction-by-instruction within matched functions: report first differing instruction with index and surrounding context
- [ ] 3.4 Report differences as `Outcome[DiffResult, DiffError]` — structured output with function name, instruction index, expected line, actual line

## Phase 4: CLI Integration

- [ ] 4.1 Add `ir-diff` subcommand to compiler CLI (`compiler/src/cli/commands/`) — `tml ir-diff file1.ll file2.ll [--summary] [--function name]`. Command dispatches to the `tools/ir-diff/` package, either by linking it as a TML crate/rlib or by compiling `tools/ir-diff/src/main.tml` to a standalone binary that the CLI shells out to. Decide based on simplicity; prefer in-process linkage if the build system supports it.
- [ ] 4.2 Exit code: 0 if semantically identical after normalization, 1 if different — print diff to stdout in unified diff format.
- [ ] 4.3 Create `tools/ir-diff/build.tml` or equivalent manifest so the tool can be built independently via `tml build tools/ir-diff/`.

## Phase 5: Tests

- [ ] 5.1 Update `tools/ir-diff/tests/` — migrate existing `parser.test.tml` and `normalizer.test.tml` (already moved). Add: identical files (expect exit 0), cosmetically different register names (expect exit 0), different instruction count (expect exit 1), different instruction type (expect exit 1).
- [ ] 5.2 Integration test: compile the same `.tml` file twice with `mcp__tml__emit-ir`, run `tml ir-diff` on the two outputs — must report identical (exit 0).

## Phase 6: Stdlib Cleanup (migration)

- [x] 6.1 Remove `pub mod ir_diff` from `lib/std/src/mod.tml`.
- [x] 6.2 Move `lib/std/src/ir_diff/*.tml` → `tools/ir-diff/src/`.
- [x] 6.3 Move `lib/std/tests/ir_diff/*.tml` → `tools/ir-diff/tests/`.
- [x] 6.4 Remove empty `lib/std/src/ir_diff/` and `lib/std/tests/ir_diff/` directories.
- [ ] 6.5 Update any `use std::ir_diff::*` imports in the migrated files to use the new module path (likely `tools::ir_diff` or relative imports within the package).
- [ ] 6.6 Verify the relocated files still compile via `mcp__tml__check tools/ir-diff/src/mod.tml`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
