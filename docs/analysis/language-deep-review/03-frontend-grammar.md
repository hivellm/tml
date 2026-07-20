# 03 — Frontend: Lexer, Parser, Grammar, AST, HIR/THIR, Diagnostics

**Findings:** L-040..L-048 · **Method:** code audit + `check`/`run` probes (exhaustiveness, `?.`) + `sizeof` measurement · **Builds on:** F-001 (`../architecture-performance-review/01-dual-codegen-split.md`).

## Summary

The lexer and parser are the healthiest part of the compiler: a compact (~2.8K + 6.4K LOC) SIMD-accelerated lexer and a hand-written recursive-descent + Pratt parser with real panic-mode recovery and a genuinely good diagnostic renderer. The problems are one layer up: the frontend's semantic core does not *flow* information forward — type checking runs on the raw AST and its results are stored in a `void*`-keyed side map, after which the borrow checker, the HIR builder, the THIR lowering, and the AST-legacy code generator each **re-derive types independently** (4 separate inference implementations, ~2.8K LOC of it living inside the code generator). HIR/THIR is a half-real layer: it desugars some sugar, keeps other sugar as nodes, silently drops `?.` semantics, and hosts the language's only exhaustiveness checker — which probes prove never fires on `tml check`, fires only as a timestamped log line on the MIR path, and is completely absent on the AST path real programs use. Grammar-level, the celebrated "LL(1)" constraint is folklore: the ADR file doesn't exist, and the parser itself uses two-token lookahead plus unbounded backtracking for `type`/`enum` disambiguation, while newline-significance is enforced by 148 hand-placed `skip_newlines()` calls — the mechanism behind the T6 DocComment bug.

---

### L-040 — ADR-008 "LL(1) single-token lookahead" is a phantom document, and the parser violates it anyway

**Impact:** Medium (design distortion) · **Confidence:** High · **Layer:** design

- The ADR does not exist: `docs/adr/` contains only ADR-009 and ADR-010; no `*ADR-008*` file exists anywhere in the repo — yet it is cited as active in `AGENTS.override.md` (T8) and in `docs/analysis/language/ambiguity-review.md:11-16`, where it is used to **reject language proposals** ("MUST BE REMOVED — breaks LL(1)").
- The parser is not LL(1): `peek_next()/check_next()` (`compiler/src/parser/parser_core.cpp:56-61,85`, 8 use sites); `type` alias-vs-sum-type disambiguation scans ahead **unbounded** tracking bracket depth for a top-level `|`, then rewinds (`compiler/src/parser/parser_decl.cpp:185-216`); struct-vs-enum requires entering the braces, skipping newlines *and* doc comments, consuming an identifier, then rewinding (`parser_decl.cpp:224-251`) — a direct consequence of making `enum` a keyword alias of `type` (`compiler/src/lexer/lexer_core.cpp:46`, `docs/specs/03-GRAMMAR.md:116`).

**What earlier models decided & why it conflicts:** LL(1) was adopted as an LLM-determinism principle (RFC-0002:15) but then `type`/`enum` were merged, which is impossible to parse LL(1). The constraint survives only as rhetoric that vetoes ergonomics while the implementation quietly does LL(k)+backtracking. No runtime perf cost; the real cost is design decisions made against a rule that isn't real.

**Recommendation:** write the actual grammar policy down (LL(2) + bounded backtracking at declaration heads), or split `enum` back into its own keyword. Stop using the phantom ADR as a veto.

---

### L-041 — Newline significance implemented as 148 manual `skip_newlines()` sites, with doc comments in the token stream — the root cause of the T6 fragility

**Impact:** Medium-High (correctness fragility, maintenance tax) · **Confidence:** High · **Layer:** design + implementation

- Spec contradicts implementation: `docs/specs/02-LEXICAL.md` §11 says "Spaces and newlines ignored between tokens… Newlines significant only in strings"; the lexer says the opposite: "Newlines are significant in TML for statement separation" (`compiler/src/lexer/lexer_core.cpp:270-272`).
- Newline policy is enforced nowhere central: 148 `skip_newlines` call sites across 9 parser files (`parser_decl.cpp` alone has 40). Each production must individually decide; missing one is a parse bug, over-skipping is the commit `a68f4c4f` bug ("skip_newlines was eating them" — doc comments silently discarded compiler-wide).
- Because `///` doc comments are **tokens** (`DocComment`/`ModuleDocComment`) interleaved with `Newline`, every lookahead must special-case them — see the struct/enum disambiguator's dedicated doc-comment skip loop (`parser_decl.cpp:229-233`) and `collect_doc_comment`'s stateful blank-line logic (`parser_core.cpp:128-160`).

