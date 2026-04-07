# Section 4 — Body Checking and Inference

**Sources**: `compiler/src/types/checker/expr.cpp` (652 LOC),
`compiler/src/types/checker/expr_call.cpp` (802 LOC),
`compiler/src/types/checker/expr_call_method.cpp` (1363 LOC),
`compiler/src/types/checker/expr_call_method_types.cpp` (668 LOC),
`compiler/src/types/checker/expr_ops.cpp` (partial, binary/unary ops),
`compiler/src/types/checker/types_checker.cpp` (closure checking),
`compiler/src/types/env_core.cpp` (unification engine)

---

## 4.1 Expression Dispatch

The entry point for all expression type checking is `TypeChecker::check_expr`, which exists in two overloads.

### Overload 1 — Unconditional (`expr.cpp:137`)

```cpp
auto TypeChecker::check_expr(const parser::Expr& expr) -> TypePtr
```

Dispatches via `std::visit` over the `expr.kind` variant. Every known expression kind has a dedicated handler. The fallthrough branch at line 209 returns `make_unit()` for unrecognized kinds — this is a silent fallback that does NOT emit an error.

### Overload 2 — With Expected Type (`expr.cpp:215`)

```cpp
auto TypeChecker::check_expr(const parser::Expr& expr, TypePtr expected_type) -> TypePtr
```

Handles only a subset of expression kinds that benefit from expected-type propagation:
- `LiteralExpr` — forwards to `check_literal` with the expected type
- `UnaryExpr` (negation only) — forwards to `check_literal` on the operand
- `CallExpr` — forwards to `check_call` with the expected return type
- `ArrayExpr` — forwards to `check_array` with the expected element type
- `TupleExpr` — decomposes `expected_type` to a `TupleType` and propagates per-element
- `ClosureExpr` — calls `check_closure` then unifies inferred types with expected `FuncType`

All other expression kinds fall through to overload 1, ignoring the expected type entirely.

**Invariant I-4.01**: Expected-type propagation only reaches `LiteralExpr`, `UnaryExpr` (negation), `CallExpr`, `ArrayExpr`, `TupleExpr`, and `ClosureExpr`. All other expression kinds are checked without context. The HIR builder must not assume that every expression node carries the type its context expects.

---

## 4.2 Literal Type Inference

`check_literal` is implemented in two forms: `check_literal(lit)` calls `check_literal(lit, nullptr, false)` — all logic is in the three-argument version at `expr.cpp:303`.

### Integer Literals (`expr.cpp:325`)

Resolution order:
1. If the literal has a **suffix** (`i8`, `i16`, `i32`, `i64`, `i128`, `u8`, `u16`, `u32`, `u64`, `u128`): map suffix to `PrimitiveKind`, run overflow check via `check_int_overflow`, return that type. Unknown suffixes silently default to I64 (line 352).
2. If no suffix and `expected_type` is a primitive integer kind: use `expected_type`, running the same overflow check (line 373).
3. Default: return `make_i64()` (line 379).

**Invariant I-4.02**: An unsuffixed integer literal infers as I64 unless an expected type is present that is a primitive integer type. There is no bidirectional flow: once the literal is type-checked, its type is fixed. Coercion from I64 to I32 at call sites does not happen automatically through a second pass.

**Invariant I-4.03**: Overflow is checked at type-checking time for suffixed literals and for literals with an explicit expected type. For default I64 inference without a suffix, no overflow check occurs. The maximum value for I64 literals is `9223372036854775807` for non-negated; for negated literals `is_negated=true` allows the magnitude `9223372036854775808` to represent `i64::MIN` (line 311).

**Invariant I-4.04**: Assigning a negative literal (negated `UnaryExpr` wrapping a `LiteralExpr`) to an unsigned type emits error `T050` with message "Cannot assign negative value to unsigned type X" (expr.cpp:231). The check occurs before the literal is forwarded to `check_literal`, inside the `UnaryExpr` branch of `check_expr` overload 2.

### Float Literals (`expr.cpp:381`)

Resolution order:
1. Suffix `f32` → `F32`, suffix `f64` → `F64`.
2. No suffix: if `expected_type` is `F32` or `F64`, use it.
3. Default: `make_f64()`.

### String, Char, Bool, Null Literals (`expr.cpp:398`)

| Literal kind    | Resulting type    | Notes                         |
|-----------------|-------------------|-------------------------------|
| StringLiteral   | `Str` (primitive) | —                             |
| CharLiteral     | `Char` primitive  | —                             |
| BoolLiteral     | `Bool` primitive  | —                             |
| NullLiteral     | `Ptr[Unit]`       | `null` is `Ptr[Unit]` always  |

**Invariant I-4.05**: `null` has type `Ptr[Unit]`. There is no null-polymorphism; `null` cannot be assigned to `Maybe[T]`. Code that assigns `null` to a pointer typed variable must expect `Ptr[Unit]` or a compatible `Ptr[T]`.

---

## 4.3 Identifier Resolution (`expr.cpp:411`)

`check_ident` implements name resolution in a fixed priority order:

1. **Current scope chain** (`env_.current_scope()->lookup(ident.name)`, line 415). Returns `sym->type` directly if found.
2. **Function table** (`env_.lookup_func`, line 419). Returns a `FuncType` constructed from the function's parameter and return types.
3. **Enum constructor** — iterates `env_.all_enums()` looking for a variant matching the name (line 424). A zero-payload variant returns the enum's `NamedType`; a variant with payload returns a `FuncType` from the payload types to the enum type.
4. **Local struct/enum type names** — checks `env_.all_structs()` (line 443) then `env_.all_enums()` (line 449), returning `NamedType{name, "", {}}`.
5. **Imported symbols** — calls `env_.resolve_imported_symbol` (line 457), then looks up the resolved module for a struct or enum with that name.
6. **Enum constructors from imported modules** — iterates `env_.all_imports()`, loads each imported module, searches all enum variants (line 481).
7. **Imported constants** — calls `resolve_imported_symbol` again (note: duplicated at line 506, same call as step 5) and looks up the constant in the resolved module. Type resolution for constants is done by string comparison against known type names (lines 520–615). Tuple constant types are parsed from a string representation like `"(U8, U8, U8)"` using ad-hoc string splitting.
8. **Const generic parameters** — checks `current_const_params_` map (line 620).
9. **Error** — emits "Undefined variable: X" with fuzzy name suggestions (line 627). Returns `make_unit()`.

