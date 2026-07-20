# 02 — Memory Model, Ownership & Borrow Checker

**Findings:** L-020..L-030 · **Method:** code audit + runtime probes (`.sandbox/`, deleted after use) · **Builds on:** F-015..F-017 (`../architecture-performance-review/04-memory-model-foundation.md`), ADR-009.

## Summary

The memory model's *architecture* (borrow facts exported to codegen, drop flags, Copy/Drop exclusion) is genuinely Rust-shaped, but three load-bearing switches are off or broken: move-semantics diagnostics are **disabled by default** behind an env var, the MIR codegen path **still double-drops on `let b = a`** (proven at runtime), and the library's smart pointers **never run their payload's destructor** (proven at runtime) because the one primitive they need — `drop_in_place` — is codegen-broken. Around these holes, ownership questions that Rust answers statically (is this Str owned? does this return value own its fields? may this closure cross a thread?) are answered by runtime pointer classification, initializer-syntax heuristics, and documentation, respectively. The borrow checker itself is an AST walk with a linear statement counter — solid for straight-line aliasing, structurally blind to loop back-edges — and its most precise output (`initialized`, partial-move projections) is still computed-then-discarded. The cost model follows: every band-aid is a copy, a deep clone, an FFI call, or a runtime check on a path Rust compiles to nothing.

---

### L-020 — Move semantics are opt-in: `TML_STRICT_MOVES` defaults off, so use-after-move is legal in shipped TML

**Impact:** Very High · **Confidence:** High · **Layer:** design

Evidence:
- `compiler/include/borrow/checker.hpp:969-976` — `strict_moves_`: "When false (default) … suppresses every move-derived diagnostic"; B001/B002/B005 and assign-after-move only fire when the env var is set (`compiler/src/borrow/checker_core.cpp:79-91`).
- Runtime probe: `let b = a; a.push(2)` on a `List` — default mode passes borrow check, then dies in codegen with `ERROR [build] Unknown variable:  [const_gen_vals={} type_subs={}]`; with `TML_STRICT_MOVES=1` it correctly reports `use of moved value: 'a'`. A double-consume probe (`take(a); take(a)` with `R: Drop`) produced "The generated LLVM IR is invalid" instead of a diagnostic.
- ADR-009 implementation table (`docs/adr/ADR-009-memory-model-soundness.md:203`) — phase26f 1.6 measured strict-mode blast radius as **0 over 183 files** (v0.3.65), i.e. flipping the default was already proven safe and still wasn't done.

**What earlier models decided & why it conflicts:** staging strictness behind an env var was a prudent migration tactic, but leaving it off after the blast radius reached zero means the language's core ownership contract is *unenforced for users*. Every downstream mechanism (drop suppression, drop flags, clone-read balancing) exists to keep programs that violate move semantics from corrupting the heap — cost paid to compensate for a check that exists but is off. Worse, the failure mode of un-diagnosed moves is now cryptic internal codegen errors instead of B001.

**Recommendation:** flip strict moves to default-on (keep the env var as an escape hatch for one release). One-line change, measured zero blast radius, converts two probe-confirmed internal-error classes into real diagnostics.

---

### L-021 — The MIR path still double-drops on the simplest move; the entire phase26 fact pipeline is AST-only

**Impact:** Very High · **Confidence:** High (runtime-proven) · **Layer:** implementation

Evidence:
- Runtime probe: `type R` with `impl Drop`, `let a = R{..}; let b = a` — **no imports** (routes to MIR path) prints `drop R` **twice**; the same program plus `use std::collections::List` (routes to AST path) prints it **once**.
- `compiler/include/mir/mir_builder.hpp:68,92-96` — `is_moved` / `mark_moved` exist; `grep mark_moved compiler/src` → **zero call sites**. The ADR-009 "dormancy" (line 66-67: "`BuildContext.is_moved` scaffolding exists but is never called") is still true two eras later.
- `set_ownership_facts` is only wired into the AST `LLVMIRGen` (`compiler/src/codegen/llvm/core/drop.cpp:201-209`, `compiler/src/query/query_core.cpp:1103`, `compiler/src/cli/builder/build.cpp:575`); `MirCodegen` never receives facts.