**What earlier models decided:** semicolon-free syntax (good, Go-like) but without Go's single-point terminator-insertion rule; each parser function got discretion instead. The DocComment bug is the predictable failure mode, and more of this class will recur.

**Recommendation:** move newline policy into the lexer (Go-style: emit one `Terminator` token by a fixed rule, e.g. "after identifiers/literals/closing delimiters"), and move doc comments out of the token stream into a position-keyed side table. Deletes ~148 fragile call sites.

---

### L-042 — `?.` optional chaining is implemented in the type checker and AST codegen only; HIR/THIR/MIR ignore the flag entirely

**Impact:** High (silent miscompile class on the "modern" path) · **Confidence:** High (static); Medium (runtime — probe timed out) · **Layer:** implementation

- `?.` is a boolean flag on call/field nodes: `compiler/include/parser/ast_exprs.hpp:208,226` (`bool optional_chain`). Grep of consumers: `types/checker/expr_call_method.cpp`, `expr_ops.cpp`, and AST codegen `codegen/llvm/expr/method.cpp`, `struct_field.cpp`, `infer_methods.cpp` — and **zero occurrences in `src/hir/`, `src/thir/`, `src/mir/`**. `HirBuilder::lower_method_call` (`compiler/src/hir/hir_builder_expr.cpp:575-650`) lowers a plain method call, never reading the flag, so the unwrap-or-Nothing branch is never generated on the MIR path.
- This is the idiom the project *mandates* (`.claude/rules/optional-chaining.md`). It's masked in practice only because naming `Maybe` in a signature routes the program to the AST path (`query_core.cpp:823-847`) — i.e., the pipeline advertised as the future silently drops semantics of a flagship feature.

**Recommendation:** desugar `?.` in exactly one place (HIR build, into a `when`), delete the flag-reading from codegen. This is the test case for L-048's single-lowering rule.

---

### L-043 — Exhaustiveness checking exists only in THIR: silent under `tml check`, a log line on the MIR path, absent on the shipping AST path

