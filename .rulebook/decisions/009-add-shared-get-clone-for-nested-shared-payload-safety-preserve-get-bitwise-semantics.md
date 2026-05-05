# 9. Add Shared.get_clone for nested-Shared payload safety; preserve .get bitwise semantics

**Status**: proposed
**Date**: 2026-05-05
**Related Tasks**: phase24l_shared-get-aliasing-deep-fix, phase24k_essential-cleanup-segv, phase24g_heap-rc-or-borrow-language-fix, phase24m_essential-c-residual-segv, phase0z_cc-driver-essential-c-residual

## Context

phase24k diagnosed that `Shared.get(this) -> T` returns the inner T by bitwise copy. When T contains nested `Shared[U]` fields (e.g., recursive `CType` enums in compiler-tml/src/cc/), those nested Shareds are aliased into the returned T without bumping their refcounts. Dropping the returned T then decrements those refcounts toward zero, freeing storage that the source Shared still expects to own. This is the documented `Shared.get` aliasing class of bug. Phase24k attempted naive `.duplicate()` chains and a custom `expand_typedef_value` helper at the consumer site; both REGRESSED essential.c (5/5 -> 30/30 crashes). Phase24l needed a structural fix that closes the typedef-arm crash without regressing existing Shared users.

## Decision

Add a NEW method `pub func get_clone(this) -> T where T: Duplicate { return (*this.ptr).value.duplicate() }` to `lib/core/src/alloc/shared.tml`. Keep existing `pub func get(this) -> T` semantics (bitwise copy) UNCHANGED — preserves the entire existing Shared blast radius. Migrate ONLY the proven hot site (`compiler-tml/src/cc/types.tml::base_to_ctype` Typedef arm) to use `.get_clone()`. Document the aliasing hazard on `.get()` and the safe alternative inline. Defer broader call-site migration to phase24m once a structurally cleaner approach is identified (codegen-level deep-clone or HashMap.get specialization).

## Alternatives Considered

- (Pure option a) Modify `.get()` to require T: Duplicate and call value.duplicate() — REGRESSED lib/core test suite (cache_aligned_box, cache, cache_soavec_set, future_fuse fail with K001 codegen errors). Reverted in phase24l attempt 3.
- (Broad option b) Migrate all ~40 .get() sites in compiler-tml/src/cc/ to .get_clone() — REGRESSED minimal repro (29/30 -> 25/30) and didn't close essential.c. Each additional site introduces refcount-bump leaks the consumer never decrements. Reverted in phase24l attempt 2.
- (Option c) Compiler-codegen automatic deep-clone — wide blast radius, requires invasive C++ codegen changes; deferred to phase24m.
- (Option d) HashMap.get specialization for Shared values — narrows the fix to the actual aliasing site without changing Shared.get semantics; requires generic-trait-aware codegen; deferred to phase24m.

## Consequences

PROS: Closes the typedef-arm class of essential.c minimal repro deterministically. Zero blast radius — `.get()` semantics unchanged, all existing Shared users untouched. New method is opt-in for callers who need deep-clone semantics. Inline documentation steers future authors to `.get_clone()` when T contains nested Shareds. CONS: essential.c × 5 = 0/5 gate NOT closed — residual SIGSEGV survives (filed as phase24m). Per-call-site migration is brittle (phase24l attempt 2 demonstrated regression risk); broader migration needs a different mechanism. The hybrid (a)+(b) form ships with a minimal footprint (1 typedef arm + 1 new method) and trades full gate-closure for zero-regression delivery. Future work in phase24m must choose between language-level semantic change (option a + audit) or codegen-level automatic deep-clone (option c).