**What earlier models decided & why it conflicts:** ADR-009's B1-on-AST pivot ("fix the path reality runs") was locally rational, but it left the *strategic* path — the one with the 30 optimization passes the perf goal depends on (F-001) — unsound at `let b = a`. Every future step toward MIR unification re-encounters the exact bug class that took 14+ phases, and the fact-join machinery built in phase26 (span keys into a string-emitting AST codegen) is not reusable there.

**Recommendation:** before any MIR-unification work, land a minimal `mark_moved` wiring in `thir_mir_builder` fed by the same borrow facts — or gate the MIR route off for any program containing a droppable local. A first-class `DropInst` in MIR (ADR-009 step 4) is where drop logic should ultimately live; the AST fact-join should be treated as scaffolding, not the destination.

---

### L-022 — Smart pointers never destroy their payloads: `Heap`/`Shared`/`Sync` drop = `mem_free` only, because `drop_in_place` is broken

**Impact:** Very High · **Confidence:** High (runtime-proven) · **Layer:** design + implementation

Evidence:
- `lib/core/src/alloc/heap.tml:264-271` — `impl[T] Drop for Heap[T]` is `mem_free(this.ptr)`; T's destructor is never invoked. `lib/core/src/alloc/shared.tml:334-353` — `decrement_count` decrements counters and `mem_free`s; the doc claims "the value is dropped" but no code drops it. Same in `sync.tml`.
- Runtime probes: `Heap::new(R)` and `Shared::new(R)` with `impl Drop for R { println("drop R payload") }` — **the message never prints** in either case.
- The needed primitive exists but is disabled: `lib/core/src/ops/drop.tml:150-157` (`drop_in_place[T]`), with both test files stubbed out: `lib/core/tests/ops/drop_in_place.test.tml:2` ("generates infinite recursion (stack overflow)") and `lib/core/tests/drop/drop_in_place.test.tml:1` ("ACCESS_VIOLATION … generates invalid code").
- The compiler papers over one corner only: enum drop-glue hardcodes `Heap__X` inner drops (`compiler/src/codegen/llvm/core/drop.cpp:1131-1208`) and *documents deliberate leaks* for the rest — `drop.cpp:1198-1204` emits `"; skipped drop for inner struct '…' (no explicit Drop impl; fields may leak)"`, and `drop.cpp:1239-1243` carries a literal `TODO` for non-Drop payload fields.
- Library workaround shape where it *was* fixed: `List::destroy` drops elements by bitwise-copying each one into an owning local so scope-exit glue fires (`lib/std/src/collections/list.tml:296-299`; same in `retain` at 354-357) — a full `memcpy` per element to run a destructor.

**What earlier models decided & why it conflicts:** when `drop_in_place` codegen broke, models routed around it (free-without-drop, copy-to-drop, compiler special cases) instead of fixing the intrinsic. The result: RAII does not compose through any pointer indirection — `Heap[File]` leaks the fd, `Shared[List]` leaks the buffer — and the workaround (copy-to-drop) is pure overhead Rust doesn't pay.

**Recommendation:** fixing the `drop_in_place` intrinsic is the single highest-leverage memory-model fix in the codebase: it closes the payload-leak class in all three smart pointers, deletes the copy-to-drop pattern, removes the enum-glue Heap special case, and un-documents two deliberate leaks. It is compiler work (the intrinsic self-recurses instead of expanding to drop glue) with a ready-made spec in `docs/specs/22-LOW-LEVEL.md:119`.

---

