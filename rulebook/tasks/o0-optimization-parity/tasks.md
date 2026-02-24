# Tasks: O0 Optimization Parity with Rust

**Status**: In Progress (65%)

## Phase 1: Already Implemented (Always-On at O0)

- [x] 1.1 SretConversion — ABI correctness for struct returns >8 bytes (Windows x64)
- [x] 1.2 InstSimplify — Identity operations (x*1, x+0, x/1, x&-1, x|0, x^0, x<<0)
- [x] 1.3 StrengthReduction — Power-of-2 mul/div/mod to shift/and, shift-add patterns (x*3,5,7,9), x*-1 negate
- [x] 1.4 Checked arithmetic — Overflow checks at O0 via `@llvm.sadd.with.overflow` etc.

## Phase 2: Enable Existing TML Passes at O0 (Safe, Already Implemented)

These passes exist in TML but only run at O1+. Rust runs equivalent passes at O0.

- [x] 2.1 ConstantFolding — Evaluate constant expressions at compile time (Rust: `ConstProp`)
- [x] 2.2 ConstantPropagation — Replace uses of constants with their values (Rust: `ConstProp`)
- [x] 2.3 DeadCodeElimination — Remove instructions whose results are never used (Rust: `SimplifyLocals`)
- [x] 2.4 SimplifyCfg — Merge trivial blocks, remove empty blocks (Rust: `SimplifyCfg`)
- [x] 2.5 CopyPropagation — Replace copies with original values (Rust: `CopyProp`)
- [x] 2.6 MatchSimplify — Simplify trivial when/match arms (Rust: `MatchBranchSimplification`)
- [x] 2.7 UnreachableCodeElimination — Remove provably unreachable code (Rust: `UnreachableEnumBranching`)
- [x] 2.8 BlockMerge — Merge basic blocks with single predecessor/successor (Rust: part of `SimplifyCfg`)
- [x] 2.9 Verify O0 pipeline with full test suite after enabling Phase 2 passes
- [ ] 2.10 Benchmark debug-mode performance improvement

## Phase 3: Enable More Existing Passes at O0 (Lower Risk)

These exist in TML at O2+ and have Rust O0 equivalents but need careful validation.

- [x] 3.1 Mem2Reg — Promote single-def allocas to SSA registers (Rust: `ScalarReplacementOfAggregates`)
- [x] 3.2 SROA — Break up aggregate allocas into individual fields (Rust: `ScalarReplacementOfAggregates`)
- [x] 3.3 EarlyCSE — Eliminate redundant computations within basic blocks (no Rust equivalent at O0, but safe)
- [x] 3.4 Inlining (always-inline only) — Inline `@inline(always)` functions (Rust: `Inline` for `#[inline(always)]`)
- [x] 3.5 DeadFunctionElimination — Remove functions never called (Rust: always runs)
- [x] 3.6 MergeReturns — Merge multiple return points into single exit (safe cleanup)
- [x] 3.7 Verify O0 pipeline with full test suite after enabling Phase 3 passes
- [ ] 3.8 Benchmark compile time impact (ensure passes are fast enough for O0)

## Phase 4: Implement Missing Passes (Rust Has, TML Doesn't)

These are passes Rust runs at O0 that TML has no equivalent for yet.

- [x] 4.1 DestinationPropagation — Eliminate intermediate copies by propagating destination (Rust: `DestinationPropagation`)
- [ ] 4.2 DataflowConstProp — SSA-aware constant propagation using dataflow analysis (Rust: `DataflowConstProp`) — deferred, existing ConstProp+CopyProp covers 80%+
- [x] 4.3 RemoveUnneededDrops — Elide drop calls for types that don't implement Drop (Rust: `RemoveUnneededDrops`)
- [x] 4.4 UnreachablePropagation — Propagate unreachable status through branches after const-prop (Rust: `UnreachablePropagation`)
- [x] 4.5 NormalizeArrayLen — Normalize array length checks for bounds check elimination (Rust: `NormalizeArrayLen`)
- [x] 4.6 RemoveNoopLandingPads — N/A, TML has no exception handling/landing pads
- [x] 4.7 SimplifyComparisonIntegral — Simplify integer comparisons (Rust: `SimplifyComparisonIntegral`)
- [x] 4.8 DeadStoreElimination (basic) — Remove stores to locations never read (Rust: `DeadStoreElimination` at mir-opt-level=1)
- [x] 4.9 Verify new passes with full test suite
- [ ] 4.10 Benchmark combined impact

