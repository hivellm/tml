# phase0f — Decorators tell the truth (`@inline(never)`, `@repr`)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-143/L-008/L-142 (08-design-coherence.md, 01-type-system-generics.md). Two
> small, surgical fixes to performance-critical knobs that currently do nothing
> or the opposite of their documentation.

## Motivation

`@inline(never)` dispatches on the decorator *name* only, so it sets
`has_inline_decorator = true` and emits LLVM `alwaysinline` — the exact
opposite of the documented effect
(`compiler/src/codegen/llvm/decl/func.cpp:602-611,1173-1179,637-641`).
`@repr(U8)` is validated by the checker
(`compiler/src/types/checker/decl_struct.cpp:715-737`) and then never reaches
enum codegen — tags are always i32 (`decl/enum.cpp:250-269`) — while the
project's own rule file recommends it "to control discriminant size".

## 1. Implementation
- [ ] 1.1 `@inline` argument dispatch: branch on `decorator.args` — `never` →
  noinline, `always`/empty → alwaysinline. IR probes assert both attributes.
- [ ] 1.2 Honor `@repr(U8/U16/U32)` in enum tag emission: tag width follows the
  repr (default stays i32). Layout probes assert the i8 tag and the struct-size
  shrink; audit payload-slot alignment for the narrower tag.
- [ ] 1.3 Standardize decorator argument syntax on the equals-form the parser
  actually accepts; fix the colon-form examples in docs/specs/25-DECORATORS.md
  (L-142) — doc sweep, no parser change.
- [ ] 1.4 Full quality gate — @repr layout changes can surface latent width
  assumptions; fix any.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (25-DECORATORS.md examples + semantics table
  match the implementation)
- [ ] 2.2 Write tests covering the new behavior (IR-assertion fixtures for
  inline/noinline; size/layout fixtures for @repr enums)
- [ ] 2.3 Run tests and confirm they pass
