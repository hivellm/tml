# 04 — The Shipping Codegen (AST-Legacy LLVM Path)

**Findings:** L-060..L-068 · **Method:** code audit + IR probes compiled on both paths (`.sandbox/`, deleted after use) · **Builds on:** F-001..F-004 (`../architecture-performance-review/01-dual-codegen-split.md`). This file audits **the path real programs actually run** — not the split itself.

> File refs: short paths like `decl/func.cpp` are under `compiler/src/codegen/llvm/`; `cmd_test.cpp` is `compiler/src/cli/commands/cmd_test.cpp`; `llvm_backend.cpp` is `compiler/src/backend/llvm_backend.cpp`.

## Summary

The path real programs run — the AST-legacy LLVM generator — emits textbook `-O0` IR: **every parameter is spilled to a fresh alloca and reloaded on use, aggregates/enums are materialized in memory, and there is no mem2reg-class cleanup on the TML side.** The whole question of "≤2× Rust" collapses to one fact confirmed by probe: **both `tml build` and the test harness default to O0** (`dispatcher.cpp:359`, `cmd_test.cpp:313`), and at O0 the backend runs *zero* LLVM passes (`llvm_backend.cpp:383`). So the naive IR ships verbatim — nothing recovers it. The AST path itself is not the disaster the prior "known targets" suggested (Maybe[I32] is now a compact `{i32,i32}`, struct literals use `insertvalue`, runtime decls are on-demand, arithmetic is checked-with-panic) — those are genuinely fixed. The real, systemic gaps are **missing pointer-parameter attributes and TBAA metadata** (the two things that let LLVM `-O2` recover naive IR), a **shallow enum clone that reintroduces the F-016 double-free class**, and an **all-textual string-buffer codegen** that makes every one of these fixes a manual, fragile, hundreds-of-sites edit. The probes also produced the cleanest possible proof of the dual-path split: the *same* function `add_mul` is 4 instructions on the dead MIR path and ~28 instructions / 5 allocas / 7 basic blocks on the live AST path.

---

### L-060 — The IR that actually ships is unoptimized O0; no LLVM pass ever runs on default builds or tests

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

`tml build` defaults to O0 (`dispatcher.cpp:359`: `opt_level = manifest_opt ? manifest->build.optimization_level : 0`), and the test harness is O0 unless `--release` (`cmd_test.cpp:313`: `tc.optimization_level = opts.release ? 3 : 0` — tests never pass `--release`). The backend gates *all* LLVM optimization behind `if (options.optimization_level > 0)` (`llvm_backend.cpp:383`) with `LLVMCodeGenLevelNone` at O0. So for every default build and all ~12,000 tests, the alloca-heavy AST IR is handed to the target machine with **no SROA, no mem2reg, no inlining, no DCE**. `--emit-ir -O2` is also misleading: it dumps *frontend* IR before `LLVMRunPasses`, so the O2 dump of `add_mul` still had all 5 allocas — passes run later on the in-memory module (corroborates F-004).

**Why it conflicts with ≤2× Rust:** the ≤2× target is only meaningful for optimized output. The IR developers actually run and benchmark (O0) is not optimized at all, so felt performance and test-execution cost are governed by unoptimized IR. This is also why the test-speed pain persists.

**Recommendation:** decouple *frontend codegen decisions* (checked-math on/off) from *LLVM SSA cleanup*. Run a curated pass set (mem2reg/SROA/instcombine/EarlyCSE/DCE) even at "O0-semantics" so tests keep checked-math but ship clean IR; measure the compile-time delta before committing.

---

### L-061 — Measured dual-path divergence: the live AST path emits ~7× the IR of the dead MIR path for identical code, via universal parameter spilling

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

Same source, two generators (each forced via the routing gate):

`add_mul(a,b,c) = (a+b)*c - a`, **MIR path (dead code, never ships):**

```llvm
define i64 @"add_mul"(i64 %a, i64 %b, i64 %c) inlinehint {
entry0:
  %v3 = add i64 %a, %b
  %v4 = mul i64 %v3, %c
  %v5 = sub i64 %v4, %a
  ret i64 %v5
}          ; 4 instructions, 0 allocas, 1 block
```

**AST path (live, ships):**

```llvm
define internal i64 @tml_add_mul(i64 %a, i64 %b, i64 %c) #0 {
entry:
  %t0 = alloca i64
  store i64 %a, ptr %t0     ; every param spilled...
  %t1 = alloca i64
  store i64 %b, ptr %t1
  %t2 = alloca i64
  store i64 %c, ptr %t2
  %t8 = alloca i64          ; ...plus a temp alloca per sub-expression
  %t14 = alloca i64
  ...                       ; ~28 instructions, 5 allocas, 7 basic blocks
```

