# Tasks: MIR Optimization Passes — Rewrite in TML

**Status**: Planned (0/25)
**Depends on**: phase15c (MIR builder produces correct MIR)
**Blocks**: Phase 16 (codegen needs optimized MIR)
**Duration**: 6–8 weeks
**Risk**: Medium — each pass is self-contained and independently testable
**C++ reference**: 52 passes, 19,422 LOC → ~12,600 TML

---

## Phase 1: Pass Infrastructure (3 items)

- [ ] 1.1 Create `compiler-tml/src/mir/passes/mod.tml` — pass manager: ordered list of passes, run all sequentially
- [ ] 1.2 Define `MirPass` behavior: `func run(module: mut ref MirModule) -> MirModule`
- [ ] 1.3 Create `compiler-tml/src/mir/passes/analysis.tml` — shared analysis utilities (dominator tree, use-def chains, CFG traversal)

## Phase 2: Critical Passes — Tier 0 (6 items)

These passes are REQUIRED for correctness — tests fail without them.

- [ ] 2.1 `mem2reg.tml` — promote allocas to SSA registers (MOST CRITICAL pass)
- [ ] 2.2 `dead_function_elimination.tml` — remove unused functions
- [ ] 2.3 `dead_code_elimination.tml` — remove instructions with no uses
- [ ] 2.4 `unreachable_code_elimination.tml` — remove unreachable basic blocks
- [ ] 2.5 `block_merge.tml` — merge sequential blocks with single predecessor/successor
- [ ] 2.6 Test: run Tier 0 passes on full test suite → verify all tests still pass

## Phase 3: Important Passes — Tier 1 (6 items)

These passes significantly improve code quality.

- [ ] 3.1 `constant_folding.tml` — evaluate constant expressions at compile time
- [ ] 3.2 `constant_propagation.tml` — replace variables with known constant values
- [ ] 3.3 `copy_propagation.tml` — eliminate redundant copies
- [ ] 3.4 `simplify_cfg.tml` (795 LOC) — simplify control flow graph
- [ ] 3.5 `inst_simplify.tml` (416 LOC) — algebraic simplifications (x+0→x, x*1→x)
- [ ] 3.6 `sroa.tml` — scalar replacement of aggregates

## Phase 4: Optimization Passes — Tier 2 (6 items)

Performance optimizations — important but not correctness-critical.

- [ ] 4.1 `inlining.tml` (1,132 LOC) — function inlining with cost model
- [ ] 4.2 `escape_analysis.tml` (1,314 LOC) — detect heap allocations that can be stack-allocated
- [ ] 4.3 `devirtualization.tml` (875 LOC) — replace virtual calls with direct calls
- [ ] 4.4 `rvo.tml` (376 LOC) — return value optimization
- [ ] 4.5 `tail_call.tml` — convert tail-recursive calls to loops
- [ ] 4.6 `licm.tml` — loop-invariant code motion

## Phase 5: Remaining Passes — Tier 3 (2 items)

Port remaining 34 passes in batches.

- [ ] 5.1 Port 17 medium passes (200-500 LOC each): adce, batch_destruction, bounds_check_elimination, common_subexpression_elimination, const_hoist, constructor_fusion, dead_arg_elim, dead_method_elimination, destination_propagation, destructor_hoist, early_cse, gvn, jump_threading, match_simplify, merge_returns, narrowing, peephole
- [ ] 5.2 Port 17 remaining passes: alias_analysis, async_lowering, builder_opt, infinite_loop_check, ipo, load_store_opt, loop_opts, loop_rotate, loop_unroll, memory_leak_check, normalize_array_len, pgo, reassociate, remove_unneeded_drops, simplify_select, sinking, strength_reduction, vectorization

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Run all 52 passes on full test suite → MIR-diff optimized output against C++ pass output
- [ ] 6.2 IR-diff: compile test files with TML MIR pipeline → identical LLVM IR to C++ pipeline

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