### L-023 — No `Send`/`Sync` enforcement: the working thread-spawn API takes an unbounded closure

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence:
- `lib/std/src/thread/mod.tml:328` — `pub func spawn_fn(f: func()) -> UnitJoinHandle` and `:363` `spawn_i64` — the only *working* spawns; no bounds of any kind.
- `lib/std/src/thread/mod.tml:760-766` — the bounded `Builder::spawn[T: Send]` is a stub: "blocked by codegen limitations … return Err(SpawnError::Unsupported)".
- `lib/core/src/traits/marker.tml:87-89,198-209` — `Send` is an empty marker with a handful of manual primitive impls; there is no auto-trait machinery, and closure types can't carry capture bounds anyway.
- `lib/core/src/alloc/shared.tml:15-17` — `Shared[T]` refcounting is non-atomic and only *documentation* says it must not cross threads.

**What earlier models decided & why it conflicts:** shipping `spawn_fn` unbounded was the pragmatic response to generic-closure codegen gaps. But it means a data race — `Shared` cloned on two threads, count corruption, double-free — is reachable in 100% "safe" TML. The performance goal is directly implicated: the safe fallback is "use `Sync` (atomic) everywhere", i.e. paying atomics because the type system can't prove single-threadedness — the inverse of Rust's model where `Rc` is safely usable *because* `!Send` is enforced.

**Recommendation:** short-term, make `spawn_fn`'s doc warning a compile-time lint on captures of known-non-atomic types (`Shared`, `Cell`, collections of them). Long-term, `Send`/`Sync` need to be compiler-known auto-behaviors checked at closure-capture time; without that, the `Shared`-vs-`Sync` performance distinction is unsound to offer. See also L-102/L-103 (`06-runtime-concurrency.md`): the atomics underneath are themselves fake today.

---

### L-024 — The borrow checker is a single-pass AST walk with a linear clock, not a CFG dataflow; loops and non-trivial patterns are structurally blind spots

**Impact:** High · **Confidence:** High · **Layer:** implementation

Evidence:
- Architecture: `compiler/src/borrow/checker_core.cpp:10-20` ("single forward pass over the AST"); locations are a monotonically increasing `current_stmt_` counter (`checker.hpp:98-114,966-967`); NLL is `last_use < loc` on that linear clock (`checker_nll.cpp:195-229`).
- Loops are checked **once**, no fixpoint, no back-edge: `checker_expr.cpp:795-818` (`check_loop`) / `:839-869` (`check_for`) — a borrow living across the back edge into the next iteration (classic iterator invalidation across iterations) is invisible, because "later use" is defined by statement index, not by the CFG.
- `when`-arm pattern bindings are mostly untracked: `checker_expr.cpp:739-749` binds only bare `IdentPattern` arms ("simplified handling") plus the one "confident shape" `Just(r)` for interior refs (`bind_interior_ref_payload`, `:680-704`); other payload bindings are never defined as places → their moves/borrows escape checking entirely.
- Interior-reference invalidation (B009, the phase26g headline) covers only "confident shapes": direct method-call scrutinee/initializer over a place receiver (`checker.hpp:1303-1317`); a `get_ref` result routed through a variable, field, or free function is unchecked — the file itself calls under-borrowing "the safe direction," which is safe for false positives but is exactly the UAF-on-realloc false-negative class F-013 was.
- The Polonius alternative (`compiler/src/borrow/polonius_*.cpp`, behind `CompilerOptions::polonius`) is a second parallel checker that, when enabled, **silently exports no ownership facts** — `compiler/src/query/query_core.cpp:540-551` and `compiler/src/cli/builder/build.cpp:163-181` only call `ownership_facts()` on the NLL branch — so opting into the "more precise" checker reintroduces the `let b = a` double-drop that facts fixed.

**What earlier models decided & why it conflicts:** building borrow checking as an AST visitor was the fastest route to B001-B030 diagnostics, and within straight-line code the aliasing rules (B007/B008/B009, two-phase borrows, reborrow chains) are real. But without a CFG the checker can never be *complete* against Rust NLL — loop-carried borrows and pattern-bound places are not incremental gaps, they need a different substrate (MIR-based liveness). Meanwhile facts that codegen soundness now depends on are produced by only one of two checkers.

