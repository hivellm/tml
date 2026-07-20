# 01 — Type System & Generics

**Findings:** L-001..L-011 · **Method:** code audit + compiled probes (`.sandbox/`, deleted after use) · **Builds on:** F-001/F-006 (`../architecture-performance-review/`).

## Summary

TML's type system makes the right **design-level** bets for a Rust-class language (monomorphized generics, static dispatch by default, fat-pointer `dyn` vtables, tagged-union enums with one real niche case) — but the **implementation substrate is string-typed and split-brained**. There are three independent type-inference engines with *conflicting* integer defaults (checker=I64, HIR=I32, codegen=I32, spec says I32), glued by pointer-keyed side tables; `unify()` is a 6-line assignment that can't detect conflicts; type identity round-trips through a lossy `__`-separated mangled-string encoding that monomorphization *parses back* into types; and layout is computed by a hand-rolled string parser that disagrees with LLVM's DataLayout. A live miscompilation from the split-brain inference was demonstrated (unannotated `-5` prints `4294967291`; unannotated `4294967296` prints `0` — type check passes on both) on the MIR path, while the AST-legacy path gets the same cases right — fresh, present-tense proof of F-003's divergence. The trait system's "false positives" (gotcha T6) root-cause to a name→name registry that records operator *promises* without method bodies, with no generic-argument or coherence tracking, and the principled `TraitSolver` that would fix it is wired only into the dead THIR path (F-001).

---

### L-001 — Three inference engines, three integer defaults; demonstrated silent miscompilation

**Impact:** Very High · **Confidence:** High · **Layer:** implementation (with a design gap underneath)

There is no single source of typed truth. Unsuffixed integer literals get a different default in each phase:
- Type checker: **I64** — `compiler/src/types/checker/expr.cpp:409` (`return make_i64();`)
- HIR lowering: **I32** — `compiler/src/hir/hir_builder_expr.cpp:158` ("default to I32 (like most languages)"), patched after-the-fact from a side table (`expr.cpp:222-231`, "W3" comments)
- AST-legacy codegen: **I32** — its *own* 2,832-LOC re-inference engine (`compiler/src/codegen/llvm/expr/infer.cpp:17` doc table "Int lit → I32", plus `infer_methods.cpp`, `infer_types.cpp`)
- Spec: **I32** — `docs/specs/04-TYPES.md:19`, and `:72-73` claims widening is "explicit, never implicit."

The glue is `TypeEnv::set_expr_type/get_expr_type`, a map keyed by **raw AST node pointer** (`compiler/src/types/env_core.cpp:102-115`) — any path that forgets to populate or consult it silently falls back to the wrong default. That is the real mechanism behind gotcha T6 ("annotate `:I64` in index code").

**Probes:** a file with `let a = 5` passed to both `I64` and `I32` parameters **type-checks clean**, and the MIR-path IR emits `call i64 @"want_i64"(i32 5)` against `define i64 @"want_i64"(i64 %v)` — a mismatched-function-type call (UB). Running it: `want_i64(-5)` printed **4294967291**; unannotated `let b = 4294967296` printed **0** (silent truncation, no T051 overflow error because the checker validated it as I64 while MIR emitted i32). The identical program forced onto the AST-legacy path (via a `Maybe[I64]` reference) prints correctly — the two live codegens disagree *today* on core semantics.

**What earlier models decided & why it conflicts:** each phase re-derived types locally instead of persisting checker results, then patched divergences with side tables (W3/W4). This conflicts with both performance (sext/trunc noise, wrong-width instantiations) and correctness.

