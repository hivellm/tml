# Tasks: Build IR-Diff Testing Tool

**Status**: Planned (0/16)
**Depends on**: None (can start immediately)
**Blocks**: All Era 1 phases (differential testing is the primary correctness mechanism for self-hosting)
**Duration**: 2–3 weeks
**Risk**: Medium
**Output**: `tml ir-diff` CLI command + `lib/std/src/ir_diff/` module

---

## Phase 1: IR Parser

- [ ] 1.1 Create `lib/std/src/ir_diff/mod.tml` — module declaration, re-exports
- [ ] 1.2 Create `lib/std/src/ir_diff/parser.tml` — parse LLVM IR text into structured representation using `Text` and `List`
- [ ] 1.3 Parse function definitions: `define <type> @<name>(<params>) { ... }` — extract name, return type, parameter types, basic blocks
- [ ] 1.4 Parse instructions: assignments, calls, branches, returns, GEP, load/store, phi nodes, alloca

## Phase 2: IR Normalizer

- [ ] 2.1 Create `lib/std/src/ir_diff/normalizer.tml` — normalize IR for comparison using `HashMap` for register renaming
- [ ] 2.2 Rename all local registers (`%0`, `%1`, `%_name`) to canonical sequential names (`%r0`, `%r1`, ...) in definition order
- [ ] 2.3 Rename all basic block labels (`entry`, `bb1`, `loop_body`) to canonical sequential names (`%b0`, `%b1`, ...)
- [ ] 2.4 Strip metadata, debug info, and comments — drop lines starting with `!` or `;`, drop `!dbg` suffixes from instructions

## Phase 3: Semantic Differ

- [ ] 3.1 Create `lib/std/src/ir_diff/differ.tml` — compare two normalized IR modules, return structured diff result
- [ ] 3.2 Diff function-by-function: match by demangled name, report functions present in one file but not the other
- [ ] 3.3 Diff instruction-by-instruction within matched functions: report first differing instruction with index and surrounding context
- [ ] 3.4 Report differences as `Outcome[DiffResult, DiffError]` — structured output with function name, instruction index, expected line, actual line

## Phase 4: CLI Integration

- [ ] 4.1 Add `ir-diff` subcommand to compiler CLI (`compiler/src/cli/commands/`) — `tml ir-diff file1.ll file2.ll [--summary] [--function name]`
- [ ] 4.2 Exit code: 0 if semantically identical after normalization, 1 if different — print diff to stdout in unified diff format

## Phase 5: Tests

- [ ] 5.1 Create `lib/std/tests/ir_diff/` — test pairs: identical files (expect exit 0), cosmetically different register names (expect exit 0), different instruction count (expect exit 1), different instruction type (expect exit 1)
- [ ] 5.2 Integration test: compile the same `.tml` file twice with `mcp__tml__emit-ir`, run `tml ir-diff` on the two outputs — must report identical (exit 0)