**Invariant I-4.06**: Variable reads are tracked via `read_vars_.insert(ident.name)` at line 413 regardless of lookup success. This tracking is used for unused-variable detection (S014).

**Invariant I-4.07**: The duplicated `resolve_imported_symbol` call at lines 457 and 506 means that for an imported symbol that resolves to a constant, the symbol table is queried twice. The second query (line 506) can hit a different code path: the first checks structs/enums; the second checks the constants map. There is no bug here, but the resolution order means that if a name matches both an imported struct and an imported constant, the struct wins (it is found in step 5 before reaching step 7).

**Invariant I-4.08**: Constant type resolution from string names (line 520 onward) has a special case: `"Char"` maps to `PrimitiveKind::U32`, not `PrimitiveKind::Char` (line 541). This is the internal representation of char constants as Unicode code points.

---

## 4.4 Type Variable System

The unification system is minimal: there are no compound constraints, no occurs-check, and no constraint propagation.

### `fresh_type_var()` (`env_core.cpp:51`)

Creates a `TypeVar{id, std::nullopt}` where `id` is a monotonically increasing counter (`type_var_counter_`). There is no occurs-check or level tracking.

### `unify(a, b)` (`env_core.cpp:57`)

```cpp
void TypeEnv::unify(TypePtr a, TypePtr b) {
    if (a->is<TypeVar>()) {
        substitutions_[a->as<TypeVar>().id] = b;
    } else if (b->is<TypeVar>()) {
        substitutions_[b->as<TypeVar>().id] = a;
    }
}
```

Unification is **one-sided**: it records a substitution only when one side is a `TypeVar`. If neither side is a `TypeVar`, the call is a no-op — no structural unification is attempted.

**Invariant I-4.09**: `unify` does not perform structural decomposition. Calling `unify(List[T], List[I32])` has no effect because neither argument is a `TypeVar`. Structural matching of generic types must be performed manually by the caller before calling `unify`, or not at all (in which case the type variables remain unresolved).

**Invariant I-4.10**: `unify` has no occurs-check. Calling `unify(T, List[T])` records `substitutions_[T.id] = List[T]`. `resolve` handles this with a cycle-detection set (`visited`) that returns the unresolved `TypeVar` on cycle (env_core.cpp:81).

### `resolve(type)` (`env_core.cpp:65`)

Follows substitution chains via `resolve_impl`, which tracks visited IDs to break cycles. Only follows `TypeVar` links — does not recursively resolve type arguments inside `NamedType` or `FuncType`. If `T -> List[U]` and `U -> I32`, `resolve(T)` returns `List[U]`, not `List[I32]`.

**Invariant I-4.11**: `resolve` is shallow — it only follows the top-level TypeVar chain. Inner type arguments of resolved types are NOT transitively resolved. Callers that need fully resolved types must call `resolve` on each component separately.

### Where Type Variables Are Created

Type variables are created in three places:
1. `check_closure` (types_checker.cpp:278): for each untyped closure parameter — `env_.fresh_type_var()`.
2. `check_array` (types_checker.cpp:66): for the element type of empty array literals.
3. `resolve.cpp:121`: when resolving an unknown type name to a fresh variable.

---

## 4.5 Closure Type Checking (`types_checker.cpp:262`)

```cpp
auto TypeChecker::check_closure(const parser::ClosureExpr& closure) -> TypePtr
```

### Algorithm

1. Save `parent_scope`. Push a new empty scope (`temp_scope`).
2. **Capture analysis**: call `collect_captures_from_expr(*closure.body, temp_scope, parent_scope, captures)`. This identifies which outer variables are referenced.
3. **Define parameters** in `closure_scope` (the same `temp_scope`). If a parameter has no declared type, call `env_.fresh_type_var()`.
4. Store capture names in `closure.captured_vars` (AST mutation for codegen).
5. Save `current_return_type_`. If the closure has a declared return type, resolve it and set `current_return_type_`.
6. Call `check_expr(*closure.body)` to type-check the body. The result is the body type.
7. `return_type` = declared return type if present, else body type.
8. Restore `current_return_type_`.
9. Pop scope.
10. Write `inferred_param_types` and `inferred_return_type` into the AST node (via `shared_ptr<void>` type erasure). These are read by the HIR builder.
11. Return `make_closure(param_types, return_type, captures)`.

**Invariant I-4.12**: Closure inferred types are written to the mutable AST node fields `closure.inferred_param_types` and `closure.inferred_return_type` (types_checker.cpp:310–314). These fields use `shared_ptr<void>` for type erasure to avoid a circular dependency between parser and types headers. The HIR builder casts these back to `TypePtr` via `static_pointer_cast`. If `check_closure` is not called (e.g., closure is inside an expression that does not reach `check_expr`), these fields remain null.

**Invariant I-4.13**: Closures passed as arguments to functions with `FuncType` parameters undergo a second unification pass (expr.cpp:699–719, expr_call.cpp:699–719). After the closure's type is checked independently, the checker calls `env_.unify(clos.params[j], expected_func.params[j])` for each parameter and `env_.unify(clos.return_type, expected_func.return_type)`. The updated inferred types are then written back to the AST node (lines 711–718). This is the primary mechanism for inferring closure parameter types from context — it relies on `unify` resolving the type variable created for the untyped parameter.

