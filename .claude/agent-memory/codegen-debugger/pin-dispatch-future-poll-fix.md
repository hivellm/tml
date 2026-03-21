---
name: Pin-dispatch and Future::poll return type fix
description: Pin[mut ref T].method() dispatch + ref-wrapped generic param inference + type checker associated type resolution for behaviors (2026-03-21, PARTIAL)
type: project
---

## Future::poll Return Type Resolution (2026-03-21, PARTIAL)

### Bug
`Ready[I32]::poll()` returned `()` instead of `Poll[I32]`. Calling `pinned.poll(mut ref ctx)` on `Pin[mut ref Ready[I32]]` failed at type check, codegen, and method dispatch.

### Root Cause Chain (4 interconnected bugs)

**Bug 1: Type checker didn't unwrap Pin for behavior method lookup**
- File: `compiler/src/types/checker/expr_call_method.cpp` (line ~1612)
- When `Pin[mut ref Ready[I32]].poll()` was called, the type checker looked up `Pin::poll` (no such method), then checked behaviors on `Pin` (not Future), fell through to `return make_unit()`.
- Fix: Added Pin-dispatch section before `return make_unit()`. When receiver is `Pin[ref T]` or `Pin[mut ref T]` and method not found on Pin, unwrap to get T and look up T::method. Handles both function registry and behavior method lookup with correct associated type resolution.

**Bug 2: Pin::new(mut ref f) — ref-wrapped bare generic param not inferred (CallExpr path)**
- File: `compiler/src/codegen/llvm/expr/call_generic_struct.cpp` (line ~823)
- `Pin::new(mut ref f)` is parsed as a `CallExpr` (PathExpr "Pin::new"), NOT a MethodCallExpr.
- Case 4 inference handled `ref NamedType[T]` (matching name == name) but not bare `ref T` where T is a generic param.
- The param `mut ref T` has inner NamedType("T") with no type_args, so Case 4 skipped (line 816: `if (type_args.empty()) continue`).
- Fix: Added Case 4b — when inner is a bare generic param matching func_sig->type_params, infer T from the arg's inner type. Also map the struct-level param (P -> mut ref T when T is inferred).

**Bug 3: Same bug in MethodCallExpr path (method_static_dispatch.cpp)**
- File: `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp` (line ~800)
- Same issue as Bug 2 but for MethodCallExpr-style calls (e.g., `Type[T]::new(args)`).
- Fix: Added Case 2 (ref-wrapped bare generic param inference) after Case 1.

**Bug 4: Codegen method dispatch didn't unwrap Pin for impl methods**
- File: `compiler/src/codegen/llvm/expr/method_impl.cpp` (line ~141)
- `try_gen_impl_method_call` looked up `Pin::poll` and returned nullopt.
- Fix: Before method lookup, check if receiver is `Pin[X]` and `Pin::method` doesn't exist. If so, unwrap to inner type for dispatch.

### Key Discovery: Pin::new is a CallExpr, NOT MethodCallExpr
- `Pin::new(mut ref f)` parses as CallExpr with callee PathExpr(["Pin", "new"])
- This goes through `gen_call_expr` → `gen_call_generic_struct_method` (call_generic_struct.cpp)
- NOT through `gen_method_call` → `gen_method_static_dispatch`
- The condition at parser_expr.cpp:632 (`if generics.has_value() && match(ColonColon)`) only creates MethodCallExpr when there are generic args on the type

### Remaining Issues (NOT fixed)
1. **Pin::new overload collision**: `impl[T] Pin[ref T]::new` and `impl[T] Pin[mut ref T]::new` share the key `Pin::new` in the function registry. The wrong overload body may be generated (returns `Pin__ref_T` instead of `Pin__mutref_T`).
2. **Maybe::take() dispatch**: Inside Ready::poll body, `this.value.take()` fails because `take()` on `Maybe[T]` isn't found during method dispatch in the generated code.
3. **last_semantic_type_** missing in several static dispatch paths — added it for the paths touched in this fix.

### Files Changed
- `compiler/src/types/checker/expr_call_method.cpp` — Pin-dispatch in type checker
- `compiler/src/codegen/llvm/expr/call_generic_struct.cpp` — Case 4b + last_semantic_type_
- `compiler/src/codegen/llvm/expr/method_impl.cpp` — Pin-dispatch in impl method lookup
- `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp` — Case 2 + last_semantic_type_
