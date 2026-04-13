# Proposal: phase30c_native-method-dispatch-full

## Why
The native method dispatch layer currently handles only basic inherent method calls
on user-defined structs. Calls to collection methods (List.push, HashMap.set),
array and slice operations, primitive extensions (I64.to_string, Str.len), and
the Maybe/Outcome monad methods all fall back to the LLVM backend. Because these
types appear in virtually every TML program, the native backend cannot compile
any practical code without full method dispatch coverage.

## What Changes
- `compiler-tml/src/native/x86/emit_method.tml` is extended with dispatch branches
  for each type category:
  - Collections: `List` (push, get, len, pop, is_empty) and `HashMap` (set, get,
    has, len, remove) via their runtime ABI.
  - Arrays: indexed get/set, len, slice construction.
  - Slices: len, get, iter pointer arithmetic.
  - Primitive extensions: `I64.to_string`, `F64.to_string`, `Str.len`,
    `Str.contains`, `Str.starts_with`, `Str.slice`.
  - `Maybe[T]`: unwrap, unwrap_or, map (calls closure), is_just, or_else.
  - `Outcome[T,E]`: unwrap, map, map_err, is_ok, unwrap_err.

## Impact
- Affected specs: native-backend/method-dispatch
- Affected code: compiler-tml/src/native/x86/emit_method.tml
- Breaking change: NO
- User benefit: All standard-library method calls on collections, primitives, Maybe, and Outcome compile and run natively.
