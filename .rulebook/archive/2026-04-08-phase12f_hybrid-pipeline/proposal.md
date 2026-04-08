# Proposal: Hybrid Pipeline Framework — C++/TML Stage Swapping

**Task**: phase12f_hybrid-pipeline
**Status**: Planned
**Priority**: P0
**Estimated effort**: 2–3 weeks
**Risk**: Medium

## Why

Incremental self-hosting requires a safe way to swap individual compiler stages from C++ to TML without breaking the full pipeline — this task adds that mechanism (`--stage=<name>:tml`) and validates it end-to-end with a TML lexer stage producing bitwise-identical IR to the C++ baseline.

## Problem

Self-hosting requires replacing C++ compiler stages with TML implementations incrementally.
The danger of a "big-bang" replacement is that it creates a long period where the compiler
cannot compile anything at all — the TML implementation is incomplete, the C++ implementation
has been removed, and there is no fallback. This is the classic self-hosting bootstrapping
problem.

The only safe approach is incremental replacement: run the TML implementation of stage N
alongside the C++ implementation of every other stage, verify that the output is identical,
then commit. To do this, the pipeline must support running individual stages as TML subprocesses
while all other stages run as C++ in-process as normal.

Currently, no such mechanism exists. The `QueryContext` executes all stages in-process via
C++ function calls. There is no way to swap a single stage for a subprocess. Even the existing
`--legacy` flag (which selects between two C++ MIR implementations) does not cross a process
boundary — it is purely in-process dispatch.

Without a hybrid pipeline framework, Era 1 Phase 1 (TML lexer/parser) cannot be integrated
into the compiler for correctness verification. The team would have to complete and verify the
entire lexer + parser + type checker + HIR + MIR + codegen chain before getting any signal that
the TML implementations are correct — an enormous risk.

## Proposed Solution

Runtime stage swapping via `--stage=<name>:tml` CLI flags. Each flag replaces one named pipeline
stage with a TML subprocess for the duration of the build. The subprocess receives its input
on stdin as serialized bytes (using the phase12e format) and writes its output to stdout in
the same format. The C++ pipeline continues without modification for all non-swapped stages.

**CLI design**: `--stage=lexer:tml`, `--stage=parser:tml`, `--stage=typechecker:tml`, etc.
Multiple flags may be combined: `tml build --stage=lexer:tml --stage=parser:tml file.tml`.
Stage names map to `QueryContext` query kinds: `lexer` → `TokenizeKey`, `parser` →
`ParseModuleKey`, `typechecker` → `TypecheckModuleKey`, `hir` → `HirLowerKey`,
`mir` → `MirBuildKey`, `codegen` → `CodegenUnitKey`. Stage overrides are stored as
`std::map<std::string, StageImpl>` in the `Options` struct passed to `QueryContext`.

**TML stage launcher** (`compiler/src/pipeline/tml_stage.cpp`): A `TmlStage` class responsible
for compiling and caching the TML stage source file and executing it as a subprocess. On the
first call for a given stage name, `TmlStage` checks `.incr-cache/stages/<stage_name>.exe`
against a fingerprint of the stage's TML source file. If the fingerprint matches, the cached
executable is used. If not, the stage source is compiled via the internal `QueryContext` (the
TML compiler compiling itself), and the resulting executable is cached. On each invocation,
`TmlStage::run()` launches the cached executable, writes serialized input to its stdin, and
reads serialized output from its stdout. Stderr from the subprocess is captured and emitted as
compiler diagnostics.

**QueryContext integration**: Before executing each `force<>()` call, `QueryContext` checks the
stage override map. If an override is registered for the current query kind, it calls
`TmlStage::run()` instead of the built-in implementation. The TML stage output is deserialized
using the phase12e readers and stored in the query cache as if the C++ implementation had
computed it. All downstream stages see no difference.

**Stage interface contracts**: Each stage boundary has a defined binary format:
- Lexer: input = source file path as UTF-8 string on stdin, output = serialized token list
  (magic `0x544D4C4C` "TMLL", version 1.0, varint count, then per-token: kind tag u8, offset
  u32, length u32, interned text u32 index)
- Parser: input = serialized token list, output = serialized `Module` AST (phase12e format)
- Type checker: input = serialized `Module` AST, output = serialized `TypeEnv` (phase12e format)
- HIR, MIR, Codegen: input = serialized output of previous stage, output = serialized IR

