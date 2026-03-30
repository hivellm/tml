---
name: nested-generic-typesubs-fix
description: Fix for nested generic type_subs in method_impl.cpp — where-clause constraint destructuring for transpose/flatten on Maybe/Outcome
type: project
---

## Problem
When calling methods from specialized impls (e.g., `impl[T] Maybe[Maybe[T]]`) or methods with where-clause constraints that destructure type params (e.g., `where T: Outcome[T, E]`), the codegen produced wrong LLVM IR return types.

### Affected Tests
- `option_flatten` — `flatten()` on `Maybe[Maybe[I32]]` returned `Maybe__Maybe__I32` instead of `Maybe__I32` (FIXED)
- `result_transpose` — `flatten()` on `Outcome[Outcome[I32,Str],Str]` returned `Outcome__Outcome__I32__Str__Str` instead of `Outcome__I32__Str` (FIXED)
- `option_transpose2` — `transpose()` undefined function (deeper module loading issue, NOT FIXED)
- `outcome_transpose2` — `transpose()` invalid GEP indices (deeper module loading issue, NOT FIXED)
- `option_iter2` — `MaybeIter::next()` returned `Maybe__T` (module loading issue, NOT FIXED)

### Root Causes

**Cause 1 (FIXED): Naive positional type_subs for specialized impls**
- File: `compiler/src/codegen/llvm/expr/method_impl.cpp`
- When receiver is `Maybe[Maybe[I32]]`, the naive mapping gives `T = Maybe[I32]` (from `type_params[0] → type_args[0]`)
- For specialized `impl[T] Maybe[Maybe[T]]`, T should be I32
- Previous fix: nesting detection heuristic at line ~637 (detects when concrete type has same name as receiver and substituted return type matches receiver type)
- Existing specialized impl check (lines 574-621) works for LOCAL impls in `pending_generic_impls_all_`, but option.tml's impls aren't registered there

**Cause 2 (FIXED): Where-clause constraint destructuring not applied**
- The `transpose` method has `where T: Outcome[T, E]` — a constraint (`:` syntax) not equality (`=` syntax)
- `resolve_impl_where_clause` only handles `type_equalities`, not `constraints`
- Fix: new code at lines ~1012-1072 that extracts type params from concrete types in type_subs by matching against the concrete type's own type_params
- Example: T = Outcome[I32, Str], Outcome has type_params ["T", "E"] → extracts T = I32, E = Str

**Cause 3 (NOT FIXED): option.tml/result.tml not in module registry**
- option.tml and result.tml define methods on builtin enums (Maybe, Outcome)
- These modules are NOT registered in the module registry with source_code
- Their functions exist only in TypeEnv (from type checker) but not in mod.functions
- `pending_generic_impls_all_["Maybe"]` has 13 impls from behavior modules (PartialEq, Clone, etc.) but NONE from option.tml
- This prevents: (a) specialized impl detection via AST parsing, (b) generic method instantiation for transpose/iter/next
- The nesting detection heuristic (Cause 1 fix) works around this for flatten by detecting the pattern without AST access

### Key Discovery: Module Registry Gap
- 45 modules in registry, all have source_code
- NO `core::option`, `core::types::option`, or `core::result` modules exist
- option.tml functions are registered in TypeEnv by type checker but not in any module's functions map
- Binary cache at `build/debug/cache/meta/core/types/option.tml.meta` exists, so the file IS loaded during type checking
- The codegen's runtime_modules.cpp only processes modules in the registry → option.tml methods never get generated

### Files Modified
- `compiler/src/codegen/llvm/expr/method_impl.cpp` (lines ~1012-1072): Enhanced type_subs extraction to handle both unresolved AND self-referential type params from concrete types

**Why:** Understanding this module registry gap is critical for future fixes. The 3 remaining failures all need option.tml/result.tml loaded with source code.

**How to apply:** When fixing module loading for option/result, ensure their source_code is preserved in the module registry so codegen can parse ASTs for specialized impl detection and generic method instantiation.
