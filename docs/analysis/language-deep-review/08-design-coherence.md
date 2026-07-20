# 08 — Language-Design Coherence, Spec Drift & the Feature Gap

**Findings:** L-140..L-148 · **Method:** spec inventory + sample-verification of doc examples via `tml check` probes · **Builds on:** F-001..F-020 context.

## Summary

TML's authoritative grammar (`docs/specs/03-GRAMMAR.md`) and its "workhorse" constructs (`for`-in, `let`-else, `?.`, `when` guards, `lowlevel`, `Slice[T]`, the `Iterator` trait design, async I/O, `core::simd`) are sound and internally consistent — sample-verified directly against the compiler. The drift is concentrated in exactly the layer that matters most for an "LLM-first, no ambiguity" language: the **onboarding/marketing docs and a handful of decorator-level features say things the compiler does not do**, sometimes the *opposite* of what it does. Concretely: the flagship "unified `loop`" claim and the `module`-keyword Quick Start example in `docs/specs/INDEX.md`/`01-OVERVIEW.md` do not parse; an entire user-metaprogramming subsystem (`decorator`/`quote`/`${}`) in `25-DECORATORS.md` is lexer-reserved-keyword-only vaporware that contradicts the same spec family's "No Macros" principle; `@inline(never)` is parsed and then silently treated as `alwaysinline`; and there is no compile-time-evaluation engine at all despite spec language implying one. Underneath these is a real feature-gap pattern for the "Rust-class performance" goal: closures are unconditionally heap-allocated fat pointers, and the one published Rust-parity benchmark tests a hand-tuned raw loop, not the closure/generic iterator-chain style the docs hold up as TML's "zero-cost abstraction" idiom. Separately, `.claude/rules/` contains seven idiom-preference rules that are fully-specified `clippy`-style lints sitting outside `tml lint`'s actual (purely style/complexity) rule catalog — a clean, low-effort lint-gap.

---

### L-140 — Flagship "unified loop" claim and `module` keyword are false; the highest-traffic quick-start examples don't parse

**Impact:** High · **Confidence:** High · **Layer:** design claim (marketing/spec text) vs implementation — the implementation's actual 3-keyword design is defensible; the claim about it is not.

- README.md:304 table row asserts TML replaced Rust's three loop keywords with one: "`loop` (unified) | One keyword".
- `docs/specs/INDEX.md` Quick Start (lines 216-227) teaches exactly that: `loop item in items { }`, `loop i in 0 to 10 { }`, `loop while condition { }`, plus `module hello` (line 124) as the very first "Hello World" line.
- `docs/specs/01-OVERVIEW.md` §7.2 "One Way to Do It" (lines 246-251), in the *same spec family*, shows the opposite: separate `for item in items`, `for i in 0 to 10`, `while condition`, `loop (true)`.
- `docs/specs/03-GRAMMAR.md` §5.5.1-5.5.3 (lines 641-700) is the authoritative EBNF and defines three distinct productions: `LoopExpr = 'loop' '(' Expr ')' Block`, `WhileExpr = 'while' Expr Block`, `ForExpr = 'for' Pattern 'in' Expr Block`. Module keyword is `'mod' ModulePath` (line 21); `lexer_core.cpp:50` registers only `"mod"`, never `"module"`.
- Verified empirically via `tml check`: `loop i in 0 to n { }` → `P001: Expected '(' after 'loop'`; `for i in 0 to n { }` → passes. `module hello` → `P001: Expected declaration`; `mod hello` → passes.
- Same bug repeats in `docs/specs/23-INTRINSICS.md` lines 16, 561-562, 614-615 (`module intrinsics`, `module intrinsics.x86` — also wrong path separator, should be `::` not `.`).

**Why it conflicts:** TML's entire value proposition is deterministic, unambiguous, LLM-legible syntax. The spec's own front-door examples fail the one test that matters — `tml check` — which is the exact failure mode ("ambiguous/inconsistent syntax causes generation errors") the language claims to eliminate.

