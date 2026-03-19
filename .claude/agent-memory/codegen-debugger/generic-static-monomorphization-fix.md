---
name: Generic static method return type monomorphization fix
description: infer_expr_type returned unsubstituted return types for generic functions, causing __T in method mangling
type: project
---

## Bug: Generic function return type not monomorphized in infer_expr_type

**Fixed**: 2026-03-19

### Root Cause

When `let x = generic_func(arg)` is compiled, `infer_expr_type` in `infer.cpp` determines the semantic type of `x`. For generic functions like `make_promises[T](first: T, second: T) -> MyList[Promise[T]]`:

1. Generic functions are deferred to `pending_generic_funcs_` and NOT registered in `func_return_types_`
2. `infer_expr_type` falls through to `env_.lookup_func()` which returns the FuncSig with raw return type `MyList[Promise[T]]`
3. The raw return type was returned directly WITHOUT substituting `T=I32` from the call arguments
4. This caused `x` to have semantic type `MyList[Promise[T]]`
5. When `x.len()` was called, `try_gen_impl_method_call` used `named.type_args` from the receiver (containing `T`) to mangle the method name as `MyList__Promise__T_len`

**Key insight**: The type checker stores `T` as `NamedType("T")`, NOT as `GenericType`. So `contains_unresolved_generic()` returns false. The fix uses `!func_sig->type_params.empty()` as the condition instead.

### Fix Location

`compiler/src/codegen/llvm/expr/infer.cpp`, lines ~1564-1620 (in the `env_.lookup_func` path)

When a function has `type_params`, look up its parser declaration in `pending_generic_funcs_`, use `unify_types()` to infer type args from call arguments, and apply `types::substitute_type()` to the return type.

### Files Changed

- `compiler/src/codegen/llvm/expr/infer.cpp` — Added generic return type resolution in `infer_expr_type` for both `env_.lookup_func` and module registry paths

### Important Details

- `pending_generic_funcs_` maps function name to `parser::FuncDecl*` (parser-level types needed for `unify_types`)
- `unify_types(parser_pattern, semantic_concrete, generic_names, bindings)` matches parser type patterns against inferred argument types
- `types::substitute_type(type, bindings)` applies the substitution map to produce concrete types
- Also handles calls inside monomorphized functions via `current_type_subs_` fallback
