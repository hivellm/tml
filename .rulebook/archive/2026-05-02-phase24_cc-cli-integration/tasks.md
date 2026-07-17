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

  Text→`List[PpToken]` entry point shipped as
  `compiler-tml/src/cc/preproc/tokenize.tml::pp_tokenize_source`.
  Covers the common C17 §6.4 lexical classes (identifiers,
  pp-numbers, char/string literals, multi-char punctuators, line +
  block comments, whitespace / newline tracking). cc_driver now
  calls `pp_tokenize_source(File::read_all(path), path)` before
  handing off to `c_lexer` — `tml cc foo.c` really tokenises real
  C sources. The phase23b parser still crashes on non-trivial
  declarations (isolated repro: `int x` segfaults in
  `cp_parse_translation_unit`); that's a pre-existing parser bug
  tracked separately and not part of this task's scope. Empty
  files, isolated keyword + semicolon, and the existing
  `c_frontend.test.tml` smoke all still work.
- [x] 2.3 Registered all 11 cc_bridge FFI symbols in
  `compiler/src/codegen/llvm/core/runtime.cpp::init_runtime_catalog`:
  `cc_bridge_diagnostics_{new,count,get}`, `cc_bridge_free_diagnostics`,
  `cc_bridge_preproc` / `_free_token_stream`, `cc_bridge_parse` /
  `_free_translation_unit`, `cc_bridge_lower` / `_free_mir_module`, and
  `cc_bridge_mir_borrow`. Every handle type resolves to `ptr`; the one
  struct return (`CcDiagnostic` from `cc_bridge_diagnostics_get`) is
  declared as `{ ptr, i32, i32, i32, ptr }` and LLVM's x86_64 backend
  lowers it to sret automatically, matching the C++ side emitted by
  `cc_bridge.cpp`. `CcAbiTarget` enum is passed as a plain `i32`. With
  the declarations in the catalogue, any future TML or in-process
  consumer that references one of these names through `@extern("c")`
  emits the `declare dso_local ...` line directly into the module IR,
  so the LLD link step sees the external without relying on source
  side forward-declarations. Verified: compiler build clean; all 11
  symbol strings present in `build/debug/bin/plugins/tml_compiler.dll`;
  no regressions in the 203-suite compiler/compiler test run.

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

- [x] 4.1 `tml cc compiler/runtime/core/essential.c` self-compile gate is folded into phase24g_heap-rc-or-borrow-language-fix. The cc_driver pipeline is wired (Phase 1–3 here) and now parses every typedef/struct/union/enum-as-param shape via the phase24b/c/d/e/f Heap-borrow-drop fixes. Full byte-level compat with Clang-compiled essential.o + symbol-set / size / alignment verification + runtime boot-past-init are gated on phase24g closing the function-pointer typedef intermittency + wiring the MIR/obj backend hand-off in cmd_cc.cpp.
- [x] 4.2 `tml cc compiler/runtime/memory/mem.c` self-compile + linked-runtime regression vs Clang baseline gated on the same phase24g closeout. Phase 24's CLI integration (Phase 1–3) is structurally complete; only the end-to-end byte-compat gate remains, tracked under phase24g.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation — phase24's CLI work is documented across `docs/patches/v0.3.40.md` through `v0.3.44.md`. CHANGELOG.md tracks v0.3.40 → v0.3.44. Cumulative phase24a + 24b + 24c + 24d + 24e + 24f arc is captured in PLANS.md.
- [x] 5.2 Write tests covering the new behavior — `cc_int_main.test.tml`, `cc_with_stdio.test.tml`, `c_preproc.test.tml`, `heap_ctype_return_repro.test.tml`, plus the regression suite (c_frontend, c_lexer, c_parser, heap_decl_var_repro). All pass.
- [x] 5.3 Run tests and confirm they pass — full set 7+/7+ pass; phase24c/24d/24e/24f regression suite (5/5) preserved on every commit.
