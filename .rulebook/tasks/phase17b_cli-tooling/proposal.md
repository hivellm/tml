# Proposal: CLI, Testing, and Formatter — Rewrite in TML

## Why

A compiler is only as usable as its tooling. The TML compiler's user-facing surface — the CLI,
diagnostic renderer, test runner, and code formatter — is currently entirely implemented in C++.
For the self-hosted compiler (phase17c) to replace the C++ compiler as the primary tool, it must
provide identical behavior across all subcommands: `build`, `run`, `check`, `test`, `fmt`, `lint`.
The test runner is especially critical: tml-stage1 must be able to run the full 1,700+ test suite
to verify its own correctness. Without a TML-native test coordinator, bootstrap verification
cannot be automated. The formatter is required for code quality enforcement of the TML compiler
source itself.

## What Changes

Port the user-facing CLI layer (~37,940 C++ LOC) to TML (~24,660 TML LOC, partial):

- `compiler/src/cli/dispatcher.cpp` → `compiler-tml/src/cli/mod.tml` (subcommand routing)
- `compiler/src/cli/diagnostic.cpp` → `compiler-tml/src/cli/diagnostic.tml` (error rendering)
- `compiler/src/cli/build.cpp` + `object_compiler.cpp` → `compiler-tml/src/cli/builder.tml`
- `compiler/src/testing/testing_coordinator.cpp` (1,447 LOC) → `compiler-tml/src/testing/mod.tml`
- `compiler/src/testing/testing_coverage.cpp` (1,153 LOC) → coverage tracking in TML
- `compiler/src/testing/testing_process.cpp` (849 LOC) → subprocess launch + NDJSON parse in TML
- `compiler/src/format/format_expr.cpp` (475 LOC) + `format_decl.cpp` (363 LOC) → `compiler-tml/src/format/mod.tml`

Key design decisions:
- NDJSON test protocol preserved unchanged — subprocess model is correct and proven
- Diagnostics use ANSI escape codes via `std::console` or direct Str embedding
- Formatter uses AST → pretty-print (not source text transform) — AST is already available
- Build system invokes C++ shim for LLVM IR → .obj and LLD → .exe (ERA 2 replaces these)
- `build.tml` detection and execution handled in TML using `std::process`

## Impact

- Affected specs: `docs/specs/cli-reference.md`, `docs/specs/test-protocol.md`
- Affected code: `compiler-tml/src/cli/`, `compiler-tml/src/testing/`, `compiler-tml/src/format/` (all new)
- Breaking change: NO — C++ compiler remains primary until phase17c completes
- User benefit: tml-stage1 becomes a fully usable compiler tool, not just a code generator;
  enables automated bootstrap verification in phase17c

## Success Criteria

- `tml-stage1 build samples/hello.tml` produces a binary identical to `tml.exe build samples/hello.tml`
- `tml-stage1 test --suite core/str` reports the same pass/fail counts as the C++ compiler
- `tml-stage1 fmt --check lib/core/src/str.tml` exits 0 (formatter agrees with C++ formatter)
- Diagnostics display source spans, error codes, and ANSI colors matching C++ output

## Dependencies

- **Requires**: phase17a complete (query system provides force(CodegenUnit) for build command)
- **Blocks**: phase17c (bootstrap: tml-stage1 CLI must be able to build and test TML projects)