**Invariant I-4.14**: The `check_expr` overload with expected type also does a third write to `inferred_param_types`/`inferred_return_type` after unification (expr.cpp:282–289). This can overwrite the values written by `check_closure`. The final values stored are post-unification.

### The `ClosureType` Collapse Issue (Implementation Note)

**Known bug tracked in `phase0h_closure-type-preservation`.**

When a closure is stored in a struct field (e.g., `RepeatWith[F]` where `F` is the closure type), the type checker records the struct's `type_args[0]` as the closure's type at the time of struct construction. However, when a generic struct type instantiation is later reconstructed from a mangled LLVM IR name (e.g., `RepeatWith__Fn`), the round-trip through the mangle/demangle path in `generic_instantiate.cpp:268–277` produces `NamedType{"Fn", "", {}}` — a degenerate placeholder — instead of the original `ClosureType`.

The specific location: `compiler/src/codegen/llvm/core/generic_instantiate.cpp:273–276`:
```cpp
if (s == "Fn") {
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{"Fn", "", {}};
    return t;
}
```

And the comment at `generic_instantiate.cpp:269`: "We can't recover the actual FuncType signature from just 'Fn', so we use NamedType as a marker."

This collapse occurs **during codegen**, not during type checking. The type checker itself preserves `ClosureType` correctly. The bug manifests when `method_impl.cpp` reads `type_subs[F] = named.type_args[0]` and gets `NamedType("Fn")` instead of the original `ClosureType`, causing the LLVM function signature to use the unsubstituted `Maybe__T` instead of `Maybe__I32`.

**Self-hosting implication**: The TML-written type checker is not responsible for this bug. The type checker's invariant that `ClosureType` is emitted from `check_closure` is correct. The bug lives in codegen's type-name deserialization layer.

---

## 4.6 Function Call Resolution (`expr_call.cpp:147`)

`check_call` is the primary function call checker, accepting an optional `expected_type` for return-type-driven inference.

### Resolution Order

The following priority sequence is applied (each step only reached if previous steps fail to match):

**Step 1 — Polymorphic builtins** (`expr_call.cpp:149`):
`print` and `println` accept any argument types. Each argument is type-checked by calling `check_expr(*arg)` (the return type is discarded). Return type is `Unit`.

**Step 2 — Compiler intrinsics via PathExpr** (`expr_call.cpp:160`):
When the callee is a `PathExpr` with a single segment matching one of the intrinsic names:
- `type_id`, `size_of`, `align_of`, `field_count`, `variant_count`, `field_type_id`, `field_offset`, `method_count`, `interface_method_count`, `impl_count` → return `I64`
- `type_name`, `field_name`, `base_class`, `method_name`, `interface_method_name`, `impl_name` → return `Str`
- `is_abstract`, `is_sealed`, `is_virtual`, `is_override`, `is_static_method` → return `Bool`

These are recognized by name matching only, with no arity check. The type parameters (e.g., `[I32]` in `type_id[I32]()`) are accepted but not validated.

**Step 3 — Generic free function via PathExpr** (`expr_call.cpp:183`):
If the single-segment PathExpr matches a function in `env_.lookup_func(name)`, build substitutions from explicit type arguments in `path.generics`, then type-check arguments with expected types, then infer remaining substitutions from argument types via `extract_type_params`. Return `substitute_type(func->return_type, substitutions)`.

**Note**: Steps 2 and 3 share the same `path.segments.size() == 1` guard. Step 3 executes only after steps 2 fail to match, meaning if a user names a function `type_id`, it will be shadowed by the intrinsic.

**Step 4 — Generic free function via IdentExpr** (`expr_call.cpp:264`):
When the callee is an `IdentExpr`:
1. Type-check all arguments to collect `arg_types`.
2. Call `env_.lookup_func_overload(name, arg_types)` for overload resolution.
3. Fall back to `env_.lookup_func(name)` if no exact overload match.
4. If found and generic, infer substitutions from arguments via `extract_type_params`.
5. Check where-clause constraints (behavior bounds) with error `T026`.
6. Check lifetime bounds with error `T054`.

**Step 5 — Enum constructor via IdentExpr** (`expr_call.cpp:374`):
Iterates all enums in `env_.all_enums()`, looking for a variant whose name matches the callee ident. Checks arity with error `T034`. Returns `NamedType{enum_name, "", {}}`.

**Invariant I-4.15**: Enum constructors returned by `check_call` always produce a bare `NamedType{enum_name, "", {}}` — no generic type arguments are propagated. This is correct for non-generic enums. For generic enum variants (e.g., `Just(42)` for `Maybe[I64]`), the type arguments of the result are **empty**. The HIR builder and downstream must handle this; the type checker does not fill in generic type arguments for enum constructors called as functions.

**Step 6 — Static methods on primitive types via PathExpr (2-segment)** (`expr_call.cpp:400`):
When the callee is `Type::method` for a known primitive type. Only `default` and `from` are handled explicitly. `default` returns the named primitive. `from` type-checks the argument (discarding result) and returns the target type.

**Step 7 — Class constructor and static methods** (`expr_call.cpp:481`):
`ClassName::new` returns `ClassType{name, "", {}}`. Other static methods return the method's declared return type, with substitution of explicit type arguments.

**Step 8 — Local/imported static functions (`Type::method`)** (`expr_call.cpp:522`):
Looks up `type_name + "::" + method` in the local function table, then in the imported module for the type. Performs full generic substitution: explicit type args first, then argument inference, then expected-return-type inference.

**Step 9 — Fallback callee type check** (`expr_call.cpp:679`):
If none of the above matched, type-check the callee expression itself. If it produces a `FuncType`, apply it. If it produces a `ClosureType`, apply it. If the callee produces neither, return `Unit`.

**Invariant I-4.16**: `check_call` checks arguments **twice** in the generic function path (step 4): once in the overload-resolution pre-check (line 270: `arg_types.push_back(check_expr(*arg))`), and once again in the generic substitution loop (line 286: `check_expr(*call.args[i], func->params[i])`). This means each argument expression is type-checked twice. The second check passes the expected parameter type for literal coercion. Side effects (e.g., recording variable reads) execute twice.

