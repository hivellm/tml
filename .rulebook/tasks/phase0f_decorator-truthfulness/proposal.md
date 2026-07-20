# Proposal: phase0f_decorator-truthfulness

## Why
`@inline(never)` silently emits `alwaysinline` (inverted), and `@repr` is
validated then ignored — both are load-bearing performance knobs whose
documented behavior is fiction today (analysis L-143, L-008).

## What Changes
Inline dispatch reads its argument; enum tag width follows @repr; decorator
argument syntax examples standardized on the form the parser accepts.

## Impact
- Affected specs: docs/specs/25-DECORATORS.md
- Affected code: compiler/src/codegen/llvm/decl/func.cpp, compiler/src/codegen/llvm/decl/enum.cpp
- Breaking change: YES for @repr users (enum layout/ABI changes to what was documented); NO otherwise
- User benefit: performance annotations do what they say; smaller enums where requested
