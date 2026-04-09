# Phase 0p — Self-hosting compiler blockers

## Why

During phase13a (TML token/AST data types for the self-hosting frontend)
we hit a cluster of C++ compiler bugs and missing features that make
writing moderate-sized pure-TML code effectively impossible. Each
attempt to express a normal compiler-shaped pattern (large dispatch,
enum→int conversion, modular AST, cross-module pattern match) either
produced invalid LLVM IR, timed out in codegen, or required contorted
workarounds that defeated the purpose of the feature.

This task fixes the blockers in the C++ compiler *first*, so
phase13a/b/c/d (self-hosting frontend) can proceed on a healthy
foundation instead of fighting the tooling on every line.

## What Changes

### Critical blockers (must fix — ordered by dependency)

**C5. Per-binary dead function elimination**
- Symptom: any `.test.tml` that imports anything from `compiler::*`
  forces codegen of every `.tml` file under `compiler-tml/src/`, even
  functions never referenced from the test. Amplifies C1 — one slow
  function in one module breaks every test in the package. Pre-existing
  `ast_roundtrip.test.tml` started timing out just because a sibling
  module gained a slow function.
- Fix: investigate why the `dead_function_elimination` MIR pass is not
  eliminating unreferenced functions per test binary. Candidates: `pub`
  functions kept as roots regardless of reachability; query cache
  pinning modules at package granularity.

**C1. Codegen timeout on large `when`/`if`-chains returning enum variants**
- Symptom: `[X002] Codegen timed out after 30s` on any binary linking a
  module with a ~100+ arm `when` / `if`-chain returning payloadless
  enum variants.
- Repro: `compiler-tml/src/token.tml` 139-arm `tag_to_token_kind` over
  `TokenKind`. Splitting into 5 sub-functions of 28 arms does not help.
  `if tag == N { return ... }` chain also explodes.
- Impact: impossible to write lexer tables, pretty-printers, keyword
  dispatchers — core self-hosting lexer/parser building blocks.
- Root cause hypothesis: MIR→LLVM emits one basic block + enum store
  per arm; a subsequent O(n²) pass (mem2reg / DSE) blows up.
- Fix: lower integer `when` with N ≥ 16 constant arms to an LLVM
  `switch` + jump table. For payloadless enum returns, emit a single
  `global [N x i32]` lookup table and an indexed load.

**C2. `enum_value as I64` emits invalid LLVM IR**
- Symptom: `error: '%tN' defined with type 'i32' but expected 'i64'`
  — zext width wrong.
- Repro: `return kind as I64` where `kind: TokenKind`. Workaround:
  `let t: I32 = kind as I32; return t as I64`.
- Fix: `MirCodegen::emit_cast` — when source is an enum discriminant
  and destination wider than i32, emit `zext i32 %disc to <dest>`.

**C3. Pattern match on imported enum rejects `Mod::Variant`**
- Symptom: `error[T023]: Unknown enum type 'TokenKind' in pattern`
  when `when x { TokenKind::Eof => ... }` is used in a module that
  imports `TokenKind` but does not define it.
- Impact: forces all dispatch over an enum to live in the enum's
  defining module. Root cause of why C1 could not be worked around —
  the dispatch could not be moved out of `token.tml`.
- Fix: `TypeChecker::resolve_pattern_enum_variant` — resolve the
  `EnumName::Variant` path against the imported symbol table.

**C6. TML does not support cyclic type imports between modules**
- Symptom: splitting the AST into `ast/exprs.tml`, `ast/stmts.tml`,
  `ast/decls.tml` fails because `Expr` contains `Stmt` and vice versa.
- Workaround used: consolidate ~40 mutually-recursive AST types into
  a single ~760-line `ast/nodes.tml`. Will get worse as HIR/MIR land.
- Fix: two-phase cross-module type resolution — (1) declare all
  nominal type names, (2) resolve bodies. Extend `resolve_imported_symbol`
  to permit forward-declared types at the type level.

