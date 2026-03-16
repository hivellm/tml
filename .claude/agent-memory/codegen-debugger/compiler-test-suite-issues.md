---
name: Compiler test suite collision patterns
description: Compiler tests fail in suite mode due to duplicate symbols and missing test entry wrappers in MIR codegen
type: project
---

## MIR Codegen Missing tml_test_0 Entry

Test files without `use test` go through MIR codegen. The MIR path has `emit_test_entry_wrapper()`
which generates the `tml_test_0` function, but this only works when `options_.test_entry_name` is
set by the test harness. The incremental cache can serve stale results compiled without the entry.

**Fix applied**: Added `use test` to 12 compiler test files so they use AST codegen which always
generates proper test entries.

**Root cause (unfixed)**: MIR codegen's `emit_test_entry_wrapper` in `mir_codegen.cpp:408-457`
works correctly when invoked, but the incremental cache may serve results from a previous
compilation that didn't have `test_entry_name` set.

## Suite Symbol Collisions

Compiler tests define `impl` blocks on primitive types (e.g., `impl I32 { pub const MIN... }`)
which produce external symbols. When multiple test files are merged into one DLL (suite mode),
these symbols collide.

**Fix applied**: Force `max_per_suite=1` for `compiler_compiler` group in `testing_discovery.cpp`.

## --no-suite CLI Flag

The `--no-suite` flag was documented in help text but not implemented. Now wired up:
- `cmd_test.cpp`: parse `--no-suite` and `--suite-mode` flags
- `cmd_test.hpp`: `suite_mode = true` (default preserves current behavior)
- `cmd_test.cpp:259`: `tc.max_per_suite = opts.suite_mode ? 10 : 1`

## Remaining Unfixable Tests (4)

- `iter_from_fn`: needs `where F = func() -> Maybe[T]` constraint support
- 3 zstd tests: need zlib/zstd/brotli runtime libraries linked into test DLL