**Invariant I-4.17**: Where-clause constraint checking (`expr_call.cpp:308`) verifies that each type parameter substitution satisfies declared behavior bounds. For parameterized bounds (e.g., `T: Iterator[Item=I32]`), only the base behavior name is checked — the type argument is NOT verified (comment at line 346: "Note: Full parameterized bound checking... For now, we just verify the base behavior is implemented").

---

## 4.7 Generic Type Substitution

`extract_type_params` (defined in both `expr_call.cpp:31` and duplicated in `expr_call_method.cpp:77`) extracts type variable bindings by structural matching of a parameter type against an argument type.

### Structural Matching Rules (`expr_call.cpp:31`)

The function handles the following structural forms:

| Param type form             | Condition for match              | Action                          |
|-----------------------------|----------------------------------|---------------------------------|
| `NamedType{name, "", {}}`   | `name` is in `type_params`      | `substitutions[name] = arg`     |
| `NamedType{A, _, [T...]}`   | `arg` is `NamedType{A, _, [U..]}` | Recurse on each type arg pair  |
| `GenericType{name}`         | `name` is in `type_params`      | `substitutions[name] = arg`     |
| `RefType{inner}`            | `arg` is `RefType{inner}`       | Recurse on inner types          |
| `TupleType{[...]}`          | `arg` is same-arity `TupleType` | Recurse pairwise on elements    |
| `ArrayType{elem}`           | `arg` is `ArrayType{elem}`      | Recurse on element types        |
| `SliceType{elem}`           | `arg` is `SliceType{elem}`      | Recurse on element types        |
| `FuncType{params, ret}`     | `arg` is `FuncType`             | Recurse pairwise + return type  |
| `FuncType{params, ret}`     | `arg` is `ClosureType`          | Same: closures match func params|

**Invariant I-4.18**: `extract_type_params` does not modify existing substitutions. If `T` was already mapped to `I32` and a second call tries to map `T` to `I64`, the second call will overwrite the first (via `substitutions[tp] = arg_type`). There is no conflict detection.

**Invariant I-4.19**: Unresolved type variables (`TypeVar`) appearing in argument types are **not** resolved before being stored as substitutions. If an argument has type `TypeVar(5)` and the parameter is `T`, then `substitutions["T"] = TypeVar(5)`. Downstream code calling `substitute_type` will embed the unresolved variable. `env_.resolve` must be called to follow the chain.

### `substitute_type`

The `substitute_type` function (referenced but defined elsewhere in the checker infrastructure) replaces `NamedType{name, "", {}}` entries in `substitutions` within a type tree. It is called to compute concrete return types from generic signatures.

---

## 4.8 Method Dispatch Algorithm (`expr_call_method.cpp:194`)

`check_method_call` implements a 12-step dispatch algorithm. The steps are:

### Step 1 — Optional chaining (`?.`) (`expr_call_method.cpp:202`)

When `call.optional_chain == true`:
1. Type-check `call.receiver` to get the receiver type.
2. Unwrap any `RefType` wrapper.
3. Verify the receiver is `Maybe[T]`. If not: error T090, return `Unit`.
4. Extract `inner_type = type_args[0]`.
5. Delegate to `check_method_call_builtin_types(call, inner_type, method)`.
6. If found: wrap the result in `Maybe[ReturnType]`, flattening if result is already `Maybe[V]`.
7. If not found: look up `inner_type_name::method` in local function table and all module caches.
8. Wrap the result in `Maybe[...]`, flattening if result is already `Maybe`.
9. If still not found: error, return `Unit`.

**Invariant I-4.20**: Optional chaining flattens `Maybe[Maybe[V]]` to `Maybe[V]` automatically. The flattening check is `method_ret->is<NamedType>() && method_ret->as<NamedType>().name == "Maybe"` (expr_call_method.cpp:229). This applies to ALL method names, including methods that happen to return `Maybe` for reasons unrelated to optional chaining (e.g., a method named `last()` that returns `Maybe[T]`).

### Step 2 — Static methods on primitive type names (`expr_call_method.cpp:337`)

When the receiver is an `IdentExpr` matching a known primitive type name. Only `default` is handled explicitly (returns the primitive type itself). Other methods on primitive type names fall through to later steps.

### Step 3 — Static methods on class types (`expr_call_method.cpp:394`)

When the receiver is an `IdentExpr` matching a class name, look for a static method. Apply explicit type arguments if provided.

### Step 4 — Type-check receiver (`expr_call_method.cpp:422`)

```cpp
auto receiver_type = check_expr(*call.receiver);
```

This is the first point where the receiver expression is actually type-checked. Steps 2 and 3 short-circuit this for static calls.

### Step 5 — Type alias expansion (`expr_call_method.cpp:427`)

If the receiver resolves to a `NamedType` that is a type alias, expand it before method lookup. Searches local aliases, then all loaded modules, then `GlobalModuleCache`. Generic type alias parameters are substituted.

### Step 6 — Pointer type methods (`expr_call_method.cpp:485`)

`Ptr[T]` has exactly four hardcoded methods: `read()`, `write(value)`, `is_null()`, `offset(count)`. Any other method name on a `Ptr[T]` emits error `T084` and returns `Unit`.

### Step 7 — NamedType impl method lookup (`expr_call_method.cpp:539`)

For `NamedType` receivers (after unwrapping `RefType`):

a. **Discriminated key lookup** (`expr_call_method.cpp:626`): Constructs `"Type[discriminator]::method"` where the discriminator is the first type arg's name (for `NamedType` args) or `"ref"`/`"mut_ref"` (for `RefType` args). This handles specialized impls like `impl[T] Pin[ref T]` vs `impl[T] Pin[Heap[T]]` defining the same method name.