**Recommendation:** accept the AST checker as the diagnostics layer, but plan the soundness-critical dataflow (init/move/liveness) on MIR where a CFG exists — that is also where L-021 wants it. Delete or fact-parity the Polonius branch; a flag that silently degrades memory safety is worse than no flag.

---

### L-025 — `Str` is a Copy primitive whose ownership is discovered at runtime, per free, by pointer classification

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence:
- `compiler/src/borrow/checker_core.cpp:222-224` — all primitives, including `Str`, are Copy; there is no owned-string/borrowed-string type split.
- Consequently every string deallocation must decide ownership at runtime: `compiler/runtime/memory/str_free.c:135-179` — `tml_str_free` does a PE-image-range binary search (Windows) or `malloc_usable_size` (Linux) plus a magic-header probe (`tml_str_cap`, header at `ptr[-24]`) on **every** free. The Linux path calls `malloc_usable_size` on arbitrary pointers, which is UB for non-malloc pointers by glibc's contract.
- Codegen keeps a parallel bookkeeping layer to balance frees: `pending_str_temps_` flushed per statement (`compiler/src/codegen/llvm/llvm_ir_gen_stmt.cpp:127-132`), with the ownership of a `Str` argument resolved by guess: `consume_str_temp_if_arg` — "the callee **may** take ownership … Remove it from pending" (`compiler/src/codegen/llvm/core/drop.cpp:1454-1464`) — i.e., leak-by-default when the callee doesn't.
- `register_heap_str_for_drop` (`drop.cpp:426-437`) + `is_heap_str_producer` heuristics decide at each `let` whether a Str is heap-owned.

**What earlier models decided & why it conflicts:** making `Str` a single Copy type avoided the String/&str complexity early on, but it moved the owned-vs-borrowed question from the type system to (a) a runtime classifier on the free path and (b) per-callsite codegen heuristics with an unspecified argument-ownership ABI. Rust frees a `String` with a statically known dealloc and frees `&str` never; TML pays a classification check on every string drop and still leaks or double-frees when the heuristics disagree.

**Recommendation:** redesign-scale (an owned `String` type, or a tag bit in the Str representation). Interim: define the argument-ownership convention explicitly (callee never owns unless the parameter type says so) so `consume_str_temp_if_arg` stops guessing, and replace the Linux `malloc_usable_size` probe with the same header-magic check used for capacity. See also L-085 (`05-stdlib-implementation.md`) for the length-query cost of the same representation.

---

### L-026 — Drop-glue discovery is string matching over type names and *source code text*

**Impact:** Medium-High · **Confidence:** High · **Layer:** implementation

Evidence:
- `compiler/src/codegen/llvm/core/drop.cpp:229-240` — to decide whether a generic type has a Drop impl, codegen **substring-searches the source code of every registered module** for the literal pattern `"Drop for " + base_type + "["`.
- Generic instantiation identity is `name.find("__")` parsing throughout the drop path (`drop.cpp:222-241,553-559,624-640,957-962`), and `parse_mangled_type_for_drop` (`drop.cpp:54-108`) re-derives semantic types *from mangled strings*.
- The recursive field-scan fallback for locally-defined structs is manually depth-limited to two levels (`drop.cpp:288-313`).
- Suppression bookkeeping is name-keyed where the facts side is span-keyed: `consumed_vars_` and `drop_flag_by_name_` key on bare variable names (`drop.cpp:110-131,180`), so shadowed bindings share one consumed/flag slot — the exact ambiguity the span key was introduced to fix (`drop.cpp:184-186`), still present on the syntactic half of the union.
- The span key itself has no file dimension: `compiler/include/codegen/llvm/llvm_ir_gen.hpp:632-638` packs file-relative byte offsets and the comment asserts "within the single file of a codegen unit" — but the AST path inlines imported library functions into the same unit, so a library binding whose offsets collide with a moved user binding gets its drop wrongly suppressed (last-writer-wins, `drop.cpp:204-208`).

