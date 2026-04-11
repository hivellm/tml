# TML Codegen Analysis: Comparison with Rust, Go, and Clang

**Date**: 2026-04-03
**Scope**: MIR-to-LLVM IR code generation (`compiler/src/codegen/mir*.cpp`, ~5,700 LOC)
**Reference compilers**: rustc (MIR→LLVM), Go gc (SSA→machine code), Clang (AST→LLVM)

## Executive Summary

The TML MIR codegen works — it passes 1,659 tests and handles a 500+ type standard library. But it has **structural problems** that cause recurring bugs, each requiring bespoke patches. Analysis of the codebase reveals **7 fundamental architectural differences** from established compilers that explain the pattern of repeated codegen failures.

### The Core Problem

TML's codegen operates on **string-typed LLVM IR text** with **retroactive type discovery**. It generates LLVM IR by concatenating strings, and when type information is missing, it falls back to `i32`. This creates a cascade of problems:

1. Type mismatches discovered at LLVM verification time (not codegen time)
2. Implicit coercions scattered throughout (13+ inttoptr patterns, 23+ make_i32_type fallbacks)
3. No clear value/pointer distinction (the "is this a value or a pointer?" problem)
4. ABI decisions made ad-hoc at each call site (no centralized ABI lowering)

### Key Metrics from Code Analysis

| Metric | TML | What Rust/Go/Clang do |
|--------|-----|----------------------|
| `make_i32_type()` fallbacks | **23 sites** | 0 — types guaranteed before codegen |
| `inttoptr` coercions | **13 sites** | 0 — pointer/integer distinction in type system |
| `value_types_` side-table lookups | **61 sites** | N/A — types are intrinsic to IR values |
| `"void"` string comparisons | **61 sites** | 0 — Unit has a proper type representation |
| `starts_with("%struct.")` checks | **10+ sites** | 0 — aggregate types are typed IR objects |
| String-based IR generation | **5,700 LOC** | 0 — all use typed IR builder APIs |
| Centralized ABI handling | **none** | Yes — ABIInfo/FnABI/ABIArg |

### The 7 Structural Problems

1. **No Type Guarantee** — Codegen doesn't enforce that all values have types before emission
2. **String-Based IR** — Concatenating LLVM IR text instead of using LLVM C API / IRBuilder
3. **No ABI Abstraction** — Win64 struct passing rules scattered across 10+ call sites
4. **Value/Pointer Ambiguity** — No distinction between RValues and LValues
5. **Retroactive Type Discovery** — Types looked up from 4+ side-tables with fallback chains
6. **Monolithic Intrinsic Handling** — 1,357 LOC of special-case function name matching
7. **Dual Code Path Divergence** — Legacy AST codegen and MIR codegen duplicate logic

### Impact Assessment

| Problem | Bug Frequency | Fix Difficulty | Structural Fix |
|---------|--------------|----------------|----------------|
| No type guarantee | Weekly | Easy (patch) | Hard (pipeline change) |
| String-based IR | Constant | Easy (patch) | Very hard (rewrite) |
| No ABI abstraction | Monthly | Medium | Medium (new module) |
| Value/Pointer ambiguity | Weekly | Hard | Medium (new types) |
| Retroactive type discovery | Weekly | Medium | Hard (MIR enrichment) |
| Monolithic intrinsics | Monthly | Easy | Medium (table-driven) |
| Dual path divergence | Monthly | Medium | Hard (remove legacy) |

### Recommended Priority

1. **[HIGH] Enrich MIR types** — Guarantee every Value has a non-null MirTypePtr
2. **[HIGH] Create ABI module** — Centralize struct/enum passing decisions
3. **[HIGH] Introduce CGValue** — Wrap LLVM values with type + pointer/value distinction
4. **[MEDIUM] Table-driven intrinsics** — Replace if/else chains with dispatch table
5. **[MEDIUM] Eliminate i32 fallbacks** — Make missing type a hard error
6. **[LOW] Migrate to LLVM C API** — Replace string concat with IRBuilder
7. **[LOW] Remove legacy codegen** — Single path through MIR

See detailed analysis in the following documents:
- [02-TYPE-SYSTEM.md](02-TYPE-SYSTEM.md) — Type tracking comparison
- [03-ABI-CALLING-CONVENTION.md](03-ABI-CALLING-CONVENTION.md) — ABI handling comparison
- [04-VALUE-REPRESENTATION.md](04-VALUE-REPRESENTATION.md) — Value/pointer distinction
- [05-INTRINSICS-DISPATCH.md](05-INTRINSICS-DISPATCH.md) — Intrinsic lowering comparison
- [06-IR-GENERATION-STRATEGY.md](06-IR-GENERATION-STRATEGY.md) — Text IR vs builder API
- [07-RECOMMENDED-CHANGES.md](07-RECOMMENDED-CHANGES.md) — Prioritized action plan
