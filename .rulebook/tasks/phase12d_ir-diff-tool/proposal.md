# Proposal: Build IR-Diff Testing Tool

**Task**: phase12d_ir-diff-tool
**Status**: Planned
**Priority**: P0
**Estimated effort**: 2–3 weeks
**Risk**: Medium

## Problem

The self-hosting project requires a reliable way to verify that a TML-written compiler stage
produces output that is semantically equivalent to the C++ stage it replaces. LLVM IR is the
natural comparison point: it is the boundary between the compiler frontend (which changes during
self-hosting) and the LLVM backend (which does not). If two compiler pipelines produce the same
LLVM IR for the same input, they are equivalent by definition.

The problem is that LLVM IR cannot be compared with `diff` directly. Two IR files that are
semantically identical will differ textually because register names (`%0`, `%1`, `%_name`) are
assigned in program-order, label names (`entry`, `bb1`, `loop.body`) are chosen by the emitter,
and metadata IDs (`!1`, `!dbg !42`) are assigned sequentially. A TML lexer that processes tokens
in the same order as the C++ lexer but names its temporaries differently will produce IR that is
byte-for-byte different but semantically identical. A naive `diff` would report this as a failure.

Without a semantic IR diff tool, verifying parity between C++ and TML stages requires either
manual inspection (error-prone, slow) or accepting false positives from textual diff (noisy,
causes alert fatigue). Both approaches break down at scale across 1,700+ tests.

The IR-diff tool is needed by phase12a (MIR consolidation parity verification in Phase 2),
phase12f (hybrid pipeline correctness gate), and all Era 1 phase tests.

## Proposed Solution

A TML-implemented `tml ir-diff` CLI command backed by a `tools/ir-diff/src/` module with
three components:

**Parser (`tools/ir-diff/src/parser.tml`)**: Reads LLVM IR text using `Text` and `List` and produces a
structured in-memory representation. Parses:
- Function definitions: `define <rettype> @<name>(<params>) { <blocks> }`
- Basic blocks: label + sequence of instructions
- Instructions: assignments, calls (direct and indirect), branches, returns, GEP, load, store,
  phi nodes, alloca, extractvalue, insertvalue, bitcast, trunc, zext, sext

Does not parse: module-level metadata (`!N = ...`), debug info declarations, attributes, comdat.
These are stripped before parsing begins.

**Normalizer (`tools/ir-diff/src/normalizer.tml`)**: Rewrites a parsed IR module to use canonical names:
- All local registers (`%0`, `%_tmp`, `%retval.0`) renamed to `%r0`, `%r1`, ... in definition order
- All basic block labels renamed to `%b0`, `%b1`, ... in definition order
- All metadata references stripped (`!dbg !N` suffixes removed from instructions)
- Comment lines (`;`) stripped
- Module-level metadata lines (`!N = distinct !DICompileUnit(...)`) stripped

Normalization uses a `HashMap[Str, Str]` rename map built in a first pass over each function.
The second pass applies renames to all instruction operands. Renaming is per-function: two
functions may independently have a `%r0` without conflict.

**Differ (`tools/ir-diff/src/differ.tml`)**: Compares two normalized IR modules and returns a structured
`Outcome[DiffResult, DiffError]`:
- Match functions by demangled name; report functions present in one file but absent in the other
- Within matched functions, compare instruction sequences line-by-line
- On first mismatch, report: function name, instruction index, expected instruction text, actual
  instruction text, and N lines of surrounding context (default N=3)
- Report instruction count mismatches as a distinct error class

**CLI integration**: `tml ir-diff file1.ll file2.ll [--summary] [--function <name>]`
- Exit 0 if semantically identical after normalization
- Exit 1 if different; print structured diff to stdout in unified-diff-like format
- `--summary`: print only the count of differing functions, no per-instruction detail
- `--function <name>`: compare only the named function, skip all others

## Key Decisions

**Normalize before compare, not during.** Running normalization as a separate pass over both
files before diffing makes the algorithm simpler and the output more predictable than trying to
rename on-the-fly while comparing.

**Parse only what matters for semantic equivalence.** Debug info, metadata, attributes, and
comdat are intentionally not parsed. They are cosmetic from the perspective of program semantics.
Stripping them before parsing avoids false positives from DWARF debug information that legitimately
differs between two compilation runs even for identical source.

**Function-by-function matching by demangled name, not by position.** Two IR files may define
the same functions in different orders if the emitter processes declarations in a different
sequence. Matching by name is robust; matching by position would fail on any reordering.

**Implement in TML, not as a shell script.** A shell script diff would require normalizing IR
via sed/awk, which is fragile across Windows and Linux. The TML implementation is
cross-platform, testable, and can be integrated into the MCP test infrastructure without
spawning shell processes.

**`--function` flag for focused debugging.** During parity work in phase12a and phase12f,
engineers will often know which specific function is wrong. The `--function` flag avoids
printing noise from the thousands of runtime library functions that are identical.

## Files to Create/Modify

**Created**:
- `tools/ir-diff/src/mod.tml` — module declaration with `pub use` of public types
- `tools/ir-diff/src/parser.tml` — LLVM IR text parser
- `tools/ir-diff/src/normalizer.tml` — register and label renaming pass
- `tools/ir-diff/src/differ.tml` — semantic comparison and diff result types
- `lib/std/tests/ir_diff/basic.test.tml` — test pairs: identical, cosmetically different, instruction count mismatch, instruction type mismatch
- `compiler/src/cli/commands/ir_diff_command.cpp` — CLI subcommand wiring

**Modified**:
- `compiler/src/cli/dispatcher.cpp` — register `ir-diff` subcommand
- `lib/std/src/mod.tml` — add `pub mod ir_diff`

## Success Criteria

- `tml ir-diff a.ll a.ll` exits 0 (identical file vs itself)
- `tml ir-diff a.ll b.ll` exits 0 when `a.ll` and `b.ll` differ only in register names and
  label names (cosmetic differences)
- `tml ir-diff a.ll b.ll` exits 1 when `b.ll` has a different instruction at any position in
  any function, and prints the differing instruction with context
- `tml ir-diff a.ll b.ll` exits 1 when `b.ll` is missing a function present in `a.ll`
- Integration test: compile the same `.tml` file twice with `mcp__tml__emit-ir`, run
  `tml ir-diff`, must report exit 0
- All tests in `lib/std/tests/ir_diff/` pass

## Dependencies

**Blocks**: phase12a Phase 2 item 2.5 (IR-diff of 50 representative test files), phase12f
Phase 5 item 5.3 (hybrid pipeline IR-diff correctness gate), all Era 1 phase parity tests.

**Depends on**: Nothing. `std::collections::HashMap`, `std::collections::List`, and `Text` are
already implemented. The CLI subcommand mechanism exists in `dispatcher.cpp`. This task can
start immediately.
