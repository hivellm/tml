# Proposal: Legacy LLVM Codegen — Port Remaining to TML

## Why

After phases 16a–16c complete the MIR codegen path (types, instructions, calls), the legacy LLVM
codegen in `compiler/src/codegen/llvm/` remains as the final large C++ subsystem in the compiler.
It is approximately 41K LOC spread across 60+ files covering builtins, destructor emission, derived
impls, let statement codegen, runtime declarations, and the Cranelift backend. Not all of this code
needs to be ported — some files handle patterns that the MIR codegen path already covers. Phase 16d
starts with an audit to identify what is genuinely still needed, ports what must be ported, and
retires what MIR codegen has made redundant. This is the largest single sub-task in Phase 16 and
the last step before full end-to-end IR-diff verification.

## What Changes

After a retirement audit, the remaining needed files from `compiler/src/codegen/llvm/` are
replaced by TML equivalents in `compiler-tml/src/codegen/`. Files fully covered by the MIR
codegen path are deleted from C++ without replacement. The `compiler/src/codegen/llvm/` directory
is progressively emptied as coverage expands.

### Architecture

```
compiler-tml/src/codegen/
  emit_intrinsic.tml   — builtin and LLVM intrinsic calls (Phase 2)
  emit_drop.tml        — destructor emission and drop glue (Phase 3)
  emit_derive.tml      — auto-derived Clone, Eq, Hash, Ord impls (Phase 4)
  emit_let.tml         — destructuring let, let-else patterns (Phase 5)
  runtime_decls.tml    — selective runtime function declarations (Phase 6)

compiler-tml/src/codegen/legacy/
  retirement-log.md    — which C++ files were retired, why, and what covers them
```

### Key Design Decisions

- **Retire before porting** — the audit in Phase 1 is non-negotiable. The 41K LOC estimate is
  the total legacy LLVM codegen size, but a significant fraction may already be covered by the
  MIR codegen path. Porting code that will be deleted is wasted effort. The audit runs IR-diff
  on the full stdlib to find the actual gap, then ports only that gap.
- **Drop glue in reverse field order** — destructor emission must drop fields in the reverse of
  their declaration order, matching the C++ `core/drop.cpp` behavior and the TML language
  specification. Getting drop order wrong causes use-after-free when a dropped field referenced
  another field's memory. The `DropEmitter` iterates the field list in reverse.
- **Derived impls are pure TML-level expansion** — `#[derive(Clone)]` could be handled at the
  HIR or THIR level before codegen. However, the current C++ implementation generates IR
  directly in `compiler/src/codegen/llvm/derive/`. The TML port matches this structure; a future
  refactor can move derived impls to the HIR lowering phase where they belong architecturally.
- **Runtime declarations are a maintenance liability** — the C++ legacy codegen declares all
  500+ runtime functions in every compiled module. The TML implementation uses selective
  declaration (tracking which functions the module actually calls) which was introduced in
  phase16a item 5.3. Phase 16d's `runtime_decls.tml` extends this to the remaining runtime
  functions not yet covered by the MIR codegen path.
- **Cranelift is low priority** — the Cranelift backend in `compiler/src/codegen/cranelift/` is
  an experimental alternative to LLVM. It is not used in the CI test suite. Phase 16d audits
  its usage and defers porting unless it is exercised by tests. The TML compiler's primary
  backend is and will remain LLVM for the foreseeable future.
- **Incremental retirement** — C++ files are not all deleted at once. Each file is removed only
  after the TML equivalent passes IR-diff for the patterns that file handled. The `retirement-log.md`
  tracks this mapping explicitly so any regression can be traced to the specific deletion.

### Scope Reduction Strategy

The 41K LOC estimate is a worst-case. The actual porting scope will be smaller because:

1. **Patterns already covered by MIR path** — binary ops, struct construction, GEP, method calls
   (phases 16b–16c) handle the vast majority of expression codegen. Most `llvm/expr/` files
   may be retirable after IR-diff confirms coverage.
2. **Dead code** — some legacy LLVM files implement code paths for AST patterns that no longer
   reach codegen (the THIR→MIR path handles them). The audit will identify these.
3. **Cranelift deferral** — if Cranelift is confirmed unused in CI, its entire directory (~5K LOC)
   is excluded from scope.

Realistic porting scope after audit: estimated 15–20K LOC → ~10–13K TML.

## Impact

- Affected code: all remaining files in `compiler/src/codegen/llvm/` (retired or replaced)
- Affected phases: phase17c (bootstrap verification) depends on complete TML codegen
- Breaking change: NO — full-suite IR-diff (item 8.1) ensures character-identical output before
  any C++ file is retired
- User benefit: eliminates the last major C++ codegen subsystem; TML compiler fully self-hosting
  for the codegen layer

## Success Criteria

IR-diff on all 93 stdlib modules shows zero differences between TML and C++ codegen output. The
full 1,659-test suite passes with the TML codegen path active. The `compiler/src/codegen/llvm/`
directory is empty or contains only files explicitly deferred with written justification.

## Dependencies

- **Requires**: phase16c (complete MIR codegen path — types, instructions, calls — all passing
  IR-diff before the retirement audit begins)
- **Blocks**: phase17c (bootstrap verification needs zero-diff codegen across all modules)
- **Risk**: HIGH — large scope, diverse patterns, and retirement decisions require correct IR-diff
  tooling. Mitigated by the audit-first strategy (item 1.1–1.3) and incremental retirement with
  per-file IR-diff confirmation before deletion.