**Recommendation:** either implement `loop x in y` as a real alias (cheap: `loop` already special-cases `(true)`) or delete the "unified" claim everywhere it appears. Add a CI gate that runs every fenced ` ```tml ` block in `docs/` through `tml check`.

---

### L-141 — User-defined `decorator`/`quote`/`${}` metaprogramming system is 100% unimplemented, and contradicts the spec's own "No Macros" principle

**Impact:** High · **Confidence:** High · **Layer:** design (spec describes a feature that was never built, structurally conflicting with a sibling design principle)

- `docs/specs/25-DECORATORS.md` §2, §6, §7, §10-13 document a full compile-time macro system: `decorator name(params) { func apply(target: DecoratorTarget) -> DecoratorResult { ... } }`, `quote { ... }` templates, `${expr}`/`$name` splicing, a `compiler.*` API.
- `docs/specs/01-OVERVIEW.md` §6 "No Macros": *"Macros break deterministic parsing. TML uses directives instead... Directives... are processed by the compiler, not arbitrary code transformations."* — directly contradicted by the above.
- Implementation: `KwDecorator`/`KwQuote` exist only as reserved-keyword table entries (`compiler/include/lexer/token.hpp:93,166`; `compiler/src/lexer/lexer_core.cpp:53,111`) — zero references anywhere else in the compiler (no parser production, no AST node). Verified: `decorator log_calls(level: I32) { func apply(...) {...} }` → three `P001: Expected declaration` errors.
- Only the built-in attribute-style decorators (§8 table: `@test`, `@inline`, `@derive`, etc.) are real, parsed generically as `@name(args)` (`compiler/src/parser/parser_decl.cpp:77-114`) and hardcoded per-name in the checker/codegen — nothing user-extensible.

**Why it conflicts:** not a perf conflict directly, but the worst kind of coherence conflict: the spec's most powerful "novel" feature is fiction, while its sibling doc brags about not having that exact category of feature. An LLM (the stated audience) generating from this spec will confidently produce code that cannot parse.

**Recommendation:** split §2/§6/§7/§10-13 out of `25-DECORATORS.md` behind an explicit `unimplemented` banner, or delete them. Do not build the quote/splice system to match the spec — it is fundamentally in tension with the LL(1)/no-macro goal and should be formally dropped.

---

### L-142 — Decorator argument syntax is internally inconsistent; only one of two documented forms parses

**Impact:** Medium · **Confidence:** High · **Layer:** implementation (parser accepts one form; spec documents two as interchangeable)

- `25-DECORATORS.md` §8/§11 use colon-style named args: `@test(name: "...")`, `@deprecated(since: "v1.2", use: "...")`.
- `25-DECORATORS.md` §16.1 uses equals-style: `@extern("c", name = "GetTickCount64")`.
- `parser_decl.cpp:92-109`: decorator args are parsed as a plain comma-separated `parse_expr()` list — no named-argument grammar.
- Verified: `@extern("c", name = "GetTickCount64")` parses (assignment is a valid expr); `@deprecated(since: "v1.2", use: "Instant::now()")` → `P001: Expected ',' between decorator arguments, found ':'`.

**Why it conflicts:** roughly half the spec's own usage examples for *real* built-in decorators don't compile as written — the exact ambiguity TML claims to have designed away.

**Recommendation:** standardize on equals-style (already load-bearing for FFI) and mechanically fix every colon-style example in `25-DECORATORS.md`.

---

### L-143 — `@inline(never)` is parsed, then its argument is silently discarded, producing the opposite of the documented effect

**Impact:** High (perf-tuning correctness) · **Confidence:** High · **Layer:** implementation

- `25-DECORATORS.md` §8: `@inline(always)` "Force inline", `@inline(never)` "Never inline."
- `compiler/src/codegen/llvm/decl/func.cpp:602-611` and `:1173-1179`: dispatch keys **only** on `decorator.name` — `"inline"`/`"always_inline"` → `has_inline_decorator = true`; `"noinline"`/`"never_inline"` → `has_noinline_decorator = true`. `decorator.args` (the `always`/`never` token) is never inspected.
- Consequence: `@inline(never)` has `decorator.name == "inline"`, so it sets `has_inline_decorator = true`, and `func.cpp:637-641` emits LLVM `alwaysinline` — the exact opposite of what's documented and intended.

**Why it conflicts directly with the perf goal:** manual inline control is one of the few genuinely load-bearing Rust-parity knobs (I-cache pressure, hot-loop code size). A silently-inverted `@inline(never)` actively defeats a deliberate performance decision in exactly the subsystem most tied to codegen quality.

**Recommendation:** branch on `decorator.args` for the `inline` name, or drop the arg-based spelling entirely in favor of the already-correct distinct names (`@inline`, `@noinline`) everywhere, spec included.

---

### L-144 — No comptime/const-evaluation engine exists; `const` is sugar for "checked immutable global," not compile-time computation

**Impact:** Medium-High · **Confidence:** High · **Layer:** design (feature gap, currently undocumented as such)

- Zero matches for `comptime`, `const_eval`, `ConstEval`, `CompileTimeEval` anywhere in `compiler/src/`.
- `compiler/src/types/checker/stmt.cpp:192-210` (`ConstDecl` handling): `value_type = check_expr(*d.value, const_type)` — the ordinary runtime-expression checker, with no requirement that the initializer be compile-time-foldable.
- `23-INTRINSICS.md` §10.1 documents `static_assert`/`static_assert_eq`/`static_assert_size` — only meaningful with a const-eval pass to execute them, and none exists.

**Why it conflicts:** Rust-class performance idiomatically leans on `const fn`/const-generics for compile-time-sized arrays, lookup tables, and zero-runtime-cost configuration. TML has no counterpart and doesn't say so anywhere — the gap is invisible until you go looking.

**Recommendation:** one sentence in `04-TYPES.md` scoping this out explicitly (or an ADR committing to build it) — before const-generic-shaped APIs accumulate that would need retrofitting on top of the already-fragile generics/monomorphization path.

---

### L-145 — Closures are unconditionally heap-allocated fat pointers; the "zero-cost iterator chain" claim is unproven for TML's own idiomatic style

**Impact:** High · **Confidence:** Medium-High · **Layer:** design + implementation (sound trait design undercut by closure representation and the dual-codegen split)

- `compiler/src/codegen/llvm/expr/closure.cpp:14-30`: *"Closures are represented as fat pointers: { func_ptr, env_ptr }... **Non-capturing closures**: function has NO %env parameter... env_ptr in the fat pointer is null."* Every capturing closure's environment is `malloc`'d; every closure value (captured or not) is dispatched through an indirect call via `func_ptr`.
- `docs/specs/06-MEMORY.md` §10.3 "Zero-Cost Abstractions" (lines 489-503) shows exactly `items.filter(do(x) x > 0).map(do(x) x * 2).sum()` compiling to "simple loops."
- `lib/core/src/iter/adapters/map.tml`/`filter.tml`: the adapter design itself is properly lazy and Rust-shaped (`impl[I: Iterator, F, B] Iterator for Map[I, F]`, generic, monomorphizable) — architecturally correct.
- But per F-001, any generic code — which `Map[I,F]`/`Filter[I,P]` always are — routes to the legacy AST→LLVM path that the 30 MIR optimization passes never touch.
- The one published parity number (README "Performance" table: 4.32B vs 4.57B ops/s, 1.06×) traces to `.rulebook/archive/2026-04-15-phase0r_auto-vectorization-hints/tasks.md:9,14` — a hand-tuned pointer-stepping `for-in` over `ListIter`, **not** a closure/adapter chain. No equivalent measurement for `.filter().map().sum()` exists anywhere.

**Why it conflicts:** the design is right, but two implementation facts undercut the specific spec claim: closures can never be zero-sized/inlined the way Rust's are, and the code implementing the adapters bypasses the optimizer per F-001. The README's benchmark table invites generalizing a raw-loop result to the whole "zero-cost abstraction" story; the two code shapes take unequally-optimized paths.

**Recommendation:** produce an IR/benchmark comparison for the actual `.filter().map().sum()` idiom before keeping or retracting the §10.3 claim. Long-term: zero-capture closures should lower to thin function pointers or be inlined at monomorphization time, not carry a fat pointer.

---

### L-146 — `@simd` decorator (the spec's documented SIMD entry point) doesn't exist; real SIMD lives in an unspecced module

**Impact:** Medium · **Confidence:** High · **Layer:** documentation (implementation is actually ahead of spec here)

- `23-INTRINSICS.md` §5b documents `@simd` on a function as turning element-wise array arithmetic into `<N x T>` vector ops. Zero references to `"simd"` anywhere in `compiler/src/parser/` — not even reserved, unlike the decorator/quote case.
- Real SIMD ships as an explicit typed-vector library: `lib/core/src/simd/{f32x4,f64x2,i32x4,i64x2,u8x16,i32x8,f32x8,f64x4,i64x4,i8x32,i8x16,i16x16,u8x32}.tml` + `detect.tml` (runtime feature detection) + `neon.tml`/`sse42.tml` + `portable.tml` fallback — a serious, Rust-`std::simd`-shaped surface never mentioned in `23-INTRINSICS.md`.

**Why it (mostly) doesn't conflict:** the underlying capability ("does the language expose SIMD primitives?") is answered "yes, extensively" — genuinely good news for the perf goal. The conflict is pure discoverability: project rule T7 ("Use SIMD in tensor/numeric code") exists in `AGENTS.override.md` precisely because the doc an agent is mandated to consult first (T3) doesn't mention `core::simd` and instead points at a non-existent decorator.

**Recommendation:** rewrite `23-INTRINSICS.md` §5b to document `core::simd` as it actually exists; drop or explicitly flag the `@simd` decorator. This alone would let T7 retire as a standing rule.

---

### L-147 — Rules-as-symptoms: seven AI-behavioral idiom rules police exactly the gap where `tml lint` has zero semantic-idiom rules

**Impact:** Medium-High (leverage, not severity) · **Confidence:** High · **Layer:** implementation (tooling gap)

- `.claude/rules/{prefer-for-in-loops,use-let-else,optional-chaining,prefer-auto-derive,prefer-destructuring-let,prefer-struct-update,prefer-pattern-guards}.md` — seven standing prose rules re-injected every session, each a mechanical before/after code transformation, each with a direct Rust-`clippy` analogue (`manual_let_else`, `needless_range_loop`, `option_if_let_else`/`manual_map`, `field_reassign_with_default`, etc.).
- `tml lint`'s actual catalog (`compiler/src/cli/linter/semantic.cpp:35-52`): `S001-S003` (style), `S010-S013` (naming), `W001-W004` (unused-X), `C001-C003` (complexity) — 13 rules, 100% style/complexity, 0% semantic-idiom. None of the seven patterns are checked.
- Adjacent proof the compiler *can* do this: `compiler/src/types/checker/decl_struct.cpp:206-214` emits an ad hoc semantic warning (`W-DERIVE-ON-GENERIC`) outside this catalog entirely — the AST-walking capability already exists; it's simply never been pointed at the seven patterns the AI rules encode.

**Why it conflicts:** a language needing seven standing natural-language reminders re-taught every AI session (and, by the same logic, needed by every human contributor without CLAUDE.md loaded) to write idiomatic-not-just-correct code has a tooling gap, not a style preference. Every pattern is already fully specified — the design work is done and checked into git; it's just encoded as prose instead of as a lint pass.

**Recommendation:** port the seven rules to `tml lint` as `I001-I007` with `--fix`, mirroring the existing style/complexity infrastructure. Cheapest, highest-leverage fix in this review.

---

### L-148 — The documentation layer doesn't consistently flag aspirational vs. shipped features

**Impact:** Medium · **Confidence:** High · **Layer:** design (documentation practice)

- README.md:10 states *"the C++ compiler is **100% functional (beta)** — all language features... fully implemented and test-covered,"* two sentences before its own honest, prominent caveat (line 12) about the self-hosting pause and the memory-model/codegen-stability issues behind it.
- That caveat pattern is *not* applied to any of the gaps this review found: the decorator/quote system (L-141), the `@simd` decorator (L-146), the `loop`-unification claim (L-140), or the `@inline(never)` inversion (L-143) — none are flagged anywhere in `docs/`.
- Not confined to marketing copy: `docs/specs/INDEX.md` and `01-OVERVIEW.md` — the *spec*, not the README — contain the non-parsing `module`/`loop` quick-start examples, so even the sober directory doesn't reliably separate current-shipped syntax from aspirational syntax.

**Why it conflicts:** for a project whose stated audience is LLMs treating documentation as ground truth (no "ask a senior engineer" fallback), an unflagged aspirational claim is strictly worse than a flagged one. The project already knows how to write an honest caveat (the self-hosting paragraph proves it); the practice just isn't uniform.

**Recommendation:** adopt one `> STATUS: unimplemented/partial` convention project-wide, sweep it across every item this review found to be vapor or partial, and pair with the L-140 doc-test CI gate so "shipped" claims are mechanically enforced rather than asserted.

---

## Verdict

**Fix now, while cheap:**
1. Front-door docs (`module`/`loop` examples) + a doc-example CI gate — mechanical, zero design risk, highest trust payoff.
2. Decide the fate of the vaporware specs (decorator/quote, `@simd` decorator) — implement or formally cut. Every day they stand, more spec pages get built on top of fiction (`25-DECORATORS.md` already has 12 sections resting on it).
3. Port the seven `.claude/rules/` idiom rules into `tml lint` — the design work is done; only the enforcement layer is missing.
4. Fix `@inline(never)`'s argument dispatch — small, surgical, a live correctness bug in a performance-critical knob.
5. Explicitly scope out (or commit to) comptime before const-generic-shaped APIs accumulate that would need retrofitting on a generics system that F-001 already shows is fragile.

**Leave alone:** the `Iterator` trait design, `Slice[T]`, async I/O, the `core::simd` library itself (not its docs), `?.`/`let-else`/pattern-guards/`for-in`, and `03-GRAMMAR.md` itself — all sound, all sample-verified. The fixes needed are documentation/lint-tooling fixes layered on top of the deeper compiler work flagged in F-001..F-020.

**On naming (Maybe/Just/Nothing, `behavior`, `.duplicate()`, Heap/Shared/Sync):** two categories of "words over symbols" decision are currently lumped under one banner. `[T]` instead of `<T>` and `do(x) expr` instead of `|x| expr` remove *real grammar ambiguity* (comparison-vs-generic, bitwise-or-vs-closure) and earn their keep even at a familiarity cost. `Maybe`/`Just`/`Nothing`, `Outcome`, `behavior`, `.duplicate()`, `Heap`/`Shared`/`Sync` remove no ambiguity at all — they're synonym substitution for `Option`/`Some`/`None`/`Result`/`trait`/`.clone()`/`Box`/`Rc`/`Arc`. For an LLM-first language that's a cost without a compensating parser benefit: every model is overwhelmingly pretrained on Rust's vocabulary, and `AGENTS.override.md` T3 has to carry a permanent "Rust → TML quick reference" table as direct evidence of an ongoing translation tax rather than a one-time cost. Keep the ambiguity-driven renames; reconsider the cosmetic ones soon — the window is closing, not open, since 15,000+ library functions and all error messages already speak fluent "Maybe/Outcome."

## Keep

- `Iterator` behavior design — lazy, `next()`-based, generic, monomorphizable (`lib/core/src/iter/traits/iterator.tml`, `adapters/map.tml`) — architecturally Rust-correct even though closures undercut it today (L-145).
- `Slice[T] { data: ref T, len: I64 }` — genuine zero-copy fat-pointer view (`lib/core/src/slice/mod.tml:92-95`).
- Async I/O is real, not aspirational — epoll/WSAPoll/timer-wheel implementations in `lib/std/src/aio/poller.tml`, `lib/std/src/net/eventloop.tml`.
- `core::simd` — wide, serious, portable-with-fallback SIMD surface (needs only a documentation fix, L-146).
- `?.` optional chaining, `let-else`, `when` pattern guards, `for i in 0 to n`, `lowlevel { }` — sample-verified together in one probe with zero surprises; these compose coherently.
- `docs/specs/03-GRAMMAR.md` itself — accurate, matches the parser for everything sampled; the drift lives in the marketing-adjacent restatements (README, quick starts), not the grammar.
- `[T]` generics and `do(x)` closures — genuinely ambiguity-removing, worth their familiarity cost.

## Top 3 highest-leverage recommendations

1. **Doc-example CI gate:** extract every ` ```tml ` block from `docs/` and README.md, run each through `tml check`, fail CI on parse errors. Catches L-140 for free and prevents recurrence project-wide.
2. **Promote the 7 idiom rules in `.claude/rules/` into `tml lint` (`I001-I007`, with `--fix`).** The design is already written down as prose; porting it converts a per-AI-session behavioral patch into tooling that helps every contributor.
3. **Force a binary decision on each vapor-spec item** (decorator/quote L-141, `@simd` decorator L-146, `@inline(arg)` dispatch L-143): implement or formally cut — no more "spec says X, compiler silently does not-X or the opposite of X." Small individually, but each compounds the documentation-trust problem described in L-148.