**What earlier models decided & why it conflicts:** each string-match was the cheapest way to make one case work in a codegen that emits text. Cumulatively, drop correctness — the most safety-critical glue in the language — rests on name conventions, substring scans, and a two-key (name ∪ span) union with acknowledged blind spots (`drop.cpp:176-179`: the fact side "deliberately under-reports" method args). Both a soundness fragility and a compile-time cost (source scans per registration).

**Recommendation:** intern a `TypeId`-keyed `needs_drop`/`has_drop_impl` table in `TypeEnv` computed once per type, and key all move bookkeeping by `(file, span)` or by the borrow checker's `PlaceId`. Mechanical, high-value hardening.

---

### L-027 — Fact granularity: `initialized` and partial moves are still computed-then-discarded; only two bits survive to codegen

**Impact:** Medium-High · **Confidence:** High · **Layer:** implementation

Evidence:
- The exported fact is three booleans (`compiler/include/common.hpp:265-277`); codegen consumes exactly two — `moved_out` (`drop.cpp:187-188`) and `conditionally_moved` (`drop.cpp:366-376`). `initialized` appears only in a debug print (`drop.cpp:1288`). A droppable `let x: T` declared uninitialized and conditionally assigned is registered for drop unguarded — drop of garbage on the never-assigned path.
- Partial moves never cross the boundary: `common.hpp:261-264` ("`moved_projections` … deferred to phase26b Step 4") and `drop.cpp:1313-1316` — partial-move drop decisions still run on the syntactic `"var.field"` prefix scan of `consumed_vars_`, which the checker populates only for single-level `ident.field` projections (`checker_core.cpp:396-441`; deeper chains "conservatively skipped").
- The checker's full ownership/borrow state (`PlaceState.active_borrows`, NLL lifetimes, reborrow chains, `moved_projections` as a `set<vector<Projection>>`, `checker.hpp:567-621`) is dropped on the floor after diagnostics; the export is an end-of-function snapshot only (`checker_core.cpp:93-117`).

**What earlier models decided & why it conflicts (or not):** granularity-(i) was an explicit, recorded scoping decision (ship the monotonic bit first) — sound method. But Step 4 never happened, and the residual classes are exactly the ones this review asks about: partial-move double-drop shapes that neither key covers, and uninitialized-drop UB. F-015 is thus "mostly resolved" only for whole-variable, straight-line moves.

**Recommendation:** wire `initialized == false` at declaration into the existing drop-flag mechanism (arm the flag at first assignment instead of at declaration — the machinery already exists, `drop.cpp:362-376`), and export `moved_projections` as span+path facts to replace the name-prefix scan.

---

### L-028 — Return-value ownership is decided by the *syntax* of the initializer expression

**Impact:** Medium-High · **Confidence:** High · **Layer:** design (manifested in implementation)

Evidence:
- `compiler/src/codegen/llvm/llvm_ir_gen_stmt_let.cpp:1258-1266` — field drops are suppressed iff the initializer node is literally a `MethodCallExpr` or `CallExpr`: "Let bindings from method/function calls don't own their struct fields. The caller (collection) retains ownership of inner heap handles."
- The reason this heuristic must exist: container reads return `T` **by value** as a bitwise copy that aliases the container's heap handles (`lib/core/src/alloc/shared.tml:118-140` documents the pattern; the phase27a carve-out at `llvm_ir_gen_stmt_let.cpp:1279-1297` then re-adds drops for handle-bearing aggregates because those reads are now balanced clones).
- The heuristic doesn't see through wrapping: `let r = if c { list.get(0) } else { list.get(1) }` is not a `MethodCallExpr` at the top level, so it takes the full-registration path — whether that is correct depends entirely on whether the read at the leaf was a balanced clone (F-016's `gen_structural_duplicate` fallback at `compiler/src/codegen/llvm/builtins/intrinsics.cpp:853-857` is exactly the case where it isn't).

