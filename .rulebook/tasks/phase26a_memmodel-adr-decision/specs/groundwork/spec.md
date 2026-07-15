# ADR-009 Groundwork — Read-Only Codebase Evidence (pre-spike)

Collected 2026-07-15 by read-only research pass. Feeds items 1.2/1.3 of phase26a.
All claims cite file:line. Spikes (item 1.4) must confirm the two flagged dynamic
questions (F-013 MIR elaboration; AST-path fallback frequency).

## Q1 — How drop insertion works today

- `LLVMIRGen::register_for_drop` (`compiler/src/codegen/llvm/core/drop.cpp:128-323`)
  classifies: direct `Drop` impl (`env_.type_implements`, :135), field-level drops
  (recursive `struct_fields_` walk, :163-236), enum variant-aware (:241-262).
- Scope state: `drop_scopes_` vector-of-vectors, LIFO emission at scope exit
  (`emit_scope_drops` :1159-1182, `emit_all_drops` :1184-1204). **Purely lexical** —
  confirms F-001.
- Move/init tracking that EXISTS today, fragmented in 3 disconnected mechanisms:
  1. AST codegen `consumed_vars_` (`drop.cpp:97-116`, consulted :1170-1179, :543-686)
     — syntactic "name was consumed" set, 30+ live `mark_var_consumed` call sites
     (e.g. `llvm_ir_gen_expr.cpp:109`, `expr/call_user.cpp:1050`, `control/return.cpp:114`).
  2. MIR `BuildContext::DropInfo.is_moved` + `mark_moved()` + move-filtered getters
     (`compiler/include/mir/mir_builder.hpp:68,92,104-130`) — **DEAD CODE: `mark_moved`
     is never called anywhere**. Scaffolding for B1 already written, unwired.
  3. Borrow checker `OwnershipState`/`is_initialized` (see Q4) — computed then discarded.
- No `drop_flag` / `init_state` / per-local runtime flag exists anywhere in `compiler/src`.
- **MIR has no `DropInst`.** Drops lower eagerly to `CallInst("Type::drop")`
  (`compiler/src/mir/thir_mir_builder_expr.cpp:1088-1094`; struct-field recursion
  :1097-1135; arrays :1138-1155). Drop is never a symbolic node in the optimizer.

## Q2 — Two live codegen paths, two drop subsystems

- Selection in `compiler/src/cli/builder/build.cpp:409-576`: MIR path default
  (`ThirMirBuilder` :431 → `MirCodegen` :529); **AST fallback fires when
  `has_tml_imports_needing_codegen || has_local_generics || emit_ir_only` (:413, :550)
  — i.e. exactly for generics + stdlib imports (`Shared`, `Heap`, `HashMap`), where the
  bug class lives.** `parallel_build.cpp:665-676` mirrors this.
- Consequence: B1 (or B2) must land in BOTH paths, or the AST fallback must be
  retired first. This is a major scoping decision for the ADR.

## Q3 — Duplicate synthesis, drop-glue, ptr_read_clone detection

- `gen_derive_duplicate_struct` (`compiler/src/codegen/llvm/derive/duplicate.cpp:99-190`),
  `..._instantiation` (:200+), `..._enum` (:278+). Derived duplicate = **deep clone**
  (primitives bit-copied, non-primitives recurse via `field::duplicate()`, :148-183).
- Aggregate drop-glue: `emit_field_level_drops` (drop.cpp:397-541),
  `emit_partial_field_drops` (:543-686), enum drop switch (`ensure_enum_drop_function`
  :840-1157). **drop.cpp:460-471 hand-special-cases Heap/List/HashMap/Buffer/
  BinaryWriter/BinaryReader to SKIP field drops** with a comment admitting the
  F-002 use-after-free — a band-aid inside the drop engine itself.
- `ptr_read_clone` detection (= embryonic B2 machinery, in both paths):
  - MIR: `emit_intrinsic_ptr_read_clone` (`compiler/src/codegen/mir/instructions_call.cpp:1049-1099`)
    — resolves module via **hardcoded name map** `canonical_module_for_type`
    (:1008-1040: Shared, Heap, Box, Rc, Arc, Cell, RefCell, List, HashMap, HashSet,
    Buffer, Maybe, Outcome, Str). Bitwise fallback for tuples/arrays/refs/unmapped.
  - AST: `intrinsics.cpp:675-822` — decorator scan (`@derive(Duplicate)`/`@auto`) +
    manual-impl scan; bitwise fallback :819-822.
  - Limit: detection is name/decorator-driven, NOT a semantic "transitively owns a
    refcount" query. User type wrapping a `Shared` without decoration → silent
    aliasing bitwise read.

## Q4 — Borrow checker: right facts, wrong layer, discarded

- AST-level: `BorrowChecker::check_module(const parser::Module&)`
  (`compiler/include/borrow/checker.hpp:888`, walkers :974-1052).
- Already models the full B1 lattice: `OwnershipState {Owned, Moved, Borrowed,
  MutBorrowed, Dropped}` (:330-336), `PlaceState.is_initialized` (:610),
  `moved_projections` (:599-607), init-state dataflow with merge
  (`save/restore/merge/apply_init_state`, :799-816). Errors B001–B016.
