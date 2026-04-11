# Language Ergonomics Analysis

**Date**: 2026-04-11
**Scope**: compiler-tml/src, lib/core/src, lib/std/src
**Method**: Static analysis of real usage patterns across 20+ files

## Executive Summary

Analysis of the TML self-hosting compiler, core library, and standard library reveals
15 categories of ergonomic friction. The top 5 improvements by impact would eliminate
~500+ lines of boilerplate and address the most common pain points encountered by
TML authors.

## Priority Matrix

| # | Improvement | Impact | Files Affected | Est. Boilerplate Removed |
|---|-------------|--------|----------------|--------------------------|
| 1 | [For-each loops & ranges](01-loop-iterator.md) | Critical | 30+ patterns | ~150 lines |
| 2 | [Struct update syntax](02-struct-update.md) | Critical | 50+ call sites | ~200 lines |
| 3 | [Pattern guards](03-pattern-guards.md) | High | 40+ nested when | ~100 lines |
| 4 | [Operator overloading](04-operator-overload.md) | High | 20+ methods | Readability |
| 5 | [Derive macros / auto-serialization](05-derive-macros.md) | High | 80+ manual lines | ~150 lines |
| 6 | [Bool struct fields](06-bool-fields.md) | Medium | 15+ workarounds | Type safety |
| 7 | [Closure shorthand](07-closure-shorthand.md) | Medium | Iterator API | Readability |
| 8 | [Wildcard match arms](08-wildcard-match.md) | Medium | Large enums | ~80 lines |
| 9 | [Module circular imports](09-module-cycles.md) | Medium | AST modules | Architecture |
| 10 | [Lowlevel abstractions](10-lowlevel-abstractions.md) | Medium | Collections | Safety |
| 11 | [String slicing syntax](11-string-slicing.md) | Low | String ops | Readability |
| 12 | [Generic monomorphization](12-generic-mono.md) | Low | Collections | Performance |
| 13 | [Safe numeric conversions](13-numeric-conversions.md) | Low | Arithmetic code | Safety |
| 14 | [Trait aliases](14-trait-aliases.md) | Low | Generic bounds | Readability |
| 15 | [Destructuring assignment](15-destructuring.md) | Low | Result handling | Readability |

## Ambiguity Review

All 15 findings were cross-referenced against TML's design principles
(ADR-008 LL(1), RFC-0001 no-coercion, RFC-0002 syntax spec). See
[ambiguity-review.md](ambiguity-review.md) for the full analysis.

### Rejected sub-proposals (violate design principles)

| Finding | Rejected part | Reason |
|---------|---------------|--------|
| F07-C | `_` placeholder in closures | Ambiguous with wildcard pattern (breaks LL(1)) |
| F10-A | Inline `lowlevel` without `{ }` | No block boundary → unbounded lookahead (breaks LL(1)) |
| F13 | Implicit numeric widening | Violates no-implicit-coercion rule (RFC-0001) |

### Originally used wrong syntax (fixed)

| Finding | Wrong syntax | Correct TML syntax |
|---------|-------------|-------------------|
| F01 | `0..n`, `0..=n` | `0 to n`, `0 through n` |
| F11 | `s[2..5]` | `s[2 to 5]` |

### Already in spec (compiler implementation gaps, not language design)

| Finding | RFC-0002 reference |
|---------|-------------------|
| F01 for-in loops | §2.3 `ForExpr`, §3.4 desugaring rules |
| F03 pattern guards | §2.5 `AndPattern <- PrimaryPattern ('if' Expr)?` |
| F08 wildcard `_` | §2.5 `WildcardPattern <- '_'` |
| F15 destructuring | §2.4-2.5 `LetStmt` + `StructPattern` |

## Implementation Tasks (phase30)

9 tasks created for the features approved for implementation:

| Task | Finding | Status | Scope |
|------|---------|--------|-------|
| `phase30a_for-in-loops` | F01 | pending | Parser + HIR desugaring + Range types |
| `phase30b_pattern-guards` | F03 | pending | Parser + MIR pattern matching |
| `phase30c_struct-update-syntax` | F02 | pending | Parser + type checker + codegen |
| `phase30d_operator-overloading` | F04 | pending | Core behaviors + type checker + codegen |
| `phase30e_auto-repr-directives` | F05 | pending | Directive system + codegen |
| `phase30f_bool-layout-fix` | F06 | pending | LLVM type lowering (i1 → i8) |
| `phase30g_closure-type-inference` | F07-A/B | pending | Type checker bidirectional inference |
| `phase30h_behavior-aliases` | F14 | pending | Parser + type checker |
| `phase30i_destructuring-let` | F15 | pending | Parser + HIR/MIR desugaring |

**Not tasked** (deferred or out of scope):
- F08 wildcard `_` — already works in compiler
- F09 module cycles — architectural change, needs separate RFC
- F10 lowlevel — only @packed (covered by F05); inline lowlevel rejected
- F11 string slicing — depends on F04 (operator overloading)
- F12 monomorphization — major backend rewrite, separate initiative
- F13 numeric conversions — only `.abs()` methods (library work, no task needed)

## Spec & Grammar Updates

Updated in this analysis:
- **03-GRAMMAR.md**: Added pattern guards (§5.4), behavior aliases (§3.3), struct update rule (§5.9), operator behaviors (§3.3.1), built-in directives (§7.1)
- **tml.peg**: Added BehaviorAlias alternative, ClosureParam with optional type

## Findings

Each finding includes:
- **Problem**: What pattern is verbose or error-prone
- **Evidence**: File paths, line numbers, code samples
- **Proposal**: C++ compiler change needed
- **Complexity**: Estimated implementation effort
