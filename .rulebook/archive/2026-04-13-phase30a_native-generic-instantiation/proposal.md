# Proposal: phase30a_native-generic-instantiation

## Why
TML's type system is fully generic: `List[T]`, `HashMap[K,V]`, and user-defined
generic functions are used pervasively throughout the standard library. The native
backend currently only emits code for monomorphic (non-generic) call sites. Any
call to a generic function silently falls through to the LLVM path, making the
native backend unusable for real programs. Without monomorphisation, generics —
which cover the majority of all TML code — cannot compile natively.

## What Changes
- `compiler-tml/src/native/pipeline.tml` gains a generic call registry that
  collects every (function, type-argument-list) pair encountered during MIR
  traversal.
- A type-substitution pass replaces type parameters in MIR instruction operands
  with the concrete arguments for each instantiation.
- The pipeline emits one concrete function body per unique instantiation, with a
  mangled name (`List_I64__push`, `HashMap_Str_I64__get`, etc.) to avoid symbol
  collisions.
- A deduplication set ensures each instantiation is emitted exactly once even if
  called from multiple sites.

## Impact
- Affected specs: native-backend/generics
- Affected code: compiler-tml/src/native/pipeline.tml
- Breaking change: NO
- User benefit: Generic functions such as `List[I64].push()` and `HashMap[Str,I64].get()` compile and link correctly through the native backend.