- **Facts are discarded**: `provide_borrowcheck_module`
  (`compiler/src/query/query_core.cpp:465-502`) returns pass/fail + strings only;
  `provide_mir_build` (:575-593) depends only on typecheck — MIR never sees
  ownership facts. Polonius `FactTable` (`polonius_facts.cpp:21-54`) is opt-in
  (`CompilerOptions::polonius`, query_core.cpp:484), AST-level, terminal pass/fail.
- Confirms F-004; also means re-plumbing checker facts is necessary but NOT
  sufficient — aliasing arises below the safe layer inside `lowlevel { *this.ptr }`.

## Q5 — Refcount machinery + F-013 status

- **F-013 statically confirmed**: `increment_count` does
  `let inner: SharedInner[T] = *this.ptr` (`shared.tml:320`) to read one counter;
  `decrement_count` same (`shared.tml:333`). Full bitwise copy of `value: T`.
- MIR path registers any non-trivially-destructible `let` for drop
  (`thir_mir_builder.cpp:663-668`) and drop recurses into fields
  (thir_mir_builder_expr.cpp:1097-1135) → the `inner` local plausibly drop-decrements
  nested handles.
- **DYNAMICALLY CONFIRMED (2026-07-15, item 1.1, emit-ir on a
  Shared[Payload{id, nested: Shared[I64]}] probe):** the emitted
  `Shared[Payload]::increment_count` (a) `load %struct.SharedInner__Payload` —
  full bitwise copy to a stack alloca, (b) bumps the counter, (c) then calls
  `Shared__I64::drop` on the `nested` field INSIDE the stack copy; that drop →
  `decrement_count` → **writes the decrement to the REAL allocation's
  strong_count via this.ptr and `mem_free`s it at 0**. So on the emit-ir path,
  every `outer.duplicate()` leaks one real decrement of each nested handle.
- **OPEN QUESTION for the spikes (item 1.4):** the f013 corpus test
  (2000 duplicate/drop cycles + `payload.strong_count()==2` assert) passes
  100/100 at runtime — the refcount bleed does NOT manifest in the test
  binary. Most plausible explanation: `--emit-ir` forces the LEGACY AST path
  (build.cpp:413 `emit_ir_only`), while the test binary's codegen elaborates
  the inner-copy drop differently (or not at all). Determining WHICH path the
  test binary takes and WHERE the two diverge is decision-critical: it tells
  us which drop-elaboration subsystem is live in practice and which one B1
  must fix first.
- Same read-whole-inner shape: `Shared::take` (shared.tml:287-290),
  `Heap::into_inner` (heap.tml:152-157), plus the F-002 getters
  (`shared.tml:149`, `heap.tml:117`). Safe forms exist: `get_clone` (:177-182),
  `get_ref` (:207-213).

## Q6 — Blast radius

- 52 MIR pass files (`compiler/src/mir/passes/`). Builder sizes:
  thir_mir_builder.cpp 1088 lines, _expr 1223, _control 906; drop.cpp 1315;
  instructions_call.cpp 1410.
- MIR validator (`mir_validate.hpp:30-55`) is lightweight (null types, terminators,
  empty blocks) — won't block new instructions.
- Drop-aware passes that STRING-MATCH `::drop`/`_drop` and break under any new
  representation: `remove_unneeded_drops.cpp:22-50`, `destructor_hoist.cpp:199,247`,
  `batch_destruction.cpp:13-42`.
- Conditional primitives available for drop flags: `CondBranchTerm`/`SwitchTerm`
  (mir.hpp:748,755), `SelectInst` (:490). No conditional-drop instruction; flag =
  bool alloca + CondBranchTerm. Precedent: AST enum-drop switch (drop.cpp:960-977).

## Q7 — Type info at copy sites (B2 feasibility)

- Structural type known at every copy site (`CallInst.arg_types`,
  `StoreInst.value_type`, `ExtractValueInst.result_type` carry `MirTypePtr`).
- Existing queries: `type_needs_drop` (env.hpp:598), `is_trivially_destructible`
  (:608), `type_implements(type,"Drop")` (:590) — all conflate Str/Drop/droppable-field;
  **no "is/contains refcounted handle" query exists**. B2's retain predicate must be
  built from scratch, in both paths.

## Risk summary

**B1 top risks:** (1) drop not first-class in MIR + string-matching passes;
(2) two uneven drop subsystems (AST fallback covers exactly the stdlib/generics);
(3) borrow-checker facts are AST-level, discarded, and blind below `lowlevel`.

**B2 top risks:** (1) refcount-ownership query doesn't exist (hardcoded name map is
the current state of the art); (2) retain/release primitives are TML source that is
itself unsound (F-013) — must be fixed/intrinsified first; (3) stdlib-wide semantic
migration (every owning type + drop.cpp:460-471 special cases assume value/deep-copy).

**Half-built infra:** B1 — dead `mark_moved` scaffolding (mir_builder.hpp:104-130),
working syntactic `consumed_vars_` prototype, ready init-merge algorithm in checker.
B2 — `ptr_read_clone` is a working auto-clone-on-copy-out in both paths (retain
machine minus semantics); `get_clone`/`get_ref` API surface exists.
