# Proposal: Frontend Integration — Wire TML Lexer/Parser into Compiler

**Task**: phase13d_frontend-integration
**Status**: Planned
**Priority**: P0
**Estimated effort**: 2-3 weeks
**Risk**: Medium

## Problem

The TML lexer and parser (phases 13b/13c) exist as standalone TML code, but they
have no connection to the real compiler pipeline. Until they are wired in and
validated against the full test suite, they are unverified and cannot be relied upon.

This task uses the hybrid pipeline framework from phase12f as the integration
mechanism: the TML frontend binary runs as a subprocess, serializes the AST to
stdout, and the C++ side deserializes and continues with type checking. Without this
wiring, Phase 1 of self-hosting is incomplete and Phase 2 (porting the type checker)
cannot begin.

The risk is Medium because serialization edge cases are hard to anticipate. Unusual
AST shapes (deeply nested generics, complex pattern bindings, OOP node combinations)
may expose gaps between what the TML serializer writes and what the C++ deserializer
expects.

## Proposed Solution

**TML frontend binary** (`main_frontend.tml`) — a standalone executable that:
1. Reads a `.tml` source file path from CLI args.
2. Loads the file content into a `Source`.
3. Calls `Lexer.new(source).tokenize()` to get `List[Token]`.
4. Calls `parse_module(tokens)` to get `Module`.
5. Serializes the `Module` via `BinaryWriter` and writes the bytes to stdout.
6. On lex or parse error: writes structured JSON to stderr and exits with code 1.

**C++ integration** (`query_context.cpp`) — a new `ParseModuleTml` query that:
1. Spawns the TML frontend binary as a subprocess with the source file path as arg.
2. Captures stdout as the raw binary AST stream.
3. Deserializes via the phase12e C++ deserializer into a C++ `Module` struct.
4. Returns the `Module` to the query system for the normal type-checking pipeline.

A `--stage=parser:tml` CLI flag dispatches `ParseModule` queries to `ParseModuleTml`.
The C++ frontend remains active under `--stage=parser:cpp` (the current default) as
a fallback throughout this phase.

**Differential testing** — the full test suite (~1,700 tests) is run with
`--stage=parser:tml`. Failures are categorized into lexer bugs, parser bugs,
serialization bugs, or deserialization bugs and fixed one commit at a time. After all
tests pass, an IR-diff verifies that the LLVM IR output is byte-identical between the
C++ path and the TML frontend path for every test file.

**Performance validation** — benchmarking 50 stdlib modules via both paths. The
subprocess + serialization overhead of the TML frontend must not exceed 5% of the
total compile time. If exceeded, serialization and deserialization hotspots are
profiled and optimized before switchover.

**Switchover** — once all tests pass and performance is validated, `--stage=parser:tml`
becomes the default. The C++ lexer and parser are retained as `--stage=parser:cpp`
but are no longer exercised by the default test run.

## Key Decisions

- **Subprocess boundary, not in-process FFI** — the TML frontend runs as a separate
  process. This provides a clean isolation boundary: crashes in the TML frontend
  don't corrupt the C++ compiler process, and the communication contract (binary
  protocol on stdout) is explicit and testable.

- **Errors as JSON on stderr** — parse errors are emitted as a JSON array of
  `{message, file, line, col}` objects so the C++ side can surface them using the
  existing diagnostic infrastructure without a custom binary error protocol.

- **IR-diff as the correctness gate** — comparing serialized ASTs would require a
  perfect C++ AST equality implementation. Comparing final LLVM IR output is
  stronger: it verifies that the TML frontend + C++ type checker + codegen pipeline
  produces functionally identical code to the pure C++ path.

- **5% overhead gate before switchover** — the subprocess model has inherent
  overhead (process spawn, pipe I/O, deserialization). If this exceeds 5%, the
  primary optimization target is the serialization format (e.g., switching from
  recursive to iterative node encoding to reduce stdout write calls).

- **C++ frontend as permanent fallback** — the C++ lexer/parser are never deleted
  during Phase 13. They serve as the ground truth for differential testing and as
  an escape hatch if a TML frontend regression is discovered after switchover.

## Files to Create/Modify

| File | Action | Notes |
|------|--------|-------|
| `compiler-tml/src/main_frontend.tml` | Create | Standalone binary: lex, parse, serialize, exit |
| `compiler/src/query/query_context.cpp` | Modify | Add `ParseModuleTml` query, subprocess spawn, deserialize |
| `compiler/src/cli/commands/build.cpp` | Modify | Wire `--stage=parser:tml` flag |

## Success Criteria

- `tml build compiler-tml/src/main_frontend.tml` produces a working binary that
  correctly serializes the AST for a trivial TML source file (task 1.4).
- Full test suite passes with `--stage=parser:tml` — zero failures (task 3.4).
- IR-diff confirms LLVM IR is identical between C++ path and TML frontend path for
  all test files (task 3.5).
- TML frontend overhead is less than 5% vs the pure C++ path on 50 stdlib modules
  (task 4.3).
- `--stage=parser:tml` is set as the default after all criteria are met (task 5.1).

## Dependencies

- **Depends on**: phase13b (TML lexer), phase13c (TML parser), phase12f (hybrid
  pipeline framework providing the subprocess spawn and binary serialization
  infrastructure on the C++ side).
- **Blocks**: Era 1 Phase 2 — porting the type checker to TML. This task is the
  final gate before Phase 2 can begin; the TML frontend must be validated in
  production before adding another component to the TML side of the pipeline.
