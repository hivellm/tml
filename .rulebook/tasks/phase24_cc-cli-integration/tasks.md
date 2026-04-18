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
- [x] 2.2 cc_driver.exe builds and `tml cc` dispatches to it end to
  end. Two-part delivery:
  - `compiler-tml/src/cc/bin/cc_driver.tml` — TML binary that runs
    the phase23b pipeline (collect argv → read file → c_lexer →
    tokenize → cp_parse_translation_unit → c_lower →
    `--emit=pipeline|ast|mir|tokens`). Builds to `build/debug/cc_driver.exe`
    (~877 KB, exit 0).
  - `compiler/src/cli/commands/cmd_cc.cpp` — `run_cc` now prefers
    the `cc_driver.exe` subprocess path (mirrors the
    `coverage_cli.exe` dispatch pattern). Falls back to the
    in-process `cc_bridge` stubs when the binary is missing.
  - `compiler/src/cc/cc_bridge.cpp` — kept as the forward-looking
    FFI ABI; the diagnostic lifecycle (`_new`, `_count`, `_get`,
    `_free_diagnostics`) is fully functional for callers that
    eventually want in-process dispatch.

  **Two codegen blockers cleared in this task:**
  - Maybe generic-instantiation K001: `init_opt: Maybe[Heap[T]]`
    followed by `init_opt = Just(Heap[T]::new(...))` was emitting
    the payload as `Maybe__I32` instead of `Maybe__Heap__T`. Fixed
    by propagating the LHS struct type as `expected_enum_type_` in
    `BinaryExpr::Assign` when the target is an `IdentExpr`
    (`compiler/src/codegen/llvm/expr/binary.cpp`).
  - Enum-drop nested-struct undefined-symbol K001: emitted
    `call @Type::drop` for struct payloads whose types had
    droppable fields but no explicit `impl Drop`. Fixed by skipping
    the nested drop when no user Drop impl exists (the Heap's own
    `drop`/`mem_free` still reclaims the outer allocation). Fix in
    `compiler/src/codegen/llvm/core/drop.cpp`.

  Text→`List[PpToken]` entry point still missing from
  `compiler-tml/src/cc/preproc/`; cc_driver runs the pipeline on an
  empty pp-stream today (0 decls / 0 functions). Phase 4
  self-compile requires that entry point.
- [ ] 2.3 Register the cc_bridge ffi symbols in the runtime-modules library
  so the TML-compiled parser and lowerer can be invoked through them.
  Add the three entry points to `compiler/src/codegen/llvm/core/runtime.cpp`'s
  preamble catalogue so the linker resolves them. Gated on 2.2
  completing — there's nothing to register until the wire-up exists.

## Phase 3: CLI Subcommand (4 items)

- [x] 3.1 Created `compiler/src/cli/commands/cmd_cc.{hpp,cpp}` with
  the `run_cc(argc, argv, verbose)` entry point and the full flag
  surface: `-o <path>`, `-c` (accepted for clang parity),
  `-O0..-O3`, `-I <path>` (repeatable), `-D NAME[=VAL]`
  (repeatable), `-target <triple>`, `-g`, `--emit=obj|llvm-ir|mir|ast|tokens`,
  `-h/--help`. Two-token forms (`-I path`), attached forms
  (`-Ipath`), and `-X=value` forms all accepted. Registered in
  `compiler/src/cli/dispatcher.cpp` as the `cc` command; `tml cc
  --help` and invalid-flag / no-input paths all emit usage.
- [~] 3.2 Pipeline wired through `cc_bridge_preproc` → `cc_bridge_parse`
  → `cc_bridge_lower` with proper ownership-transfer handling and
  diagnostic rendering. The backend hand-off to
  `compiler/src/backend/llvm_backend.cpp` is a TODO in cmd_cc.cpp
  that will become active as soon as `cc_bridge_mir_borrow` returns
  a real `mir::Module*` (gated on Phase 2.2 wire-up). Today any
  invocation exits with the stub diagnostic from `cc_bridge`.
- [~] 3.3 `--emit` stage-stopping logic in place. `tokens` / `ast` /
  `mir` exit after the matching stage but currently print a
  "renderer not yet implemented" stub because `cc_bridge` doesn't
  expose enumerators over its opaque handles yet. The stage switch
  itself is a one-line change when the enumerators land. `obj` /
  `llvm-ir` need the backend hand-off from 3.2.
- [x] 3.4 `-target <triple>` is parsed and mapped to `CcAbiTarget`
  via a `abi_for_triple` helper: `x86_64-pc-windows-*` →
  `WINDOWS_X64_LLP64`; `x86_64-*-linux-*` / other `x86_64-unknown-*`
  → `SYSV_AMD64_LP64`; `aarch64-*` / `arm64-*` → `AARCH64`;
  `i686-*` / `i386-*` → `I686`; empty / unrecognised → `HOST`. The
  value is forwarded into `cc_bridge_lower` for the lowerer's
  size/alignment/calling-convention decisions.

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