**C4. Cross-module struct literal construction**
- Symptom: `error[T022]: Unknown struct or class: Module` on
  `Module { ... }` despite `use compiler::ast::nodes::Module`.
- Workaround: mark `pub type` + hand-write `pub func module_new(...)`.
- Fix: `pub type T { ... }` should authorize cross-module literal
  construction. Alternative: auto-generate a public `T::new(...)`.

### Ergonomics (important, not critical — ship with C1–C6)

**W1. Type ascription in expression position: `expr :Type`**
- Currently only in `let x: T = ...`. In argument position
  (`f(1 :I64)`) it parse-errors.
- Fix: parser accepts `expr : Type` as a type-ascription expression.

**W2. `S014 Unused variable` false-positive on fields**
- `loc: SourceLocation` is flagged unused even when `loc.file`,
  `loc.line`, etc. are read.
- Fix: field access `x.field` counts as a use of `x`.

**W3. Integer literal default I32 does not propagate across comparisons**
- `var i: I64 = 0; loop (i <= 138)` fails because `138` is I32.
- Fix: bidirectional inference — propagate concrete integer type from
  one side of a comparison to untyped literals on the other.

**W4. Expected-type propagation into generic constructor calls**
- `let x: List[Heap[Decl]] = List::new(0)` works but
  `return List::new(0)` with matching return type does not.
- Fix: propagate expected type into generic constructor inference.

**W5. Query cache + test exe cache stale across source edits**
- `[incr] GREEN: reusing cached codegen result` replayed broken IR
  after edits. Manual `rm -rf build/debug/cache/incr tests.json` needed.
- Fix: query fingerprint must include transitive content hash of all
  dependency modules; test-binary cache must invalidate when any `.tml`
  in the package changes.

**W6. Package name vs directory name error message**
- `T027: Module 'compiler_tml::source::SourceSpan' not found` gives no
  hint that the package is `compiler::` via `tml.toml`.
- Fix: error suggests `did you mean compiler::source::SourceSpan?`.

## Impact

- Affected specs: `docs/specs/01-LANGUAGE.md` (W1), `docs/specs/40-CODEGEN.md`
  (C1, C2), `docs/specs/30-TYPE-CHECKER.md` (C3, C4, C6, W3, W4),
  `docs/specs/33-SERIAL-FORMAT.md` (unblocks phase13a continuation).
- Affected code: `compiler/src/codegen/mir/` (C1, C2),
  `compiler/src/mir/passes/dead_function_elimination.cpp` (C5),
  `compiler/src/types/checker*.cpp` (C3, C4, C6, W3, W4),
  `compiler/src/parser/parser_expr.cpp` (W1),
  `compiler/src/lint/` (W2), `compiler/src/query/` (W5),
  `compiler/src/lexer/` or diagnostics (W6).
- Breaking change: NO (pure bugfix + feature additions; no existing
  valid program stops compiling).
- User benefit: unblocks phase13a/b/c/d self-hosting frontend; every
  non-trivial pure-TML codebase (compilers, parsers, IR builders)
  becomes viable instead of fighting workarounds.

## Success criteria

1. `compiler-tml/src/token.tml` can define a 139-arm dispatch function
   over `TokenKind` and have a test binary compile in <10s.
2. `enum_value as I64` produces valid LLVM IR without the I32 trampoline.
3. `when imported_enum { Mod::Variant => ... }` compiles cleanly.
4. `compiler-tml/src/ast/` can be split into `exprs.tml`, `stmts.tml`,
   `decls.tml` with cyclic type imports.
5. Pre-existing `ast_roundtrip.test.tml` (phase12e) is green again.
6. Editing a `.tml` file reliably invalidates dependent test binaries
   on the next `tml test` run without manual cache deletion.

## Source

Distilled from the phase13a session on 2026-04-08 where every blocker
above was hit at least twice. phase13a is paused until this task lands.