b. **Direct qualified lookup** (`expr_call_method.cpp:655`): `env_.lookup_func(named.name + "::" + call.method)`. If found and generic, builds substitution map via `build_receiver_subs`:
   - If `impl_self_type_args` contains specialized patterns (RefType, or NamedType with nested type args), uses pattern-based extraction via `extract_type_params`.
   - Otherwise, positional mapping: `subs[type_params[i]] = named.type_args[i]`.

c. **Module-specific lookup** (`expr_call_method.cpp:701`): If the receiver type has a `module_path`, look in that module's function table.

d. **Import-resolved lookup** (`expr_call_method.cpp:711`): Resolve the type name through `env_.resolve_imported_symbol`, look in the resolved module.

e. **All-modules fallback** (`expr_call_method.cpp:751`): Iterates all loaded modules, trying discriminated key then qualified key. Also searches `GlobalModuleCache`.

f. **Class instance methods** (`expr_call_method.cpp:782`): Walks the class inheritance chain looking for the method.

### Step 8 — ClassType method dispatch (`expr_call_method.cpp:810`)

For `ClassType` receivers, walks the inheritance chain with visibility checks. Emits error `T078` if not found.

### Step 9 — Dyn dispatch (`expr_call_method.cpp:844`)

For `DynBehaviorType` receivers (after `RefType` unwrapping): looks up the behavior definition, finds the method, builds substitutions from `dyn.type_args` against `behavior_def.type_params`, returns the substituted return type.

### Step 10 — Generic type parameter with where-clause bound (`expr_call_method.cpp:879`)

For receivers that are `NamedType` or `GenericType` matching a type parameter name in `current_where_constraints_`:
- Searches parameterized bounds for a behavior providing this method.
- Falls back to simple (non-parameterized) bounds.
- For `Self`/`This` references in behavior method return types, substitutes with the type parameter.

**Invariant I-4.21**: The where-clause constraint lookup at step 10 substitutes `This` and `Self` with the concrete type parameter name as a `NamedType` (expr_call_method.cpp:937–940). This means `This::Item` in a behavior's return type becomes `I::Item` (where `I` is the type parameter name), NOT the resolved item type. Downstream code (HIR builder, MIR builder) must resolve `I::Item` further through the behavior impl system.

### Step 11 — Primitive type methods (`expr_call_method.cpp:951`)

For `PrimitiveType` receivers (after `RefType` unwrapping):

Hardcoded table of methods organized by category:
- Arithmetic (`add`, `sub`, `mul`, `div`, `neg`) → return `receiver_type`
- Integer `rem` → return `receiver_type`
- Comparison (`eq`, `ne`, `lt`, `le`, `gt`, `ge`) → return `Bool`
- `cmp` → return `NamedType{"Ordering", "", {}}`
- `max`, `min`, `clamp` → return `receiver_type`
- `partial_cmp` → return `Maybe[Ordering]`
- Bitwise ops → return `receiver_type`
- `duplicate`, `to_owned` → return `receiver_type`
- `to_string`, `debug_string` → return `Str`
- Format methods (`fmt_binary`, etc.) → return `Str`
- `checked_*` → return `Maybe[Self]`
- `saturating_*`, `wrapping_*` → return `receiver_type`
- `borrow` → return `ref Self`
- `borrow_mut` → return `mut ref Self`
- `hash` → return `I64`
- `is_zero`, `is_one` → return `Bool`

After the hardcoded table: **dynamic lookup** (expr_call_method.cpp:1092):
```cpp
std::string qualified = type_name + "::" + call.method;
auto func = env_.lookup_func(qualified);
if (func) { return func->return_type; }
```
This handles Str methods (`len`, `char_at`, etc.) and any impl block defined in `.tml` files.

**Invariant I-4.22**: For primitive type methods found via dynamic lookup (line 1094), the return type is used **without substitution**. The function signature's type parameters (if any) are not resolved against the actual argument types. This is acceptable for primitive methods that don't have generic signatures, but would produce incorrect types for hypothetical generic primitive methods.

### Step 12 — Builtin type methods (`expr_call_method.cpp:1102`)

Delegates to `check_method_call_builtin_types`, which handles `Ordering`, `Maybe[T]`, `Outcome[T,E]`, `List[T]`, `ArrayType`, `SliceType`, closures/functions, and `TupleType`.

### Step 13 — Function-field fallback (`expr_call_method.cpp:1106`)

If the method name matches a struct field of `FuncType`, treat it as a function call through the field. This handles vtable-style dispatch.

### Step 14 — Behavior impl lookup (`expr_call_method.cpp:1143`)

Last-resort: looks up which behaviors the receiver type implements (`env_.get_behavior_impls`), searches all loaded modules and `GlobalModuleCache` for behavior_impls entries. For each behavior that implements the method: builds `Self`/`This` substitution, substitutes behavior type params from receiver type args, resolves associated types (currently only `Item` from `type_args[0]`).

### Step 15 — Pin dispatch (`expr_call_method.cpp:1208`)

Special case: if receiver is `Pin[ref T]` or `Pin[mut ref T]` and no method was found above, unwrap to `T` and attempt method lookup on `T`. This enables `Future::poll` with `Pin[mut ref This]` receiver type.

### Return on No Match

If all 15 steps fail, `check_method_call` returns `make_unit()` silently (line 1360). No error is emitted for unknown method calls.

**Invariant I-4.23**: Unknown method calls on any type return `Unit` without emitting a diagnostic. This means that typos in method names propagate silently through the type checker and manifest later as codegen failures or runtime crashes. The type checker does NOT reject code with unknown method calls.

---

## 4.9 Generic Method Instantiation

For a method call `receiver.method(args)` where the receiver has type `SomeName[I32, Str]`:

1. Look up `SomeName::method` in the function table. The stored `FuncSig` has `type_params = ["T", "E"]` (from the impl declaration).
2. Build `subs = {"T" -> I32, "E" -> Str}` via `build_receiver_subs`.
3. For each argument: call `check_expr(*args[i], substitute_type(params[i+1], subs))` to pass the expected type for literal coercion. Then call `extract_type_params(params[i+1], arg_type, type_params, subs)` to infer any remaining substitutions.
4. Return `substitute_type(func->return_type, subs)`.

The concrete example `Maybe[T].map()`:
- `named.name = "Maybe"`, `named.type_args = [I32]`
- `func->type_params = ["T"]`
- `build_receiver_subs` produces `subs = {"T" -> I32}`
- `map`'s declared return type is `Maybe[T]` (from the function table)
- `substitute_type(Maybe[T], {"T"->I32})` = `Maybe[I32]`
- Returns `Maybe[I32]`

**Invariant I-4.24**: The function table stores `FuncSig` entries keyed by `"TypeName::method_name"`. For a generic impl `impl[T] Maybe[T]`, each method is stored as `"Maybe::map"` with `type_params = ["T"]` and `impl_self_type_args = [NamedType{"T","",{}}]`. The positional mapping in `build_receiver_subs` maps `type_params[i]` → `named.type_args[i]`. There is no explicit monomorphization step at type-check time — the substitution is computed on demand per call site and never cached.

**Invariant I-4.25**: The `FuncSig::impl_self_type_args` field drives specialized impl dispatch. When an impl is declared as `impl[T] Pin[ref T]`, the stored `impl_self_type_args = [RefType{T}]`. The `has_specialized_patterns` check (expr_call_method.cpp:557) detects this and uses pattern-based extraction instead of positional mapping. The heuristic: any `RefType` in `impl_self_type_args` sets `has_specialized_patterns = true`. Any `NamedType` with nested type args that contains at least one type parameter also sets it.

---

## 4.10 Builtin Type Methods (`expr_call_method_types.cpp`)

`check_method_call_builtin_types` returns `std::optional<TypePtr>`: `std::nullopt` signals "not handled" so the caller can continue.

### Maybe[T] Methods

All methods recognize the receiver via `named.name == "Maybe" && !named.type_args.empty()` with `inner_type = named.type_args[0]`.

Key return types:
- `map`, `and_then`, `or_else`, `filter`, `alt`, `xor/one_of`, `flatten` (when inner is not `Maybe`), `duplicate`, `inspect`, `take`, `replace` → `Maybe[T]` (same as `obj_type`)
- `map(f)` returns `obj_type` — this is an **approximation**. The actual return type should be `Maybe[U]` where `U` is the closure's return type. The current implementation does NOT infer `U` from the closure argument (expr_call_method_types.cpp:67–69).
- `and_then(f)` similarly returns `obj_type` instead of inferring `U` from the closure.
- `ok_or(err)` → `Outcome[T, typeof(err)]` — actually evaluates the argument type.
- `ok_or_else(f)` → `Outcome[T, E]` where `E` is inferred from closure return type.
- `zip(other)` → `Maybe[(T, U)]` — evaluates `other` type.
- `zip_with(other, f)` → `Maybe[V]` where `V` is closure return type.
- `flatten` on `Maybe[Maybe[T]]` → `Maybe[T]` (inner type, not `obj_type`).
- `transpose` on `Maybe[Outcome[T,E]]` → `Outcome[Maybe[T], E]`.
- `iter` → `MaybeIter[T]`.

**Invariant I-4.26**: `Maybe[T].map(f)` returns `Maybe[T]` (the original type), NOT `Maybe[U]`. This is intentional simplification — correctly inferring `U` would require closure return type inference at the call site. As a result, code that chains `.map()` operations loses type information: `Just(42).map(do(x) x.to_string())` returns `Maybe[I32]` not `Maybe[Str]`. The HIR builder and codegen must not assume that `map`'s return type argument is the closure's return type.

### Outcome[T,E] Methods

Key patterns:
- `map`, `map_err`, `and_then`, `or_else`, `alt`, `duplicate`, `inspect`, `inspect_err` → `obj_type` (same Outcome type, no inference of new type arguments).
- `ok()` → `Maybe[T]`, `err()` → `Maybe[E]`.
- `flatten` on `Outcome[Outcome[T,E],E]` → `ok_type` (the inner Outcome).
- `transpose` on `Outcome[Maybe[T],E]` → `Maybe[Outcome[T,E]]`.
- `iter` → `OutcomeIter[T]`.

### List[T] Methods

Returns `ListIter[T]` for `iter`/`into_iter`. Returns `Maybe[ref T]` for `get`, `first`, `last`. Returns `T` (not `Maybe[T]`) for `remove`.

### Array and Slice Methods

`ArrayType` methods: `iter`/`into_iter` → `ArrayIter[T]` (one type arg only, not `ArrayIter[T, N]` — the size is dropped). `as_ref`/`as_slice` → `Slice[T]`. `as_mut_slice` → `NamedType{"MutSlice", "", {T}}`.

`SliceType` methods: `iter`/`into_iter` → `SliceIter[T]`.

### Closure/Function `call` Methods

`ClosureType.call()` → `closure.return_type`. `FuncType.call()` → `func.return_type`. This handles the `Fn` trait call mechanism.

---

## 4.11 Binary Operator Type Rules (`expr_ops.cpp:37`)

### Arithmetic Operators

The right operand is checked with the left operand's type as expected type for `Add`, `Sub`, `Mul`, `Div`, `Mod` (expr_ops.cpp:44–54). This enables literal coercion: `x * 3` where `x: I32` allows `3` to infer as `I32` rather than `I64`.

**Return types**:
- `Add`, `Sub`, `Mul`: return `left` type.
  - Exception: `ptr + int` returns `left` (pointer type).
  - Exception: `ptr - int` returns `left`; `ptr - ptr` returns `I64`.
- `Div`, `Mod`: return `left` type. Division by zero literal at compile time emits error `T052`.
- Bitwise ops (`BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr`): return `left` type.

### Comparison and Logical Operators