**Recommendation:** one literal default (I32, per spec) implemented as a *real* inference variable unified across all uses of a binding, erroring on conflict (Rust's `{integer}` model); persist checker types into HIR structurally (not via pointer-keyed maps); delete `codegen/llvm/expr/infer*.cpp`.

---

### L-002 — `unify()` is not unification: no recursion, no conflict detection, last-write-wins

**Impact:** High · **Confidence:** High · **Layer:** implementation

`compiler/src/types/env_core.cpp:57-63` is the entire engine:

```cpp
void TypeEnv::unify(TypePtr a, TypePtr b) {
    if (a->is<TypeVar>()) { substitutions_[a->as<TypeVar>().id] = b; }
    else if (b->is<TypeVar>()) { substitutions_[b->as<TypeVar>().id] = a; }
}
```

No recursion into constructors (`List[?T]` vs `List[I32]` binds nothing), no occurs check (cycles detected only at `resolve`, returned "as-is", `:74-92`), an existing binding is silently **overwritten**, and two mismatched concrete types "unify" as a no-op — never an error. Everything real is done by ad-hoc bidirectional expected-type threading (`checker/expr.cpp:217-327`) and per-call-site substitution extraction that only binds *bare* `T` parameters (`checker/expr_call.cpp:299-319` — a param of type `List[T]` binds nothing on this path). The 22,559 LOC under `compiler/src/types/` (11,711 in `checker/`) compensate for the missing 200-line real unifier with special cases.

**Recommendation:** implement genuine structural unification with error reporting; most of the W3/W4 band-aids and a chunk of the checker's special-casing collapse once it exists.

---

### L-003 — Type identity is a lossy string: mangled names are parsed back into types during monomorphization

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

Mangling is `Base__Arg1__Arg2` (`compiler/src/codegen/llvm/core/llvm_types.cpp:1388-1404`), and the impl-method instantiation pipeline **reconstructs semantic types by tokenizing the mangled string** — `tokenize_mangled` / `parse_mangled_type_string` / `parse_tokens_with_pattern` in `compiler/src/codegen/llvm/core/generic_instantiate.cpp:104-374`. The flat re-parser at `:333-368` splits nested generics on `__` as *siblings*: `Pair__Maybe__I32__Str` reconstructs as `Pair[Maybe, I32, Str]`, not `Pair[Maybe[I32], Str]`; correctness depends on having an impl's parser pattern to arity-guide the parse (`:125-134`). The encoding is ambiguous by construction (nesting is not delimited), struct/enum mangles carry **no module path** (same-named types in two modules collide into one symbol/dedup key), and library *functions* got a second, incompatible scheme — Itanium-style `N4core4iter4takeE__I32_h<fnv1a-hash>` where a hash was bolted on precisely to dodge the ambiguity (`llvm_types.cpp:1434-1446`), plus a `___a<N>` arity-suffix hack (`:1412-1430`).

**What earlier models decided & why it conflicts:** use human-readable name strings as the canonical type identity in codegen. Every later fix (hashes, arity suffixes, pattern-guided re-parsing) is a workaround for that lossy channel.

**Recommendation:** interned structural type IDs as the only identity in codegen; mangling becomes one-way output. This is a precondition for a correct shared-stdlib object (F-006).

---

### L-004 — Monomorphization mechanism: 110 scattered trigger sites expanding to *textual* LLVM IR, validated only at LLVM parse time — the K001 treadmill's engine

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

Instantiation is demand-discovered during the codegen expression walk: `require_struct/enum_instantiation` is called from **110 sites across 25 files** (method dispatch, derives, binary ops, field access, casts), feeding worklists drained by `generate_pending_instantiations` (`generic_instantiate.cpp:559-673`, capped at `MAX_ITERATIONS=100`). Each instantiation re-runs codegen over the parser AST with a substitution map and **appends IR text to string buffers** (`type_defs_buffer_ <<` throughout; e.g. `decl/enum.cpp:253-267`); the backend then *parses the concatenated text* (`compiler/src/backend/llvm_backend.cpp:282` — "[K001] Failed to parse LLVM IR"). There is no per-instantiation type verification: a bad type-arg combination is discovered only when LLVM rejects the whole module, with diagnostics against generated text, and post-processing includes literal string surgery on IR lines (`core/generate_support.cpp:526-527` demotes `define internal/linkonce_odr/...` → `declare`; `core/generate.cpp:682-686` scans for `"define linkonce_odr "`). That is *why* F-006's shared-stdlib work was a "K001 treadmill": every new combination is a fresh, unchecked text-generation run. Dedup is by mangled-name maps per `LLVMIRGen` instance — no cross-build instantiation cache; each test EXE re-expands everything.

**Recommendation:** instantiate through a typed IR (THIR/MIR) using LLVM's C++ API so malformed instantiations are structurally impossible, and key an instantiation cache on structural type IDs (L-003). This is the same consolidation Phase B of the prior review's plan requires.

---

### L-005 — `type_implements` root cause: a name→name promise registry — no bodies, no generic args, no conditions, no coherence

**Impact:** High · **Confidence:** High · **Layer:** design + implementation

The registry is `map<string, vector<string>>` — `register_impl` just does `behavior_impls_[type_name].push_back(behavior_name)` (`compiler/src/types/env_lookups.cpp:522-524`), and `type_implements` is a linear name scan (`:553-624`). Three structural defects:

1. **Promises without bodies:** builtins registration (`compiler/src/types/builtins/types.cpp:186-229`) records `Str: Eq/Ord`, `Bool: Ord`, etc. so the *checker* can accept `==`/`<`, but those operators are special-cased inline in codegen (str_eq/strcmp) — there is no dispatchable `cmp` method. Codegen that trusts `type_implements` then emits calls to nonexistent bodies; the `safe_types` primitive whitelist (`compiler/src/codegen/llvm/core/dyn.cpp:553-563`) exists to suppress exactly this, and it *also* means where-clause default methods are only ever generated for primitives — a silent feature hole for user types.
2. **No generic arguments:** `impl[T: X] Behavior for List[T]` registers as bare `("List","Behavior")` with the `where T: X` condition dropped (`checker/core.cpp:1430-1433`), so conditional impls are unconditionally true — `List[NotX]: Behavior` passes.
3. **No coherence:** duplicate/overlapping impls are never diagnosed (no such diagnostic exists anywhere in `compiler/src/`); codegen vtable registration is last-write-wins (`dyn.cpp:76-78`).

**Recommendation:** registry entries must be impl *records* (impl id, self-type with args, conditions, method symbols) shared by checker and codegen, with an overlap check at registration. The checker answering "yes" must entail a resolvable symbol.

---

### L-006 — The real trait solver exists but only serves the dead path

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

`compiler/src/traits/solver.cpp` (570 LOC) implements proper candidate assembly — impl/builtin/where-clause/auto candidates (`:147-154`) — i.e. the beginnings of the machinery L-005 needs. Its only consumer is `ThirLower` (`compiler/src/thir/thir_lower.cpp:24`), which per F-001 no real program reaches. The live checker and the legacy codegen both use the flat string map instead. Same pattern as F-002: the principled component is parked on the bypassed branch.

**Recommendation:** wire `TraitSolver` into the live checker path (it can sit atop the current TypeEnv) before growing it further on THIR.

---

### L-007 — Layout engine is string-parsing arithmetic that disagrees with LLVM; mixed-width bool access is the T6 root cause

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

Type lowering maps *name strings* to LLVM *type strings* (`compiler/src/codegen/llvm/core/llvm_types.cpp:45-127`), including a hand-rolled comma splitter for tuple strings (`:88-119`). Sizes come from a duplicated, wrong DataLayout: `calc_type_size` in `decl/enum.cpp:143-226` parses LLVM type strings, counts **`i1` as 4 bytes** (`:150`), sums tuple members **without padding** (`:177`), splits nested tuple strings on `", "` (breaks on `{ { i32, i64 }, i8 }`), and uses a 3-way alignment guess `i1→1 : i32→4 : else→8` (`:198-200` — so an `i8` field is assumed align 8). Variant sizes are summed unpadded (`:229-245`).

The bool gotcha's actual mechanism: `Bool` is `i1` in SSA but structs promote it to `i8` (`decl/llvm_struct_decl.cpp:11-17`), and every access site must remember the trunc/zext (`expr/struct_field.cpp:1130-1142`). Enum payloads make it worse: a probe showed `Just(true)` constructed by `store i1 1` into the `i32` payload slot of `%struct.Maybe__Bool = type { i32, i32 }` — a 1-byte store leaving **3 undef bytes** in a slot other code loads at declared width. Any payload-word comparison/hash/copy reads uninitialized memory — the same nondeterministic-corruption class as F-018/phase44b. Users were told to declare bool fields as `I64` (8× memory per flag) instead of fixing the width discipline.

**Recommendation:** delete the string size calculator; compute layout once from structural types via `llvm::DataLayout` (in-process LLVM is already linked, ADR-001), and zero-initialize/normalize enum payload slots at construction.

---

### L-008 — Enum ABI: always-i32 tag + type-erased `[N x i64]` blob; niche optimization is a single hard-coded `Maybe`+ptr case; `@repr` is validated then ignored

**Impact:** High · **Confidence:** High · **Layer:** design + implementation

Layout is `{ i32 tag, i32 | i64 | [N x i64] }` (`decl/enum.cpp:250-269`); the payload is an opaque word array, so all field access goes through casts on the blob rather than per-variant struct types. The **only** niche optimization is the special case `decl.name == "Maybe"` with exactly 2 variants and a ptr payload → bare `ptr` (`enum.cpp:360-400`) — matching `Option<&T>`, but nothing generalizes it (no bool/char/NonZero niches, no nested-enum flattening). Probe measurements: `Maybe[Bool]` = 8 bytes (Rust `Option<bool>` = **1**), `Maybe[U16]` = `{i32,i32}` = 8 (Rust = 4), tags are i32 even for 2-variant enums. `@repr(U8)` is checked for validity (`checker/decl_struct.cpp:715-737`) and then **never reaches codegen** — a probe's `@repr(U8) enum Color` still emitted `%struct.Color = type { i32 }`; only `@flags` underlying types are honored (`enum.cpp:49-75`). The project's own rule file (`.claude/rules/prefer-auto-derive.md`) recommends `@repr(U8)` "to control discriminant size" — advice that currently does nothing. `@packed` and `@simd` are real (`llvm_struct_decl.cpp:98-117`).

**Recommendation:** honor `@repr` in enum tag emission (small, self-contained fix); adopt Rust's general niche framework only after L-007's layout consolidation; emit per-variant payload struct types instead of `[N x i64]`.

---

### L-009 — Generic bounds: silently skipped when inference fails; parameterized bounds checked by base name only

**Impact:** High · **Confidence:** High · **Layer:** implementation

At call sites, where-clause checks run only for type params that made it into `substitutions` — `if (it != substitutions.end())` (`checker/expr_call.cpp:324-325`); an unbound param (common, given L-002's weak binding) means **no bound check at all**. Parameterized bounds carry an explicit confession: "Full parameterized bound checking … For now, we just verify the base behavior is implemented" (`expr_call.cpp:361-363`) — `T: From[I32]` accepts any `From` impl. Combined with L-005 (conditions dropped, no generic args in the registry), bounds are effectively name-level hints; violations surface later as codegen/K001 errors or wrong behavior, which is exactly where the "fix the codegen bug" phase-cycles come from.

**Recommendation:** make bound-checking mandatory-or-error (unresolved param ⇒ inference error, not skip), and move it onto TraitSolver goals (L-006).

---

### L-010 — Eager prelude instantiation bloats every compilation unit

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

A trivial probe program (no imports) produced **2,781 lines of IR** whose preamble instantiates generic types the program never mentions — `%struct.Maybe__U16`, `%struct.Maybe__F64`, `%struct.Maybe__tuple_Str_Str`, `%struct.List__Str`, `%struct.List__U8`, etc. ("Generic types from imported modules" section) — plus whole-module textual inclusion of `core::str`/`core::fmt` helpers into the single output module. With no cross-build instantiation cache (L-004) this cost repeats per EXE, compounding F-005/F-006's per-test embedding.

**Recommendation:** instantiate only reachable combinations (reachability from `main`/tests), which falls out of a demand-driven typed-IR collector.

---

### L-011 — Fixed-size array indexing emits no bounds check (cross-area note)

**Impact:** Medium · **Confidence:** Medium · **Layer:** implementation

The layout probe's `arr[idx]` with a runtime `idx` emitted `sext` + `getelementptr inbounds` + `load` with **no length compare or panic branch** (observed in `tml_main` IR; the index was a runtime load, not const-folded). Rust emits a checked panic here. If `List`/slice paths do check (their methods return `Maybe`), fixed arrays are an unchecked hole in a language marketing memory safety. Flagged for the memory-model area (not exhaustively surveyed).

---

## Verdict

**The design can deliver Rust-class performance; the implementation substrate cannot, and patching it piecemeal will not converge.** Monomorphization + static dispatch + fat-pointer dyn is exactly Rust's recipe, and where TML's layout matches Rust (`Maybe[I64]` = 16B, `WithBool` struct = 16B, `Maybe[ref T]` = bare ptr) it is already at parity. What blocks parity is not any single bug but three structural choices that every finding above traces back to: (1) **no single typed truth** — types are re-derived three times and reconciled by pointer-keyed side tables; (2) **string-typed identity** — mangled names, type-name strings, and LLVM type strings are the actual data model of the back half of the compiler; (3) **validation deferred to LLVM's text parser** — the K001 treadmill. These are the type-system faces of F-001/F-006 and should be fixed *as part of* the dual-codegen consolidation, not separately: a single checked, structurally-typed IR consumed by one codegen eliminates L-001, L-003, L-004, and L-007 at once. Targeted fixes that pay off immediately without redesign: honor `@repr`, real unification, mandatory bound checks, impl records with coherence.

## Keep

- **Monomorphization over dictionary-passing/boxing** — the right performance model; the problem is the mechanism, not the strategy.
- **`dyn Behavior` as `{ data, vtable }` fat pointer with per-(type, behavior) vtable globals and stable slot ordering** (`core/dyn.cpp:5-30, 83-111`) — Rust-identical, sound.
- **Static dispatch by default via direct mangled calls; `dyn` is opt-in** — zero-cost by default.
- **The `Maybe[ptr]` null-niche special case** (`enum.cpp:360-400`) — correct instinct; generalize rather than remove.
- **Bool promoted to i8 in aggregates** (`llvm_struct_decl.cpp:13-17`) — matches Rust's in-memory bool; the bug is the *inconsistent* application, not the promotion.
- **Bidirectional expected-type propagation** in the checker (`checker/expr.cpp:217-327`) — good skeleton for literals/closures/generic returns; needs a real unifier underneath, not replacement.
- **`@flags` and `@packed`/`@simd`** — genuinely implemented, reach layout correctly.
- **TraitSolver's candidate architecture** (`traits/solver.cpp:147-154`) — the correct shape; just parked on the wrong branch.

## Top 3 highest-leverage recommendations

1. **One typed truth, one literal rule.** Persist checker output into HIR structurally; delete the codegen's 2,832-LOC shadow inference and the HIR re-defaulting; give literals real inference variables unified across uses with conflict errors (fixes the demonstrated miscompilations, kills gotcha T6 and the W3/W4 side-table pattern). Highest correctness-per-effort item and a prerequisite for trusting either codegen path.
2. **Structural type IDs end-to-end; mangling one-way.** Replace string identity in instantiation keys, `type_implements`, vtable keys, and layout with interned type IDs + `llvm::DataLayout`; make impl registration record-based with coherence checks. Defuses the K001 treadmill's root cause and unblocks the shared-stdlib object (F-006).
3. **Fold this into the dual-codegen consolidation (F-001 Phase B), not around it.** Route the one surviving codegen through typed IR + LLVM's builder API so instantiations are verified structurally; wire TraitSolver into the live path; enforce bounds at check time. Sequencing type-system fixes inside the AST-legacy text pipeline would be investment in the branch the consolidation should retire.