The struct probe shows the same: MIR `sum` takes `ptr %p` and GEPs directly; AST `sum` takes `%struct.Point %p` **by value**, then `%t6 = alloca %struct.Point; store %p` — a full struct copy to stack on entry before reading fields. The anti-pattern is universal: **every parameter, scalar or aggregate, is immediately spilled to a fresh alloca and reloaded.**

**Why it conflicts:** at O0 this is final IR (L-060). At O2 LLVM SROA promotes the non-escaping spills, so scalars recover — but the pattern is exactly what forces total dependence on LLVM and blocks any hope of good debug/O0 output.

**Recommendation:** emit mutable-only locals to allocas; keep immutable params/temps in SSA. The single change that most narrows the O0 gap without relying on LLVM.

---

### L-062 — Zero pointer-parameter attributes across the entire 75K-LOC codegen — the #1 blocker to LLVM -O2 recovery

**Impact:** Very High · **Confidence:** High · **Layer:** implementation

Grepping `noalias|dereferenceable|readonly|nocapture|writeonly|sret(` across `compiler/src/codegen/llvm/` returns **10 hits total, all in `core/runtime.cpp`** (fixed runtime decls like `malloc`/`strlen`) plus 1 in `derive/duplicate.cpp`. User and monomorphized functions get *no* attributes: `@tml_sum(ptr %p)`, `List__I64::get(ptr %this, i64)` — all bare pointers. Rust emits `ptr noalias nocapture readonly dereferenceable(16) align 8 %p` on essentially every reference parameter.

**Why it conflicts:** without `noalias`/`readonly`, LLVM `-O2` must assume every pointer param may alias and may be written through, so it cannot hoist or eliminate loads across the opaque runtime calls that pervade real code (`print`, `to_string`, `List::get`, drop glue). This is precisely why aggregate/collection code stays above 2× even at release, while pure scalar code recovers. It is a real, additive, non-architectural fix.

**Recommendation:** thread ABI/aliasing attributes through the function-signature emit helper (`decl/func.cpp`, `decl/impl.cpp`) and call sites: `noalias nocapture readonly dereferenceable(N) align A` for immutable `ref` params, `dereferenceable`/`align` for `sret`.

---

### L-063 — No TBAA or alias metadata on any load/store — the #2 -O2 alias-analysis blocker

**Impact:** High · **Confidence:** High · **Layer:** implementation

`!tbaa|!alias.scope|!noalias` across `compiler/src/codegen/llvm/` = **0 occurrences.** Rust attaches `!tbaa` to nearly every memory op so LLVM can prove a load of an `i64` field doesn't alias a store to a differently-typed field.

**Why it conflicts:** combined with L-062, LLVM's type-based alias analysis is entirely absent, so `-O2` falls back to weak, address-based reasoning and cannot eliminate redundant loads/stores through the pervasive runtime calls. Directly caps how close aggregate-heavy code can get to Rust.

**Recommendation:** emit a minimal TBAA tree (a root, one node per primitive, struct-path nodes for fields). Even a coarse tree unblocks a large class of redundant-load elimination.

---

### L-064 — Aggregates returned/passed by value with no sret; enum construction is alloca+store+load, not insertvalue (inconsistent with struct literals)

**Impact:** Medium-High · **Confidence:** High · **Layer:** implementation

