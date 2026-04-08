## 1. Phase 1 — Diagnostic infrastructure (unblocks debugging)

- [ ] 1.1 Add `--debug-codegen-timing` flag that prints per-function
      MIR→LLVM lowering time, so we can confirm which pass/function is
      responsible for the timeout observed in C1/C5.
- [ ] 1.2 Add `--dump-dead-functions` flag that lists functions removed
      by `dead_function_elimination` and those kept as roots. Use this
      to verify C5.

## 2. Phase 2 — C5: Per-binary dead function elimination

- [ ] 2.1 Reproduce C5: build `compiler-tml/tests/serial/ast_roundtrip.test.tml`
      with the 139-arm dispatch present in `compiler-tml/src/token.tml`
      and confirm codegen timeout even though the test does not call it.
- [ ] 2.2 Trace why `dead_function_elimination` is not removing
      `token_kind_to_tag` in this test binary. Check root set
      computation in `compiler/src/mir/passes/dead_function_elimination.cpp`.
- [ ] 2.3 Fix root set: only `main`, `@test`-annotated functions, and
      their transitive call graph are roots. `pub` alone does not keep
      a function alive in a test binary.
- [ ] 2.4 Verify: `ast_roundtrip.test.tml` recompiles green with
      `token.tml` containing any slow function.

## 3. Phase 3 — C1: Large `when`/`if`-chain codegen blowup

- [ ] 3.1 Reproduce C1 with a minimal test: 139-arm `when tag { N => return EnumKind::V, ... }`.
- [ ] 3.2 Identify the pathological pass (probably mem2reg or DSE) via
      `--debug-codegen-timing` from 1.1.
- [ ] 3.3 Add MIR→LLVM lowering pattern: integer `when` with ≥16
      constant arms all returning constants lowers to a single LLVM
      `switch` + jump table.
- [ ] 3.4 For payloadless enum returns, emit a single
      `global [N x i32]` lookup table + indexed load instead of
      N basic blocks.
- [ ] 3.5 Verify: the 139-arm `tag_to_token_kind` compiles in <2s.

## 4. Phase 4 — C2: `enum as I64` LLVM IR width bug

- [ ] 4.1 Reproduce C2 with a minimal test: `return (kind as I64)`
      where `kind: EnumWith139Variants`.
- [ ] 4.2 Fix `MirCodegen::emit_cast` to emit `zext i32 %disc to <dest>`
      when the source is an enum discriminant and destination is wider
      than the discriminant storage width.
- [ ] 4.3 Verify: the I32 trampoline workaround
      (`let t: I32 = kind as I32; return t as I64`) is no longer needed.

## 5. Phase 5 — C3: Pattern match on imported enum

- [ ] 5.1 Reproduce C3 with a two-module test: module A defines `enum E`,
      module B imports `E` and does `when x { E::V => ... }`.
- [ ] 5.2 Trace the `T023` error in
      `compiler/src/types/checker_pattern.cpp` (or equivalent). Enum
      variants should be resolvable through the imported symbol table.
- [ ] 5.3 Fix resolution: `EnumName::Variant` in pattern position
      resolves against imports, not just local scope.
- [ ] 5.4 Verify: the `tag_to_token_kind` dispatch can live in any
      module that imports `TokenKind`.

## 6. Phase 6 — C6: Cyclic type imports

- [ ] 6.1 Reproduce C6: create `ast/exprs.tml` defining `Expr`
      containing `Stmt`, and `ast/stmts.tml` defining `Stmt` containing
      `Expr`. Confirm current rejection.
- [ ] 6.2 Implement two-phase cross-module type resolution in
      `compiler/src/types/checker.cpp`:
      - Phase A: collect all nominal type *names* from all modules in
        the compilation unit.
      - Phase B: resolve type *bodies*, allowing forward references to
        any name collected in phase A.
- [ ] 6.3 Verify: `compiler-tml/src/ast/` can be split into
      `exprs.tml`, `stmts.tml`, `decls.tml` with cyclic imports.

## 7. Phase 7 — C4: Cross-module struct literal construction

- [ ] 7.1 Reproduce C4: `use OtherMod::S; let x = S { field: v }`.
- [ ] 7.2 Fix `TypeChecker::resolve_struct_literal` to accept imported
      `pub type` names.
- [ ] 7.3 Verify: the `module_new` helper boilerplate is no longer
      needed (though it may remain for style).

## 8. Phase 8 — Ergonomics (W1–W6)

- [ ] 8.1 W1: parser accepts `expr : Type` in argument/return position.
- [ ] 8.2 W2: field access `x.field` counts as a use of `x` in the
      live-variable analysis (kills the `S014` false positive).
- [ ] 8.3 W3: bidirectional inference for comparison operators
      propagates the concrete integer type to untyped literals.
- [ ] 8.4 W4: expected type propagates into generic constructor calls
      in return / argument position.
- [ ] 8.5 W5: query cache fingerprint includes transitive content hash
      of dependency modules; test-binary cache invalidates on any
      package-level `.tml` edit.
- [ ] 8.6 W6: `T027 Module not found` error suggests the real package
      namespace from `tml.toml`.

## 9. Phase 9 — Integration verification (self-hosting sanity)

- [ ] 9.1 Restore `compiler-tml/src/token.tml` `tag_to_token_kind` with
      the full 139-arm dispatch using `kind as I64` (no I32 trampoline).
- [ ] 9.2 Move `tag_to_token_kind` into `compiler-tml/src/ast/serial.tml`
      as a cross-module `when` over imported `TokenKind`.
- [ ] 9.3 Split `compiler-tml/src/ast/nodes.tml` into `exprs.tml`,
      `stmts.tml`, `decls.tml` with cyclic imports.
- [ ] 9.4 Re-enable the full Phase 5 round-trip test in
      `compiler-tml/tests/ast/node_roundtrip.test.tml` — both
      `read_token` (full TokenKind reconstruction) and empty-Module.
- [ ] 9.5 Confirm `ast_roundtrip.test.tml` (phase12e) is green again.
- [ ] 9.6 Confirm all nine `compiler-tml/tests/*` suites pass.

## 10. Tail (mandatory — enforced by rulebook v5.3.0)

- [ ] 10.1 Update or create documentation covering the implementation
      (`docs/specs/30-TYPE-CHECKER.md`, `40-CODEGEN.md`, `01-LANGUAGE.md`).
- [ ] 10.2 Write tests covering the new behavior under
      `tests/compiler/regression/` — one minimal repro per C1–C6 and W1–W6.
- [ ] 10.3 Run tests and confirm they pass (target: all regressions
      green, `ast_roundtrip.test.tml` green, `node_roundtrip.test.tml`
      full round-trip green).
