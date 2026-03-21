---
name: fnptr-literal-coercion-fix
description: Function pointer call integer literal arguments not coerced to match declared parameter types (i32 42 passed where i64 expected)
type: project
---

## Bug: Function Pointer Call Integer Literal Coercion (2026-03-20, FIXED)

**Symptom**: `f(42)` where `f: func(I64) -> I64` crashed with access violation. Only happened when closure call coexisted with List operations in the same function (the List operations amplified the stack corruption).

**Root cause**: In AST codegen `call.cpp`, all 3 function pointer call paths (FieldExpr fat pointer, IdentExpr fat pointer, IdentExpr thin pointer) built argument lists using:
```cpp
std::string val = gen_expr(*call.args[i]);
user_args.push_back({val, last_expr_type_});
```
For integer literal `42`, `gen_expr` returns `"42"` with `last_expr_type_ = "i32"`. But the function's declared parameter type is `I64` (i64 in LLVM). No coercion was applied, producing `call i64 %fn(i32 42)` — an ABI mismatch.

**Fix**: Extract declared parameter types from `FuncType.params` or `ClosureType.params` and sext integer args when `src_bits < dst_bits`. Applied to 3 sites in `compiler/src/codegen/llvm/expr/call.cpp`.

**Why MIR path was unaffected**: `emit_indirect_call` in `instructions.cpp` already used `mir_func_type.params` to determine arg types (line 1717-1720), not expression-inferred types.

**How to apply**: Any new function pointer call codegen must use declared parameter types for arguments, not expression-inferred types. The `int_bits` lambda pattern is the standard coercion idiom in call.cpp.