- `Lt`, `Le`, `Gt`, `Ge`, `Eq`, `Ne`: return `Bool`. Both operands are type-checked and compatibility is verified via `types_compatible`.
- `And`, `Or`: return `Bool`. Operand types are NOT checked (no enforcement that operands are `Bool`).

**Invariant I-4.27**: Logical `and`/`or` operators do NOT verify that operands are `Bool` type (expr_ops.cpp:136–138). The type checker accepts `1 and 2` as valid, returning `Bool`. This is a known permissiveness.

### Assignment Operators

- `Assign`: returns `Unit`. Checks mutability via `check_assignable`. For `mut ref T` LHS, checks inner type compatibility.
- Compound assignment (`+=`, `-=`, etc.): return `Unit`. Check mutability. Division/modulo compound assignments check for literal zero.

### `check_binary_types` helper

Calls `types_compatible(resolved_left, resolved_right)`. If incompatible, emits an error message like "Binary operator '+' requires matching types, found A and B".

---

## 4.12 Unary Operators (`expr_ops.cpp:186`)

- `Neg`: return operand type.
- `Not`: return `Bool`.
- `BitNot`: return operand type.
- `Ref` (`&`):
  - In `lowlevel` blocks (`in_lowlevel_`): returns `Ptr[T]` (raw pointer).
  - Otherwise: returns `ref T`.
- `Deref` (`*`): on `RefType` → inner type; on `PtrType` → inner type; on other types → the type itself (no error).
- `RefMut` (`mut &`): returns `mut ref T`.

**Invariant I-4.28**: Dereferencing a non-pointer, non-reference type (`*x` where `x` is `I32`) does NOT emit an error. It returns the operand type unchanged. The borrow checker, not the type checker, is expected to catch invalid dereferences.

---

## 4.13 Operator Overloading Resolution

TML does not have a formal operator overloading dispatch mechanism in the type checker. Binary operators do not look up behavior methods (e.g., `Add::add`, `Sub::sub`). The THIR lowerer (`compiler/src/thir/thir_lower.cpp`) is responsible for desugaring operators to method calls after type checking completes.

The type checker's role is limited to:
1. Checking that both operands are compatible types.
2. Returning the appropriate result type (always the left operand's type for arithmetic).

**Invariant I-4.29**: Operator overloading for user-defined types is NOT checked at the type-checker stage. `a + b` where `a: MyStruct` does not look up `impl Add for MyStruct`. The type checker will accept `a + b` as long as `types_compatible(left_type, right_type)` is satisfied. Behavior-dispatch for operators happens in the THIR lowerer, not here.

---

## 4.14 Pattern Matching and `when` Expressions

`check_when` (`compiler/src/types/checker/control.cpp`) handles `when` expressions. Each arm's pattern is type-checked against the scrutinee type. This is covered in depth in Section 6 (cross-cutting invariants) but the body-checking rule is:

**Invariant I-4.30**: All `when` arms must produce compatible types. If an arm's body type differs from previous arms, the arms are allowed to diverge (the type checker uses the first arm's type as the `when` expression type and emits no error for type mismatches between arms unless the expression is used in a typed context). **This is an unverified claim — see implementation note below.**

**Implementation note**: The exact behavior of `check_when` with mismatched arm types requires reading `control.cpp`, which is not in scope for Section 4. This invariant should be verified in Section 5.

---

## 4.15 `try` Operator (`types_checker.cpp:319`)

The `!` try operator unwraps `Outcome[T,E]` to `T`, or `Maybe[T]` to `T`. If the expression type is neither, emits no type error but returns the original type unchanged (types_checker.cpp:344).

**Invariant I-4.31**: Using `!` on a non-Outcome, non-Maybe expression produces no compile-time error. The result type is the original expression type. Incorrect `!` usage is a semantic error that manifests at runtime.

---

## 4.16 `await` Expression

`check_await` (defined in `expr_special.cpp`) handles `expr.await`. The general pattern: type-check the inner expression, unwrap the `Future[T]` wrapper, return `T`. The exact implementation is in `expr_special.cpp` (not in scope for Section 4 but referenced here for completeness).

---

## 4.17 Block and Control Flow Return Types

Blocks, if-expressions, and loop expressions all contribute to the type of the enclosing function or closure:

- A block's type is the type of its last expression (or `Unit` if the last statement is not an expression).
- `return expr` terminates with the `Never` type (via `make_never()`), propagating type `Never` upward.
- `break` is handled by `check_break` which returns `Never`.
- `continue` directly returns `make_never()` in the dispatch table (expr.cpp:177).

**Invariant I-4.32**: Both `break` and `continue` have type `Never`. This is consistent with their semantics (they diverge the current control path). Expressions of type `Never` can coerce to any type in contexts where the type checker calls `types_compatible`, since `Never` is a bottom type.

---

## 4.18 Module and Cache Lookup Strategy

Method resolution in steps 7e, 14, and 15 iterates three data stores in order:

1. `env_.get_all_modules()` — modules loaded into the local module registry.
2. `GlobalModuleCache::instance().get_all()` — modules loaded globally (shared across compilation units).

**Invariant I-4.33**: When a method is found in multiple modules (e.g., two different modules both define `Foo::method`), the first match wins. The iteration order over module maps is unspecified (unordered map). This means method dispatch can be non-deterministic if two modules define the same qualified method name for the same type. In practice, this is prevented by the qualification convention (`TypeName::method`), but there is no formal uniqueness guarantee.

**Invariant I-4.34**: The `GlobalModuleCache` is a process-wide singleton accessed by `GlobalModuleCache::instance()`. Methods looked up through it during one compilation unit's type checking may have been populated by a different compilation unit compiled in the same process (e.g., during test suite compilation). This is intentional — it allows cross-module behavior impl resolution — but means that type checking is not fully hermetic per compilation unit.

---

## 4.19 Summary: Invariant Index