**Impact:** Very High (safety diagnostic hole in the language's core idiom) · **Confidence:** High (probe-verified) · **Layer:** implementation (architecture placement)

- The only exhaustiveness engine is `compiler/src/thir/exhaustiveness.cpp` (623 LOC), invoked during HIR→THIR lowering (`compiler/src/thir/thir_lower.cpp:624-628`). `grep exhaust` in `types/checker/` returns nothing.
- Probe results (missing `Color::Blue` arm): `check` → "Type check passed", **silent**. `run` (non-generic → MIR path) → `WARN [thir] non-exhaustive patterns in when expression. Missing: Color::Blue` as a timestamped logger line — no span, no code, no snippet (`query_core.cpp:656-658` demotes THIR diagnostics to `TML_LOG_WARN`) — and the program **compiles and runs, exit 0**. Same program plus one generic function (→ AST path) → **zero output of any kind**.

**Why it conflicts:** TML's whole error-handling story is `Maybe`/`Outcome` + `when`. Rust makes non-exhaustive match a hard error (E0004). Here the check was placed in the one layer the shipping path bypasses (the clearest user-facing casualty of the F-001 split), and even when it runs it can't stop compilation.

**Recommendation:** move (or duplicate-call) exhaustiveness into the type-check phase so `tml check` reports it as a real diagnostic with span + code, deny-by-default.

---

### L-044 — Type information doesn't flow: `void*`-keyed side map + four independent type-inference implementations + ≥4 full AST walks before LLVM

**Impact:** Very High (compile-time cost + the bug factory behind "checker said X, codegen did Y") · **Confidence:** High · **Layer:** implementation (with a design root)

- The checker's per-expression results live in `TypeEnv::expr_types_` — `std::unordered_map<const void*, TypePtr>` keyed by **raw AST node addresses** (`compiler/include/types/env.hpp:865`, accessor `:687`). No stable node IDs exist at AST level; the contract is pointer identity across phases.
- Type-of-expression is re-derived at least 4×: (1) `types/checker/` (11.7K LOC, authoritative); (2) `HirBuilder::get_expr_type` (177-line fallback) plus its own generic re-substitution in `lower_method_call` (`hir_builder_expr.cpp:610-650`); (3) `ThirLower` + `TraitSolver` + normalizer (`thir_lower.cpp:24-25`); (4) the AST code generator's own inference subsystem — `codegen/llvm/expr/infer.cpp` + `infer_methods.cpp` + `infer_types.cpp` = **2,832 LOC with 124 `infer_expr_type` call sites inside codegen**.
- Full AST traversals before IR: typecheck, borrow check (5.7K LOC incl. an AST-based "Polonius", `borrow/polonius_facts.cpp:751`), the codegen-router pre-scan (`query_core.cpp:902-986` walks all decls just to pick a path), then AST→LLVM or HIR→THIR→MIR. Additionally the entire `TypeEnv` (~20 hash maps) is **deep-copied per stage** — `auto env_copy = *tc.env` at `query_core.cpp:585` (HIR) and `:708` (MIR passes) — and the HIR builder's mutations are then discarded (THIR receives the *original* env at `:616-623`).
- **rustc contrast:** stable `HirId`s, one `TyCtxt`, typeck results persisted and queried; codegen never re-infers. Earlier models built each stage's inference locally because nothing carried results forward — locally reasonable, globally quadratic.

**Recommendation:** assign node IDs at parse time, persist checker results in an ID-keyed table, make every downstream stage a consumer. Deleting codegen-time inference (2.8K LOC) is the measurable payoff. (Same root as L-001 in the type-system dive, seen from the pipeline side.)

---

### L-045 — AST memory: 368-byte Expr, 640-byte Decl, one heap allocation per node, no arena, no size regression guards

**Impact:** Medium (constant-factor tax on every walk in L-044) · **Confidence:** High (measured) · **Layer:** implementation

- Measured via `sizeof` probe (zig c++, this machine): `Token=136`, `Expr=368`, `Stmt=192`, `Decl=640`, `Pattern=280`, `Type=328` bytes. `std::variant` is sized by its largest alternative, so a literal `1` costs 368 bytes; children are per-node `Box<T>` (unique_ptr) allocations by design (`compiler/include/parser/ast.hpp:20-25`). Tokens are fat because the value variant embeds `std::string` members (`compiler/include/lexer/token.hpp:305-348,386-387`).
- rustc keeps `ast::Expr` at ~72 bytes and boxes large variants, enforced by static size assertions; TML has no such guards, so nodes will keep growing silently.

**Recommendation:** box the fat variant alternatives (Closure, When, Template) to collapse `Expr` toward ~100B, add `static_assert(sizeof(Expr) <= N)`, and consider a bump arena per module. Cheap, mechanical, and it multiplies across the 4-6 tree walks.

---

### L-046 — A second, C#-style object model (~14 extra keywords) was bolted on against the RFC and leaks into every layer

**Impact:** High (semantic surface ×2, anti-zero-cost defaults) · **Confidence:** High · **Layer:** design

- RFC-0002 defines **41 keywords** and explicitly parks `class`, `virtual`, `override`, `throw` as "Reserved (future use)" (`docs/rfcs/RFC-0002-SYNTAX.md:22-59`). The lexer implements **67** `Kw` entries including `class, interface, extends, implements, override, virtual, abstract, sealed, namespace, protected, static, prop, throw` (`compiler/src/lexer/lexer_core.cpp:120-140`).
- The parallel object model penetrates everything: `parser_oop.cpp` (733 LOC), `ast_oop.hpp` (359), `TypeEnv::classes_/interfaces_/class_interfaces_` (`env.hpp:856-860`), `checker/core_oop.cpp` (1,065 LOC), `HirBuilder::lower_class_to_impl` (155 lines), and a `ClassType` branch inside every method-dispatch site (`hir_builder_expr.cpp:615-616`). The `base` contextual-keyword hack (`lexer_core.cpp:130-135`) and T6's reserved-word landmine are symptoms.

**Why it conflicts:** inheritance + virtual dispatch is the opposite default from the Rust-class zero-cost story, it doubles the method-resolution matrix that L-044 already computes four times, and it contradicts the LLM-first "one way to say it" rationale — a model must now choose between `type+impl+behavior` and `class+interface+extends` for every design.

**Recommendation:** decide. Either freeze/deprecate the OOP layer behind a feature gate and stop paying its cost in every phase, or specify its lowering to the behavior model precisely (it partially exists: `lower_class_to_impl`) and delete the parallel checker/codegen branches.

---

### L-047 — Diagnostics: excellent renderer, but structure is destroyed at query boundaries and half the pipeline never reaches it

**Impact:** Medium-High (UX + tooling) · **Confidence:** High · **Layer:** implementation

- The good half is genuinely good: Rust-style rendering with primary/secondary labels, notes, help, fix-its (insert/replace/delete visualization), JSON mode, Levenshtein did-you-mean, warning levels, `-Werror` (`compiler/src/cli/diagnostic.cpp:1-503,692-818`), 246 registered codes P/T/B/L… (`cli/diagnostic.hpp:90+`) with `tml explain` docs (`cli/explain/backend_errors.cpp`).
- But there is no shared diagnostic context: each stage has its own error struct, and the query layer **flattens them to strings** — TypeErrors pre-formatted into `"file:line:col: [code] msg"` text (`query_core.cpp:455-469`), BorrowErrors reduced to `err.message` with the **span dropped** (`:553-557`), THIR diagnostics are plain strings sent to the logger (`:656-658`, see L-043), codegen K-codes are string concatenation in the backend (`backend/llvm_backend.cpp:282`). Parser `expect()` produces errors with empty notes/fixes in the common path (`parser_core.cpp:97-107`). So the JSON/IDE output only covers whatever happened to be emitted directly, and downstream consumers can't re-render or filter.

**Recommendation:** one `Diagnostic` type end-to-end; query results carry `std::vector<Diagnostic>`, stringification happens exactly once at the emitter (rustc's `DiagCtxt` model, which the renderer already imitates visually).

---

### L-048 — No single desugaring point: every piece of sugar is lowered 2× (or kept undesugared and interpreted 4-5×)

**Impact:** High (the mechanism by which L-042/L-043 happened; each new feature costs 4-5 implementations) · **Confidence:** High · **Layer:** design (pipeline architecture)

- Template literals: lowered in HIR (`hir_builder_expr.cpp:1437`) **and** in AST codegen (`codegen/llvm/expr/core.cpp:804 gen_template_literal`). let-else: HIR (`hir_builder_stmt.cpp:171`) **and** AST codegen (`llvm_ir_gen_stmt_let.cpp`). for-in: never desugared — survives as `HirForExpr` (`hir_expr.hpp:729`) and `ThirForExpr` (`thir_expr.hpp:306`), lowered independently in the MIR builder (`mir/builder/control.cpp`) and AST codegen (`codegen/llvm/control/loop.cpp`), with the type checker (`checker/control.cpp`) and borrow checker (`borrow/checker_stmt.cpp`) each reimplementing its scoping. `?.`: checker + AST codegen only (L-042).
- Bonus dead weight in the same layer: HIR optimization passes `Inlining` and `ClosureOptimization` (`hir_pass.cpp:568,828` + `hir_pass_inline.cpp`, ~2,020 LOC) have **zero callers** anywhere outside `src/hir/` — only the MIR PassManager is wired (`query_core.cpp:709`).

**Recommendation:** declare HIR the one desugaring point; sugar must not exist below it. Any construct appearing in both `codegen/llvm/` and `mir/builder/` is a bug by definition. Delete the orphaned HIR passes.

---

## Verdict

The lexer+parser+diagnostic-renderer trio is a solid foundation — compact, fast (SIMD lexing), recoverable, with better error *rendering* than most young compilers. The frontend's failure is everything between the parser and the code generators: type checking on the raw AST with results parked in a pointer-keyed map, four independent re-implementations of type inference, a HIR/THIR layer that is neither the type-checking substrate (as in rustc) nor a complete desugaring point, and — because the shipping path bypasses it — language-level guarantees (`when` exhaustiveness, `?.` semantics) that exist on paper but not in the binary users run. This is not fixable by tuning; it requires picking one semantic spine (parse → typed IR with stable IDs → single lowering) and making both the checker's outputs and all sugar flow through it. The parser can stay; the middle-end contract must be rebuilt.

## Keep

- **Hand-written recursive descent + Pratt** (6,438 LOC across 10 focused files, largest function 219 lines) — the right architecture; rustc does the same.
- **SIMD lexer fast paths** for whitespace/comment/doc scanning (`lexer_core.cpp:227-257,336-358,455-474`) — lexing will never be the bottleneck.
- **Error recovery design:** four synchronizers (`parser_core.cpp:175-267`), `parse_module_partial` for tooling, error accumulation, cascading-error suppression — one syntax error does not cascade.
- **Keyword operators (`and/or/not`), `[T]` generics, `do(x)` closures, `to/through` ranges** — deliberate, documented (RFC-0002:12-16), and they genuinely delete ambiguity classes (`>>` splitting, `||`-closure collision) at zero runtime cost. Good trade-offs, not accidents.
- **Diagnostic renderer + fix-it machinery + JSON + 246-code `explain` registry** — keep the renderer, fix the plumbing (L-047).
- **Query-based pipeline granularity** (tokenize/parse as separate cached queries, transitive-source fingerprinting).

## Top 3 highest-leverage recommendations

1. **Build the semantic spine (fixes L-042, L-043, L-044 at once):** stable node IDs at parse time; checker results persisted in an ID-keyed table; HIR consumes them instead of re-inferring; all sugar desugared exactly once at HIR; exhaustiveness runs at check time as a real diagnostic. This is the frontend half of resolving F-001, and it deletes ~2.8K LOC of codegen-time inference.
2. **Centralize newline/doc-comment policy in the lexer** (terminator-insertion rule + doc side table), retiring 148 fragile `skip_newlines` sites — the whole T6 bug class disappears.
3. **Decide the OOP layer's fate** (L-046): gate it or fully lower it to the behavior model; every phase currently pays a ×2 dispatch-complexity tax for a feature the RFC never admitted.