**What earlier models decided & why it conflicts:** TML has no place/value distinction for returns — no way to say "this T is a view" — so codegen infers ownership from expression shape. Two independent mechanisms (initializer-shape suppression, clone-on-read synthesis) must agree case-by-case; F-016 is one disagreement, and every new expression form that can carry a container read is a potential new one.

**Recommendation:** the durable fix is API-level: container reads that don't transfer ownership should return `ref T` (the `get_ref` pattern already exists and is zero-copy, `shared.tml:163-196`), with by-value `get` requiring `T: Duplicate`. Then delete the initializer-shape heuristic entirely — drops become unconditional and symmetric.

---

### L-029 — The surviving copy tax: clone-on-read, copy-to-drop, FFI refcounts, runtime free-classification, untyped collection headers

**Impact:** High (perf) · **Confidence:** High · **Layer:** design + implementation

Evidence (each item is a per-operation cost Rust doesn't pay):
- **Clone-on-read:** `ptr_read_clone` deep-clones any handle-bearing `T` on every container read-out / iterator `next` — 20 non-test call sites across `heap/shared/sync/list/hashmap/behaviors/class_collections`; the compiler synthesizes structural clone glue when no `Duplicate` exists (`compiler/src/codegen/llvm/builtins/intrinsics.cpp:824-857`). Iterating a `List[Shared[T]]` bumps and un-bumps a refcount per element per pass; Rust yields `&T` for free.
- **Copy-to-drop:** destructors invoked by `memcpy`-ing elements into locals (`lib/std/src/collections/list.tml:296-299,354-357`) — a full element copy per drop, forced by L-022.
- **Refcount ops are FFI calls:** `Sync` increments via `@extern` C functions (`lib/core/src/alloc/sync.tml:54-61` → `compiler/runtime/concurrency/sync.c:751-771`) — a call per clone/drop vs Rust's single inlined `lock xadd`; `Shared`'s "fast" path is two separate field reads + a `lowlevel` store (`shared.tml:324-330`).
- **Per-free runtime classification** for strings (L-025, `str_free.c:135-179`).
- **Collections are untyped blobs:** `List[T]` is `handle: *Unit` plus hand-computed header offsets (`list.tml:284-306`) — invisible to LLVM alias analysis and the direct cause of the F-017/phase44b corruption class (see L-080).
- **Escape-hatch scale with no net:** 2,376 `lowlevel` occurrences in 164 of 556 lib files; ~250 in the HTTP server alone (violating the project's own T7 rule); 50 `mem_alloc(<literal>)` sites; the phase44c size-lint is still fully unchecked (`.rulebook/tasks/phase44c_hand-rolled-alloc-size-lint/tasks.md:22-39`).

**What earlier models decided & why it conflicts:** each mechanism was the sound-*enough* fix available without touching the compiler (or while the compiler primitive was broken). Together they invert the zero-cost principle: safety is purchased per operation at runtime, precisely because ownership information is missing at the points where Rust has it statically.

**Recommendation:** the unlock order is L-022 (`drop_in_place`) → L-028 (`ref`-returning reads) → typed collection headers (`type ListHeader[T] { data, len, cap, stride }` — also makes phase44c's lint mostly unnecessary) → intrinsic atomics for refcounts (L-102).

---

### L-030 — Borrow-checker compile-time complexity: per-use scans over all places × all borrows

**Impact:** Low-Medium · **Confidence:** Medium-High · **Layer:** implementation

Evidence: `mark_ref_used` and `release_dead_borrows` iterate every place and every borrow on each call (`compiler/src/borrow/checker_nll.cpp:163-229`); `apply_nll` runs per statement, giving O(statements × places × borrows) per function, plus `resolve_named_alias`'s global-module-cache scan (memoized per name, `checker_core.cpp:304-340`). Not a soundness issue; it feeds the compile-speed gap as functions grow.

**Recommendation:** index borrows by `ref_place`; only revisit places touched since the last NLL sweep. Low priority relative to the above.

---

## Verdict

**Sound-able: yes, without a redesign of the surface language — but not on the current trajectory.** The pieces of a Rust-faithful model all exist: a place-based checker with move/init dataflow, exported facts, drop flags, Copy/Drop exclusion, `Send` markers, `drop_in_place` in the API. What's missing is not architecture but *closure*: the strict-mode default was never flipped (L-020), the intrinsic was never fixed (L-022), the MIR path was never wired (L-021), Step 4 of the fact export was never done (L-027). Each is a bounded engineering task, already specified in ADR-009 or a filed phase.

**Zero-cost-able: not with the current library contracts.** Clone-on-read, copy-to-drop, runtime Str classification and FFI refcounts are consequences of two contract-level decisions — by-value container reads and `Str`-as-Copy-primitive — that no amount of compiler fact-plumbing removes. The minimal redesign is contract-level, not language-level: reads return `ref T` (mechanism already exists and works), owned strings become distinguishable from views, and destructor invocation gets a real primitive. The deeper structural risk: all soundness plumbing is welded to the AST text-emitting codegen (span keys, name sets, source-string scans); when the project unifies onto MIR, this must be rebuilt — so ADR-009's sequencing ("MIR unification deferred, facts on AST") should be treated as scaffolding with a planned demolition date, and no *new* soundness mechanism should be added AST-side.

## Keep

- **The fact-export architecture itself** (`ownership_facts()` → `set_ownership_facts`, span-keyed, join proven 0-miss) — the correct direction for RAII-over-codegen; keep the pattern, migrate the substrate.
- **Runtime drop flags for conditionally-moved bindings** (`drop.cpp:362-376,133-146` + `merge_ownership_branches`) — exactly Rust's drop-flag design, correctly scoped.
- **Copy/Drop mutual exclusion and explicit-`Copy`-derive-only semantics** (`checker_core.cpp:245-257`) — Rust-faithful, prevents the bit-copy-then-drop class at the type level.
- **`Shared` vs `Sync` split** (Rc/Arc mirror) and the F-013 field-pointer counter fix with its SOUNDNESS comments (`shared.tml:317-330`) — right shapes; they need L-022 and L-023 finished, not replaced.
- **`get_ref`/`Deref` zero-copy read pattern** (`shared.tml:163-196`) — the template for fixing L-028/L-029.
- **Staged rollout with measured blast radius** (TML_STRICT_MOVES + 183-file sweep) and **adversarial canaries** (`condmove_*`, `tml_let_move_double_drop`) — genuinely good migration engineering; the only failure was not pulling the trigger.
- **The phase44c lint plan** (survey-first design) — right instinct; typed headers would make most of it moot, but the lint protects the residue.
- **`tml_str_free`'s .rdata check as O(log n) image-range search** rather than `HeapValidate` — given L-025 exists, this is the cheap version of it.

## Top 3 highest-leverage recommendations

1. **Fix the `drop_in_place` intrinsic** (self-recursion instead of glue expansion; spec ready at `docs/specs/22-LOW-LEVEL.md:119`). One fix closes the payload-leak class in `Heap`/`Shared`/`Sync`, deletes copy-to-drop from collections, removes the enum-glue Heap special case and its two documented leaks — the largest single soundness+perf return in the memory model.
2. **Flip `TML_STRICT_MOVES` to default-on** (measured blast radius 0 at v0.3.65) and, in the same stroke, either wire `mark_moved` on the MIR path or gate droppable-local programs onto the AST path — the runtime-proven `let b = a` double-drop on the no-import path will otherwise resurface as "flaky" for months.
3. **Change container read contracts to `ref T`** (by-value `get` requires `T: Duplicate`), then delete the initializer-syntax drop-suppression heuristic in `llvm_ir_gen_stmt_let.cpp:1258-1266`. Converts the remaining F-016-class asymmetries from "two heuristics must agree" into "nothing to get wrong," and removes the per-read clone tax that separates TML's hot loops from Rust's.
