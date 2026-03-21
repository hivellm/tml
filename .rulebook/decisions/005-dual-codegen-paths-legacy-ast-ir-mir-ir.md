# 5. Dual Codegen Paths: Legacy AST→IR + MIR→IR

**Status**: proposed
**Date**: 2026-03-15
**Related Tasks**: optimize-codegen-like-rust, codegen-structural-fixes

## Context

MIR codegen was introduced as the architecturally superior path (~3K lines vs ~49K lines legacy), but the legacy LLVMIRGen path still generates ALL standard library function implementations. The MIR path handles user code in normal builds.

## Decision

Both paths coexist. MIR is primary for user code via query pipeline. Legacy generates library IR and is available via --legacy flag. THIR builder creates CallInst directly (not MethodCallInst), so MIR dispatch bypasses legacy method dispatch infrastructure.

## Alternatives Considered

- Complete migration to MIR (requires library codegen support)
- Remove MIR and fix legacy only (abandons architectural improvement)
- Run both and cross-validate (doubles compilation time)

## Consequences

Critical trade-off: maintenance burden doubles for codegen fixes. Bugs can exist in one path but not the other. Legacy elimination requires MIR to handle: library IR generation, debug info, coverage instrumentation, async codegen. Investment should prioritize closing MIR gaps rather than improving legacy. The 'legacy' label is misleading — it's still essential.
