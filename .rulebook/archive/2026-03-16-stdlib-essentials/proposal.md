# Proposal: stdlib-essentials Phase 2

## Why

The previous stdlib-essentials iteration (archived 2026-02-26) reached 98% completion.
The remaining 2% requires compiler changes that were not yet implemented:
generic Iterator behavior, slice type parameters, and function type parameters.
This task tracks exactly what the compiler needs to implement the remaining stdlib items.

## What Changes

### Compiler (Phase 1)

1. **`behavior Iterator[T]`** — core trait for all iteration in TML.
   - File: `lib/core/src/iter.tml` (update), `compiler/src/types/` (add associated type support)
   - Required for: Vec::from_iter, HashSet iterator, BTreeMap/BTreeSet ordered iteration, Lines as Iterator

2. **Slice types `[T]` / `ref [T]`** — first-class slice parameters.
   - Files: `compiler/src/parser/`, `compiler/src/types/`, `compiler/src/codegen/`
   - Required for: `choose[T]`, `Rng::fill_bytes(buf: ref [U8])`

3. **Function pointer types `func(A) -> B`** — higher-order functions.
   - Files: `compiler/src/types/`, `compiler/src/codegen/`
   - Required for: `Vec::retain`, `Vec::drain`, closures passed as predicates

4. **Compound constraints `T: A + B`** — multi-behavior bounds.
   - Files: `compiler/src/types/`
   - Required for: `Distribution[T]`, `Random[T]`

### stdlib (Phase 2, unblocked after compiler)

- Vec: `from_iter`, `retain`, `drain`, `impl Iterator`
- HashSet: `from_iter`, `impl Iterator`
- BTreeMap/BTreeSet: `impl Iterator`
- BufReader::Lines: `impl Iterator[Str]`
- `env::vars()` iterator
- `choose[T]`, `random[T]`, `Distribution[T]` behavior

## Impact

- Affected specs: `docs/04-TYPES.md` (slice types, function types), `docs/05-SEMANTICS.md` (Iterator)
- Affected code: compiler type checker, parser, codegen; `lib/core/src/iter.tml`; multiple stdlib files
- Breaking change: NO — all additions, no removals
- User benefit: Full iterator support enables idiomatic TML code (`for x in collection`),
  generic algorithms, and functional-style collection processing
