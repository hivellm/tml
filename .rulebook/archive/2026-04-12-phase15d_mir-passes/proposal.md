# Proposal: MIR Optimization Passes — Rewrite in TML

## Why

The MIR pass pipeline is the optimization layer between the MIR builder and LLVM codegen. It
contains 52 passes totaling 19,422 LOC of C++ spread across compiler/src/mir/passes/. Some passes
(mem2reg, dead_function_elimination, block_merge) are correctness-critical — the compiler produces
wrong or invalid LLVM IR without them. The rest improve output quality and runtime performance.
Porting all 52 to TML completes the self-hosting of the middle-end and eliminates the largest
remaining C++ optimization subsystem.

## What Changes

All 52 C++ pass files in compiler/src/mir/passes/ are replaced by TML equivalents in
`compiler-tml/src/mir/passes/`. A shared `MirPass` behavior and pass manager in mod.tml
orchestrate the fixed execution order matching the C++ mir_pass.cpp sequence.

### Architecture

```
compiler-tml/src/mir/passes/
  mod.tml              — pass manager: List[MirPass], run() iterates in fixed order
  analysis.tml         — shared: dominator tree, use-def chains, CFG traversal, liveness
  mem2reg.tml          — Tier 0: alloca → SSA promotion (MOST CRITICAL)
  dead_function_elimination.tml   — Tier 0
  dead_code_elimination.tml       — Tier 0
  unreachable_code_elimination.tml — Tier 0
  block_merge.tml                 — Tier 0
  constant_folding.tml            — Tier 1
  constant_propagation.tml        — Tier 1
  copy_propagation.tml            — Tier 1
  simplify_cfg.tml                — Tier 1
  inst_simplify.tml               — Tier 1
  sroa.tml                        — Tier 1
  inlining.tml                    — Tier 2 (1,132 LOC C++)
  escape_analysis.tml             — Tier 2 (1,314 LOC C++)
  devirtualization.tml            — Tier 2 (875 LOC C++)
  rvo.tml / tail_call.tml / licm.tml — Tier 2
  [34 remaining passes]           — Tier 3, batched
```

### Key Design Decisions

- **`MirPass` behavior** — `func run(module: mut ref MirModule) -> MirModule` — each pass is an
  independent unit, takes a MirModule and returns a (possibly modified) MirModule. No shared mutable
  state between passes; each runs to completion before the next begins.
- **Fixed execution order** — the pass manager runs passes in the same order as C++ mir_pass.cpp.
  This is not configurable at runtime — order is load-bearing (mem2reg must run before inlining).
- **Shared analysis utilities** — dominator tree construction, use-def chain building, and CFG
  traversal are factored into analysis.tml and reused by multiple passes. This avoids each pass
  reimplementing the same graph algorithms.
- **Tiered porting strategy** — Tier 0 (correctness) first, Tier 1 (quality) second, Tier 2
  (performance) third, Tier 3 (remaining) last. Each tier is tested before the next begins.
- **MIR-diff per pass** — each ported pass is verified against its C++ counterpart by running both
  on identical input MIR and comparing output instruction-by-instruction.

### Pass Inventory by Tier

**Tier 0 — Correctness-critical (5 passes, ~1,200 LOC C++):**
mem2reg, dead_function_elimination, dead_code_elimination, unreachable_code_elimination, block_merge

**Tier 1 — Code quality (6 passes, ~2,500 LOC C++):**
constant_folding, constant_propagation, copy_propagation, simplify_cfg (795), inst_simplify (416), sroa

**Tier 2 — Performance (6 passes, ~4,500 LOC C++):**
inlining (1,132), escape_analysis (1,314), devirtualization (875), rvo (376), tail_call, licm

**Tier 3 — Remaining 35 passes (~11,200 LOC C++):**
adce, alias_analysis, async_lowering, batch_destruction, bounds_check_elimination, builder_opt,
common_subexpression_elimination, const_hoist, constructor_fusion, dead_arg_elim,
dead_method_elimination, destination_propagation, destructor_hoist, early_cse, gvn,
infinite_loop_check, ipo, jump_threading, load_store_opt, loop_opts, loop_rotate, loop_unroll,
match_simplify, memory_leak_check, merge_returns, narrowing, normalize_array_len, peephole, pgo,
reassociate, remove_unneeded_drops, simplify_select, sinking, strength_reduction, vectorization (1,350)

## Impact

- Affected code: compiler/src/mir/passes/ (replaced), compiler/src/mir/mir_pass.cpp (replaced)
- Affected codegen: Phase 16 codegen consumes the TML-produced optimized MirModule
- Breaking change: NO — MIR-diff and IR-diff testing guarantee identical output
- User benefit: self-hosting progress; all 52 passes inspectable and modifiable in TML

## Success Criteria

All 52 TML passes produce MIR output that is instruction-identical to the corresponding C++ pass
output (MIR-diff clean). The full compiler pipeline using TML passes produces LLVM IR that is
identical to the C++ pipeline output on the complete test suite (IR-diff clean).

## Dependencies

- **Requires**: phase15c (MirModule type and builder available in TML)
- **Blocks**: Phase 16 (LLVM codegen needs optimized MirModule from TML passes)
- **Risk**: Medium — each pass is isolated and independently testable. Volume (52 passes, 19K LOC)
  is the primary challenge, not individual pass complexity. The tiered strategy ensures correctness
  is established early and each subsequent tier is additive.