| ID     | Topic                                   | File/Line                                  |
|--------|-----------------------------------------|--------------------------------------------|
| I-4.01 | Expected-type propagation scope         | expr.cpp:215–296                           |
| I-4.02 | Integer default type is I64             | expr.cpp:379                               |
| I-4.03 | Overflow checked at type-check time     | expr.cpp:308–322                           |
| I-4.04 | Negative literal to unsigned: error T050| expr.cpp:229–233                           |
| I-4.05 | `null` has type `Ptr[Unit]`             | expr.cpp:405                               |
| I-4.06 | Variable reads tracked via read_vars_   | expr.cpp:413                               |
| I-4.07 | Duplicated resolve_imported_symbol call | expr.cpp:457, 506                          |
| I-4.08 | Char constant maps to U32               | expr.cpp:541                               |
| I-4.09 | unify is one-sided, no structural decomp| env_core.cpp:57–63                         |
| I-4.10 | unify has no occurs-check               | env_core.cpp:57–63                         |
| I-4.11 | resolve is shallow (not transitive)     | env_core.cpp:65–92                         |
| I-4.12 | Closure inferred types in mutable AST   | types_checker.cpp:310–314                  |
| I-4.13 | Closure parameter types inferred via unify | expr.cpp:699–719, expr_call.cpp:699–719 |
| I-4.14 | Third write to inferred types post-unify| expr.cpp:282–289                           |
| I-4.15 | Enum constructor call returns bare NamedType | expr_call.cpp:391–393              |
| I-4.16 | Generic function arguments checked twice| expr_call.cpp:270, 286                     |
| I-4.17 | Parameterized where-clause: base only   | expr_call.cpp:346–350                      |
| I-4.18 | extract_type_params overwrites on conflict | expr_call.cpp:46                        |
| I-4.19 | TypeVar stored unresolved in substitutions | expr_call.cpp:46                        |
| I-4.20 | `?.` flattens Maybe[Maybe[V]] to Maybe[V]| expr_call_method.cpp:229                  |
| I-4.21 | where-clause bound substitutes type param name, not resolved type | expr_call_method.cpp:937–940 |
| I-4.22 | Dynamic primitive method lookup: no substitution | expr_call_method.cpp:1094        |
| I-4.23 | Unknown method calls return Unit silently | expr_call_method.cpp:1360               |
| I-4.24 | No monomorphization cache at type-check time | expr_call_method.cpp:655–679         |
| I-4.25 | impl_self_type_args drives specialized dispatch | expr_call_method.cpp:557–619      |
| I-4.26 | `Maybe[T].map(f)` returns `Maybe[T]` not `Maybe[U]` | expr_call_method_types.cpp:67–69 |
| I-4.27 | `and`/`or` operands not verified to be Bool | expr_ops.cpp:136–138               |
| I-4.28 | Deref of non-pointer/non-ref returns unchanged type | expr_ops.cpp:200+            |
| I-4.29 | Operator overloading not checked in type checker | expr_ops.cpp:37–183            |
| I-4.30 | `when` arm type mismatch behavior (unverified) | control.cpp (Section 5)          |
| I-4.31 | `!` on non-Outcome/Maybe: no error      | types_checker.cpp:344                      |
| I-4.32 | `break`/`continue` have type Never      | expr.cpp:174, 177                          |
| I-4.33 | First-match wins in multi-module method lookup | expr_call_method.cpp:751–778      |
| I-4.34 | GlobalModuleCache is process-wide singleton | expr_call_method.cpp:767             |

---

## 4.20 Surprising Findings

1. **Double type-checking of arguments in generic function calls** (I-4.16): The overload resolution pre-pass at line 270 checks all arguments to collect types, then the generic substitution loop at line 286 checks them again with expected types. This is observable: if an argument has a side effect visible to the type checker (e.g., tracking in `read_vars_`), it will be tracked twice.

2. **Enum constructors lose generic type arguments** (I-4.15): `Just(42)` returns `NamedType{"Maybe", "", {}}` — the `[I32]` argument is simply absent. The HIR builder cannot rely on the TypeEnv for the type argument of enum constructor results; it must infer them separately.

3. **`Maybe[T].map(f)` loses type argument** (I-4.26): This is the most consequential type loss for chained operations. Code like `items.iter().map(do(x) x + 1)` where the iterator produces `Maybe[I32]` elements: each `.map()` call in a chain preserves the original `Maybe[I32]` type regardless of the closure's declared return type. The HIR builder sees `Maybe[I32]` throughout and must infer the transformation from the closure's own signature.

4. **Unification is essentially a one-shot substitution** (I-4.09–I-4.11): The system created by `fresh_type_var()` + `unify()` is NOT the HM algorithm. There is no constraint propagation, no generalization step, and no principal type computation. It is used only in two specific cases: (a) inferring closure parameter types from the function signature they are passed to, and (b) `check_expr` with expected type for closures. Outside these cases, type inference is purely bottom-up with no type variables.

5. **`GlobalModuleCache` as a cross-unit oracle** (I-4.34): Type checking of file A can resolve methods defined in file B that was compiled earlier in the same process. This is the mechanism that makes behavior impls from separate modules work. But it also means type checking is not reproducible in isolation: if file B is compiled first, file A gets different results than if file A is compiled alone.

6. **`ClosureType` collapse at codegen boundary** (section 4.5, Implementation Note): The type checker correctly preserves `ClosureType`. The information loss occurs when codegen re-parses mangled names and can only recover `NamedType{"Fn", "", {}}`. The self-hosting type checker must preserve `ClosureType` correctly; the bug is in codegen, not in the type checker contract.

7. **Unknown method calls are silent** (I-4.23): `check_method_call` returns `Unit` without an error if no method is found. This means a TML-written type checker that implements this contract correctly must also silently return `Unit` for unknown methods. A stricter implementation that emits errors for unknown methods would break programs that currently compile successfully by accidentally calling methods the type checker accepts.