The AST path emits `sret` essentially nowhere (it's MIR-only). `@tml_make` returns `%struct.Point` by value; `@tml_sum` takes it by value. More telling: struct *literals* correctly use `insertvalue` (`%t3 = insertvalue %struct.Point undef, i64 %t2, 0`), but **enum/Maybe construction materializes through memory** — `alloca %struct.Maybe__I64; GEP field 0; store tag; GEP field 1; store payload; load whole struct; ret` — with a separate alloca per return path (`lookup` emitted `%t5` and `%t14`). A vestigial `bitcast ptr %t7 to ptr` (no-op under opaque pointers) is emitted on every enum payload store.

**Why it conflicts:** returning large aggregates by value defeats the Win64 sret convention and diverges from Rust; enum-through-memory is more IR than an `insertvalue` chain and, at O0, ships uncleaned. Maybe/Outcome are ubiquitous, so this is a hot pattern.

**Recommendation:** use `sret` for returns larger than a register pair; build enums with `insertvalue` into `undef` like struct literals; drop the dead `bitcast ptr→ptr`.

---

### L-065 — Enum `duplicate` is a shallow bitwise copy — reopens the F-016 double-free/leak class for handle-bearing payloads

**Impact:** High · **Confidence:** High · **Layer:** implementation

The struct clone protocol is now coherent in *intent*: `field_owns_handle` (`derive/duplicate.cpp:282`) mirrors the drop-glue predicate so "bump-on-read and decrement-on-drop stay balanced" (comment at :279-281) — deep-cloning `Str` and refcounted handles. But the **enum** emitter (`gen_derive_duplicate_enum`, :453-484) is:

```cpp
type_defs_buffer_ << "  %val = load " << llvm_type << ", ptr %this\n";
type_defs_buffer_ << "  ret " << llvm_type << " %val\n";
// comment: "may need refinement for complex payloads"
```

A `Maybe[Shared[T]]` / `Outcome[T, Heap[E]]` cloned this way copies the handle **without bumping the refcount**, so a later drop on each copy decrements twice → double-free/UAF, exactly the F-016 class. Additionally there are **three ~90%-duplicated struct-duplicate emitters** (`gen_derive_duplicate_struct`, `gen_derive_duplicate_instantiation`, `gen_structural_duplicate`) that must be kept manually in sync with the drop-glue predicate.

**Why it conflicts:** a correctness gap, not a perf one — but it means the "coherent ownership protocol" is only half-built (structs yes, enums no), and the divergence-risk of three parallel emitters is the F-016 treadmill in miniature.

**Recommendation:** give enums a variant-aware deep clone (switch on discriminant, deep-clone each variant's payload via the same `field_owns_handle` path); collapse the three struct emitters into one parameterized function.

---

### L-066 — All codegen is textual string-buffer emission — the architectural root of every IR-quality gap and the biggest migration-readiness liability

**Impact:** High · **Confidence:** High · **Layer:** implementation

The generator builds IR as strings (`type_defs_buffer_ << "  " << src_ptr << " = getelementptr..."`), everywhere. Correctness depends on exact formatting: `duplicate.cpp:348-353` documents a real bug where two `define`s interleaved and produced "two `%ret` allocas in one define → invalid IR," worked around by staging into a local `ostringstream`. There is no `IRBuilder`, no type-checked value handles, no attribute API.

**Why it conflicts:** every optimization in L-062/L-063/L-064 must be threaded by hand through hundreds of `<<` sites, and each is one typo away from invalid IR — which is why they were never added uniformly. For the eventual TML migration this is the worst possible substrate: a string formatter has no structural equivalent to port; a proper IR-builder abstraction would.

**Recommendation:** introduce a thin typed emit layer (even a C++ `struct Value { type, reg }` + `emit_load/emit_store/emit_call` helpers that own attribute/metadata emission) so attributes and TBAA are attached in one place. Also the natural pre-migration refactor.

---

### L-067 — 44 phases of accretion are visible as 16+ parallel `method_*` emitters and a handful of 1,500-line god files

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

107 files / 75,551 LOC. Method dispatch alone is fragmented across `method.cpp` (1674), `method_impl.cpp` (1553), `method_static_dispatch.cpp` (1606), `method_generic.cpp`, `method_primitive.cpp`, `method_primitive_ext.cpp`, `method_prim_behavior.cpp`, `method_array.cpp`, `method_slice.cpp`, `method_collection.cpp`, `method_maybe.cpp` (1595), `method_outcome.cpp` (1718), `method_class.cpp`, `method_dyn.cpp`, `method_static.cpp`, `method_impl_module.cpp` — ~16 files, ~15K LOC, each a special-case emitter added in a different era (`method_maybe`/`method_outcome` are type-specific hand-written dispatchers for what should be generic monomorphization). The largest god files are all method/derive/drop emitters.

**Why it conflicts (indirectly):** type-specific emitters mean each new generic type risks its own dispatch path and its own IR-quality profile; a fix to `insertvalue` or attributes in one doesn't propagate. This is where accretion has hurt most — not raw LOC, but N parallel code paths for one concept.

**Recommendation:** not urgent for ≤2×, but the method-dispatch fan-out is the prime consolidation target and should precede any migration; unify `method_maybe`/`method_outcome` into the generic instantiation path.

---

### L-068 — There is no mem2reg-equivalent on the AST path; its "optimization passes" are frontend heuristics, and `alwaysinline` never fires at O0

**Impact:** High · **Confidence:** High · **Layer:** implementation

`core/optimization_passes.cpp` is the only "optimization" file in the AST path, and it does *frontend* work — arena allocation, small-object optimization, cache-layout field reordering, class devirtualization (:3-31) — not SSA cleanup. Functions are emitted `alwaysinline nounwind willreturn` (`decl/impl.cpp:158`, `decl/func.cpp:638`) and `@inline` library methods like `List::get`/`len` are so marked, but at O0 the always-inliner pass is gated off (L-060), so the loop probe emitted a real `call @...getE` **per element** instead of inlined indexing, plus two dead full-struct loads (`%t2`, `%t10` loaded, never used) and an alloca shadow of the loop phi.

**Why it conflicts:** the machinery that would clean this (mem2reg, DCE, inlining) exists only on the dead MIR branch (F-002). The AST path can *never* produce good unoptimized IR; it can only hand naive IR to LLVM `-O2`. At O0 — the default — the hot loop is a call-per-element, which is nowhere near 2× Rust.

**Recommendation:** either run LLVM's mem2reg+always-inline+DCE at O0-semantics (cheapest), or port a minimal mem2reg into the AST emitter. The former reuses LLVM; the latter duplicates what MIR already has.

---

## Verdict — Phase B option (ii) feasibility

**Split answer, and the split is the decision.**

- **For release IR (O2/O3): option (ii) is feasible and not a rewrite.** LLVM `-O2` already recovers the AST path's scalar code to Rust parity (SROA kills the param-spills). The remaining aggregate/collection gap closes with four *additive, localized* fixes — pointer-parameter attributes (L-062), TBAA (L-063), sret + insertvalue for enums (L-064), reduced param-spilling (L-061). None touch the architecture; all are threaded through the signature/emit helpers. Rough scope: a handful of focused phases, gated behind the string-buffer-emitter tax (L-066) which makes each fix tedious but mechanical. The path is **not structurally unsalvageable for release output.**

- **For O0 / debug / test IR (the actually-felt pain): option (ii) does not solve it.** The AST path has no mem2reg (L-068) and defaults to O0 everywhere (L-060). Bringing O0 output near Rust requires reimplementing mem2reg/DCE/inlining in the string-buffer emitter — i.e. rebuilding, in a worse substrate, exactly the optimizer that already exists on the dead MIR branch. That is throwaway work.

**Therefore:** option (ii) is a legitimate *interim* that buys release-grade IR cheaply, but it structurally cannot fix the debug/test-speed story, which is the review's headline pain. The *complete* answer remains option (i) MIR-unification — because unification is the only path that makes the already-built optimizer run on real code and gives good O0 *and* O2 output from one generator. If forced to choose: pursue option (ii)'s L-062/L-063 immediately (they help *release* today at low cost and are not wasted even after unification), while treating MIR-unification (option i) as the strategic endgame that L-060/L-068 make unavoidable.

## Keep — what's genuinely well-built here

- **Maybe layout is fixed:** `Maybe[I32]` = `{i32,i32}` = 8 bytes, `Maybe[I64]` = `{i32,i64}` = 16 bytes — compact, matching Rust's `Option` sizes. The old `{i32,[1 x i64]}` waste is gone.
- **Struct-literal construction uses `insertvalue` chains** (AST) and `insertvalue`+`sret` (MIR) — the old alloca+store+load constructor is gone for struct literals.
- **Runtime declarations are on-demand** (~12 `declare`s emitted, not 500+); residual: some unused generic-type and 8 `%dyn.*` vtable type decls pulled from imports.
- **Arithmetic is checked-with-panic at O0** (`llvm.sadd.with.overflow` + panic block) and `nsw`-wrapping at O2 — a correct, Rust-like debug/release split (`checked_math = opt_level==0`).
- **Loop codegen has good bones:** `phi` induction variable, `llvm.assume(ult)` bounds hints, `!llvm.loop` metadata, and `List::get` uses `unchecked_mul` deliberately to avoid an overflow branch that would block vectorization (`lib/std/src/collections/list.tml:160-172`).
- **The routing gate is conservative and well-documented** (`compiler/src/query/query_core.cpp:800-992`) — it fails safe to the working path, which is why correctness held while the split persisted.

## Top 3 highest-leverage recommendations

1. **Emit pointer-parameter attributes + a minimal TBAA tree (L-062, L-063).** Single highest bang-for-buck for release-IR parity; unblocks LLVM `-O2` on the aggregate/collection code that currently stays above 2×. Localized to the signature/emit helpers, and *not wasted* even if MIR-unification later lands.
2. **Run LLVM's mem2reg + SROA + always-inline + DCE at O0-semantics (L-060, L-068).** Decouple checked-math (a frontend decision) from SSA cleanup (both currently gated on `opt_level`). Converts the naive O0 IR the tests and default builds ship into near-optimized IR essentially for free, improving both perf *and* test realism — measure the compile-time delta, since it trades against test-speed.
3. **Fix the enum deep-clone and unify the three duplicate emitters (L-065).** Closes a latent double-free/leak for `Maybe`/`Outcome` wrapping handle types (the F-016 class) and removes three-way manual-sync risk — a correctness win that also shrinks the accretion surface ahead of migration.
