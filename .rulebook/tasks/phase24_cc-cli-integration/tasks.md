# Tasks: `tml cc` CLI — drive the phase23b C frontend end-to-end

**Status**: Planned (0/14)
**Depends on**: phase23b (C frontend TML modules — complete)
**Blocks**: phase23c (C++ subset frontend — extends this CLI)
**Duration**: 2–3 weeks
**Risk**: Medium — codegen bug fixes are open-ended; CLI wiring is mechanical
**Output**: ~1,500 LOC C++ + ~200 LOC TML

---

## Phase 1: TML Codegen Bug Fixes (3 items)

These three bugs in `compiler/src/codegen/` prevent the phase23b TML-compiled
parser from running on non-trivial inputs. Each has an isolated reproducer
in `compiler-tml/tests/native/c_parser.test.tml` that the fix must make
pass.

- [ ] 1.1 Fix codegen bug #7: enum-variant pattern-binding on non-heap fields
  of a variant whose first payload is a `Heap[T]`. Reproducer: uncomment
  the `Func(Heap[CDeclarator], List[CParam], I64)` → `Func(_, ps, _)` form
  in `parser.tml::declarator_func_params` (currently uses the `CFuncDeclPart`
  struct workaround) and run the c_parser test suite — it should still
  pass with the direct pattern.
- [ ] 1.2 Fix codegen bug #8: deeply-nested constructor expressions. The
  duplicate codegen path recurses through each nested enum / struct
  payload without a Heap boundary, causing hang or crash on forms like
  `decls.push(Heap[CDecl]::new(CDecl::Var(Heap[CVarDecl]::new(vd))))`.
  Reproducer: collapse the stepwise let-bindings in `parser.tml::cp_parse_top_decl`
  back to a single-line nested constructor call — c_parser test suite must
  still pass.
- [ ] 1.3 Fix codegen bug #9: large-enum by-value struct payload duplicate
  crash. The enum layout inlines every variant's payload bytes even when
  the variant is not active, which multiplied by the per-payload duplicate
  recursion causes stack blowup. Reproducer: remove the `Heap[T]` wrappers
  on the `CDecl` variants in `ast.tml` (`Var(CVarDecl)` instead of
  `Var(Heap[CVarDecl])`) and run the c_parser test suite.

## Phase 2: FFI Bridge (3 items)

- [ ] 2.1 Create `compiler/include/cc/cc_bridge.hpp` with a plain-C ABI
  surface: `CcTokenStream`, `CcTranslationUnit`, `CcMirModule` opaque
  handle types; `cc_bridge_preproc`, `cc_bridge_parse`, `cc_bridge_lower`,
  and three `cc_bridge_free_*` functions. Document the ownership contract
  in each function's doc comment (caller frees everything it receives).
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