## Phase 5: Type Layout Optimizations (Always-On Like Rust)

Rust applies these at all optimization levels. They affect ABI/layout, not instructions.

- [x] 5.1 Niche enum layout for Maybe[T] with primitive types (tag + T instead of tag + max_variant)
- [ ] 5.2 Niche enum layout for Maybe[ref T] — nullable pointer, no tag byte
- [ ] 5.3 Niche enum layout for Maybe[Bool] — use 2 as Nothing discriminant, no extra byte
- [ ] 5.4 Struct field reordering — minimize padding by sorting fields by alignment (Rust: always-on)
- [ ] 5.5 Empty type optimization — ZST (zero-sized types) should not allocate stack space
- [ ] 5.6 Single-variant enum optimization — unwrap single-variant enums to their payload type

## Phase 6: Runtime Lowering (Always Required, Like Rust)

Rust has mandatory lowering passes that aren't optimizations but transform MIR for codegen.

- [x] 6.1 Async lowering — Transform async functions into state machines (compiler/src/mir/passes/async_lowering.cpp)
- [ ] 6.2 Generator lowering — Transform generators into state machines
- [ ] 6.3 Elaborate drops — Insert drop flags and conditional drops for complex control flow
- [ ] 6.4 Elaborate box derefs — Insert null checks / alignment checks for Heap[T] accesses
- [ ] 6.5 Const/static promotion — Promote constant expressions to static allocations
- [ ] 6.6 Add move validation — Validate that moved-from values are not used (debug mode)
- [ ] 6.7 Add alignment/null checks — Check alignment and null in debug mode (Rust: `CheckAlignment`)

## Rust O0 Pass Reference

Full list of Rust MIR passes that run at `-C opt-level=0` (mir-opt-level=1):

### Always-Required Lowering (not optimizations)
- `ElaborateDrops` — Drop flags, conditional drops
- `ElaborateBoxDerefs` — Box pointer checks
- `GeneratorDrop` — Generator cleanup
- `ConstProp` / `ConstDebugInfo` — Constant evaluation
- `PromoteTemps` — Promote to static
- `AddCallGuards` — Exception safety
- `AddMoveCheck` — Debug validation
- `CheckAlignment` — Alignment validation
- `AddRetag` — Miri retag (debug)
- `CleanupPostBorrowck` — Remove borrow checker artifacts

### Optimization Passes (mir-opt-level >= 1, DEFAULT)
- `SimplifyCfg` — Merge trivial blocks
- `SimplifyLocals` — Remove unused locals
- `CopyProp` — Replace copies
- `DestinationPropagation` — Eliminate intermediate copies
- `DeadStoreElimination` — Remove dead stores
- `MatchBranchSimplification` — Simplify trivial matches
- `UnreachableEnumBranching` — Remove impossible enum branches
- `UnreachablePropagation` — Propagate unreachable
- `DataflowConstProp` — SSA constant propagation
- `NormalizeArrayLen` — Array length normalization
- `RemoveUnneededDrops` — Elide trivial drops
- `RemoveNoopLandingPads` — Remove no-op landing pads
- `SimplifyComparisonIntegral` — Integer comparison simplification
- `Inline` (only `#[inline(always)]`) — Force-inline annotated functions
- `ScalarReplacementOfAggregates` — SROA
- `InstCombine` — Algebraic simplifications
- `ReferencePropagation` — Propagate references
- `GVN` (at mir-opt-level=1) — Global value numbering

### Type Layout (always-on, not passes)
- Niche optimization (Option<NonZeroU32> = u32, Option<&T> = nullable ptr)
- Field reordering by alignment
- Zero-sized type elimination
- Single-variant enum unwrapping
