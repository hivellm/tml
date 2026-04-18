# Tasks: `tml cc` CLI — drive the phase23b C frontend end-to-end

**Status**: Planned (0/14)
**Depends on**: phase23b (C frontend TML modules — complete)
**Blocks**: phase23c (C++ subset frontend — extends this CLI)
**Duration**: 2–3 weeks
**Risk**: Medium — codegen bug fixes are open-ended; CLI wiring is mechanical
**Output**: ~1,500 LOC C++ + ~200 LOC TML

---

## Phase 1: TML Codegen Bug Fixes (3 items — COMPLETE in phase0v)

These three bugs were fixed under `phase0v_codegen-bringup-bugs` (archived
2026-04-18). The workarounds in `compiler-tml/src/cc/` have been un-applied
so the natural forms are now the regression guard.

- [x] 1.1 Bug #7 fix committed as `880dfbba` — enum-variant pattern-binding
  on non-heap fields of a variant whose first payload is a `Heap[T]`. Root
  cause was the multi-field binding branch in `when.cpp::gen_when` requiring
  `payload[0]->is<IdentPattern>()`. Regression test at
  `compiler/tests/compiler/enum_pattern_bind_multiple_fields.test.tml`.
- [x] 1.2 Bug #8 fix absorbed into `880dfbba` — deeply-nested constructor
  expressions. This was a secondary manifestation of bug #7 (the pattern
  binding failure generated malformed IR that crashed downstream duplicate
  calls). Once #7 was fixed, the one-line nested-constructor form worked
  without any source-level workaround. Regression test at
  `compiler/tests/compiler/nested_constructor_push.test.tml`.
- [x] 1.3 Bug #9 fix absorbed into `880dfbba` — large-enum by-value struct
  payload duplicate crash. Same cascading story. Workarounds un-applied in
  `compiler-tml/src/cc/` under commit `cc9fb6fc`. Regression test at
  `compiler/tests/compiler/large_enum_by_value_duplicate.test.tml`.

## Phase 2: FFI Bridge (3 items)

- [x] 2.1 Created `compiler/include/cc/cc_bridge.hpp` with the plain-C
  ABI surface: opaque handle types `CcTokenStream`,
  `CcTranslationUnit`, `CcMirModule`, `CcDiagnostics`; pipeline entry
  points `cc_bridge_preproc` / `cc_bridge_parse` / `cc_bridge_lower`;
  matching `cc_bridge_free_*` destructors; `CcAbiTarget` enum
  (HOST / Windows x64 LLP64 / SysV AMD64 LP64 / i686 / aarch64);
  diagnostic accumulator with `CcDiagnostic` records; and a
  `cc_bridge_mir_borrow` accessor that returns a `const mir::Module*`
  so the existing LLVM backend can consume it without copying.
  Ownership contract documented: single-owner handles, transfer on
  successful consumption, caller retains ownership on failure.
- [ ] 2.2 Create `compiler/src/cc/cc_bridge.cpp` implementing the handle
  types and dispatching each bridge call to the matching TML entry point
  registered via `tml_register_extern`. `cc_bridge_preproc` drives the
  phase23a preprocessor (already C++) and wraps its output in an opaque
  `CcTokenStream`. `cc_bridge_parse` and `cc_bridge_lower` call into
  TML-compiled entry points and wrap the TML heap pointers.
- [ ] 2.3 Register the cc_bridge ffi symbols in the runtime-modules library
  so the TML-compiled parser and lowerer can be invoked through them.
  Add the three entry points to `compiler/src/codegen/llvm/core/runtime.cpp`'s
  preamble catalogue so the linker resolves them.

## Phase 3: CLI Subcommand (4 items)

- [ ] 3.1 Create `compiler/src/cli/commands/cmd_cc.cpp` with the `cc`
  subcommand entry point and flag table: `-o`, `-c`, `-O0`/`-O1`/`-O2`,
  `-I <path>`, `-D <name>[=<val>]`, `-target <triple>`, `-g`, `--emit=<what>`.
  Register it in the CLI dispatch table in `compiler/src/cli/main.cpp`
  alongside `tml compile` / `tml build` / `tml test`.
- [ ] 3.2 Wire the pipeline: read file bytes → call `cc_bridge_preproc`
  with include paths and defines → pass the token stream to
  `cc_bridge_parse` → pass the AST to `cc_bridge_lower` → hand the
  `MirModule` to the existing MIR → LLVM backend (`compiler/src/backend/llvm_backend.cpp`)
  → write `.obj` via the existing LLD linker integration.
- [ ] 3.3 Implement `--emit` forms: `tokens` prints the token stream,
  `ast` prints the `CTranslationUnit`, `mir` prints the lowered MIR,
  `llvm-ir` prints the emitted LLVM IR before linking, `obj` is the
  default and writes the output file. Each form exits early without
  invoking downstream stages.
- [ ] 3.4 Wire `-target <triple>` to select the `CAbiTarget` passed to
  `c_lower` (Windows x64 LLP64 vs System V AMD64 LP64) and to pass the
  matching target triple to the LLVM backend. Default to host triple.

## Phase 4: Self-Compilation Gate (2 items)

The acceptance criteria from the proposal. Each item exercises the full
pipeline against a real C runtime file and verifies byte-level or
behavioural compatibility with the current Clang-produced artifact.

- [ ] 4.1 `tml cc compiler/runtime/core/essential.c` produces an `.obj`
  that (a) contains the same symbol set as the Clang build, (b) has
  matching sizes / alignments for exported types, and (c) when linked
  into the TML test runner binary produces a binary that boots past
  the runtime initialization phase and can run `tml test --filter
  simple_smoke` without runtime crashes.
- [ ] 4.2 `tml cc compiler/runtime/memory/mem.c` produces an `.obj` that
  links with `essential.o` (both TML-compiled). The combined runtime,
  linked into `tml.exe`, must pass the full TML test suite with zero
  regressions vs the Clang-compiled baseline. This is the primary
  acceptance gate — phase 24 is not complete until this passes.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