**Minimal TML lexer stage** (`tools/stages/lexer_stage.tml`): A TML program that reads a source
file path from argv[1], tokenizes using `std::lex` module (which must be available by the time
phase 1 integration tests run), and writes the serialized token list to stdout. This is the
integration test target; it does not need to be a complete production lexer.

## Key Decisions

**Subprocess, not shared library.** The TML stage runs as a separate process, not as a
dynamically loaded plugin. This choice provides complete isolation: a bug in the TML stage
cannot corrupt the C++ heap, and the TML stage can be compiled and cached independently of
the main compiler binary. The performance cost (subprocess launch + serialization) is bounded
at < 5% per-file and is acceptable during the verification phase of self-hosting.

**Separate stage cache from test cache.** Stage executable caches live in
`.incr-cache/stages/<name>.exe` and are NOT cleared by `--no-cache` (which clears only test
output caches). A developer who runs `--no-cache` to force test recompilation should not also
force recompilation of the TML lexer they built this morning. The stage cache is invalidated
only when the stage source file's fingerprint changes.

**Serialize at stage boundaries, not at instruction level.** The serialization overhead is
incurred once per stage transition (lexer → parser → type checker), not once per instruction
or per function. For files over 100 lines, the stage transition cost is amortized over
thousands of tokens/nodes and stays below the 5% threshold.

**Identify stages by query kind name, not by file name.** The `--stage=lexer:tml` flag does
not say which `.tml` file implements the lexer. The stage runner looks up a canonical path:
`tools/stages/<name>_stage.tml`. This makes the interface deterministic and avoids the
path ambiguity that would arise if users could specify arbitrary file paths.

**Phase12d IR-diff as the correctness gate.** The integration test (Phase 5 item 5.3) must
use `tml ir-diff` to compare LLVM IR from the pure C++ pipeline against the hybrid pipeline.
A passing test suite alone is not sufficient — two pipelines can both pass tests while producing
different IR that coincidentally computes the same result on test inputs.

## Files to Create/Modify

**Created (C++)**:
- `compiler/src/pipeline/tml_stage.cpp` — `TmlStage` class: compile, cache, launch, serialize
- `compiler/include/pipeline/tml_stage.hpp` — public API: `TmlStage::run(input) -> output`

**Modified (C++)**:
- `compiler/src/cli/dispatcher.cpp` — parse `--stage=<name>:tml` flags, populate
  `Options::stage_overrides` map
- `compiler/include/cli/options.hpp` — add `stage_overrides: std::map<std::string, StageImpl>`
- `compiler/src/query/query_context.cpp` — check `stage_overrides` map before each `force<>()`
  call; delegate to `TmlStage::run()` if override is set
- `compiler/CMakeLists.txt` — add `pipeline/tml_stage.cpp` to build target

**Created (TML)**:
- `tools/stages/lexer_stage.tml` — minimal TML lexer stage for integration testing

## Success Criteria

- `tml build --stage=lexer:tml .sandbox/hello.tml` compiles successfully and produces a working
  executable with correct output
- IR-diff (phase12d): LLVM IR from pure C++ pipeline vs hybrid pipeline with TML lexer stage
  must be bitwise identical (exit 0)
- Stage subprocess launch + serialization round-trip overhead < 5% of total compile time for
  files over 100 lines (measured via `mcp__tml__project_slow-tests`)
- Stage cache hit: second build with `--stage=lexer:tml` does NOT recompile the lexer stage
  (verified by checking that `.incr-cache/stages/lexer.exe` timestamp does not change)
- `--no-cache` does NOT clear stage caches in `.incr-cache/stages/`
- Unknown `--stage=<name>:tml` with an unrecognized stage name produces a clear error listing
  valid stage names

## Dependencies

**Blocks**: Era 1 Phase 1 — lexer/parser integration (the TML lexer can only be integrated
and verified once the hybrid pipeline framework is available to run it alongside the C++ stages).

**Depends on**: phase12e (AST & TypeEnv binary serializers — the stage subprocess interface
contract depends entirely on having a stable binary format for ASTs and TypeEnvs at each stage
boundary), phase12d (IR-diff tool — the correctness gate for the integration test in Phase 5).
