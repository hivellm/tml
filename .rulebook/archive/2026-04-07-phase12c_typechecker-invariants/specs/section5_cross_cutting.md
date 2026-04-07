# Section 5 — Cross-Cutting Invariants

**Task**: phase12c_typechecker-invariants  
**Phase**: 5 of 6  
**Status**: Complete  
**Sources**: `compiler/src/types/checker/expr_ops.cpp`, `expr_special.cpp`, `stmt.cpp`,
`control.cpp`, `const_eval.cpp`, `helpers.cpp`, `resolve.cpp`, `types_checker.cpp`,
`builtins/register.cpp`, `builtins/types.cpp`, `builtins/io.cpp`, `builtins/mem.cpp`,
`builtins/math.cpp`, `builtins/atomic.cpp`, `builtins/sync.cpp`, `builtins/async.cpp`,
`builtins/string.cpp` (removed), `builtins/time.cpp` (removed),
`env_core.cpp`, `env_definitions.cpp`, `env_scope.cpp`, `env_module_support.cpp`

---

## Overview

This section documents the cross-cutting invariants that tie together all four earlier phases
(Type Registration, Module Resolution, Impl Processing, Body Checking). Where phases 1-4 each
described a logical stage, this section documents the global rules that hold across every stage,
the state fields that accumulate progressively, and the error-recovery behaviour that downstream
consumers rely on.

Minimum invariant count for this section: 20 concrete invariants with `file:line` citations.
This document contains **32** numbered invariants.

---

## 1. Phase Ordering Dependencies

The type checker runs in four sequential phases. Each phase may read from `TypeEnv` fields that
were populated by an earlier phase. No phase may read a field that has not yet been populated.

### 1.1 Canonical Phase Sequence

```
Phase 0: TypeEnv construction (builtins)
Phase 1: Type registration    (structs, enums, behaviors, type aliases)
Phase 2: Module resolution    (imports, pub use re-exports)
Phase 3: Impl processing      (behavior impls, method registration)
Phase 4: Body checking        (expression and statement type inference)
```

### 1.2 What Is Readable at Each Phase

| Field | Readable from phase |
|---|---|
| `builtins_` | 0 (set during construction) |
| `structs_`, `enums_`, `behaviors_`, `type_aliases_` | 1 |
| `imported_symbols_`, `module_registry_` | 2 |
| `behavior_impls_`, `functions_` (impl methods) | 3 |
| `substitutions_`, `current_scope_` symbols | 4 |

**Invariant CC-01** (`env_core.cpp:32-35`): `TypeEnv` construction always calls `init_builtins()`
before any user code is processed. The `builtins_` map and the primitive enums (`Ordering`,
`Maybe`, `Outcome`, `Poll`) and behaviors (`Future`, `Drop`, `Send`, `Sync`) are populated in
the constructor. They are present unconditionally for every compilation unit.

**Invariant CC-02** (`builtins/register.cpp:38-52`): The builtin initializers are called in a
fixed order: `init_builtin_types` → `init_builtin_io` → `init_builtin_mem` → `init_builtin_atomic`
→ `init_builtin_sync` → `init_builtin_math` → `init_builtin_async`. A TML implementation must
preserve this exact order if any initializer depends on a type registered by a prior initializer.
In practice `init_builtin_async` depends on `Poll[T]` being present, which is registered by
`init_builtin_types`.

**Invariant CC-03** (`env_core.cpp:102-122`): The `snapshot()` operation creates a new `TypeEnv`
that shares all *definition* tables (`structs_`, `enums_`, `behaviors_`, `functions_`,
`behavior_impls_`, `type_aliases_`, `classes_`, `interfaces_`, `class_interfaces_`, `builtins_`)
but gets a fresh local state (`current_scope_`, `substitutions_`, `module_registry_`,
`imported_symbols_`, `import_conflicts_`). A snapshot is therefore safe to use for per-file body
checking once the shared definition tables are fully populated by phases 1-3.

### 1.3 Cross-Phase Read Ordering Violations

**Invariant CC-04**: `resolve_type_path()` (`resolve.cpp:274-277`) reads `builtins_` on every
call. If `init_builtins()` has not run, all named type lookups will fall through to the unknown-
type path and silently return `NamedType{name, "", {}}`. There is no guard against calling
`resolve_type_path` before builtins are initialised. A self-hosted implementation must call
`init_builtins()` (or equivalent) before any `resolve_type` call.

**Invariant CC-05** (`resolve.cpp:279-282`): `resolve_type_path` checks `type_aliases_` *after*
`builtins_`. This means a user-defined type alias whose name shadows a builtin name (e.g.,
`type Bool = ...`) is silently ignored; the builtin takes precedence. This is intentional and
must be preserved.

**Invariant CC-06** (`resolve.cpp:284-299`): `resolve_type_path` checks structs, then classes,
then enums, then behaviors. If a name could refer to both a struct and a behavior (impossible in
valid TML but possible in error recovery), the struct takes priority.

---

## 2. Global State in `TypeEnv` — Field-by-Field Safety

### 2.1 Immutable-after-Phase Fields

These fields are written once during their respective phase and are then read-only:

| Field | Written in phase | Source |
|---|---|---|
| `builtins_` | 0 (constructor) | `builtins/types.cpp`, `register.cpp` |
| `structs_` | 1 | `env_definitions.cpp:27-29` |
| `enums_` | 1 | `env_definitions.cpp:31-33` |
| `behaviors_` | 1 | `env_definitions.cpp:35-37` |
| `type_aliases_`, `type_alias_generics_` | 1 | `env_definitions.cpp:63-68` |
| `classes_`, `interfaces_`, `class_interfaces_` | 1 | `env_definitions.cpp:75-98` |

**Invariant CC-07** (`env_definitions.cpp:27-37`): `define_struct`, `define_enum`, and
`define_behavior` perform a simple map insertion by name. The last call with a given name wins.
There is no de-duplication guard: if the same struct or enum is registered twice (e.g., via two
separate module loads), the second registration silently overwrites the first. A self-hosted
checker must either (a) replicate this last-wins behaviour, or (b) add a duplicate-name error.

**Invariant CC-08** (`env_definitions.cpp:39-61`): `define_func` *appends* to a vector of
overloads rather than replacing. This is the function overloading mechanism. Each unique
`(name, params)` pair is a separate overload. The lookup in `env_lookups.cpp` iterates the
vector and picks the first matching overload. A self-hosted checker must preserve the insertion
order because the first match wins.

### 2.2 Accumulating-across-Phase Fields

These fields grow monotonically during compilation:

| Field | Grows during | Source |
|---|---|---|
| `functions_` | Phases 1, 3 | `env_definitions.cpp:39-61` |
| `behavior_impls_` | Phase 3 | `env_definitions.cpp` (register_impl) |
| `imported_symbols_` | Phase 2 | `env_module_support.cpp:244-278` |
| `import_conflicts_` | Phase 2 | `env_module_support.cpp:256-265` |

### 2.3 Per-File Transient Fields

These fields are reset between files (snapshot semantics):

| Field | Reset by | Source |
|---|---|---|
| `current_scope_` | `snapshot()` | `env_core.cpp:114` |
| `type_var_counter_` | `snapshot()` | `env_core.cpp:116` |
| `substitutions_` | `snapshot()` | `env_core.cpp` (default-initialised) |
| `module_registry_` | `snapshot()` | `env_core.cpp:117` |
| `imported_symbols_` | `snapshot()` | `env_core.cpp` (default-initialised) |
| `import_conflicts_` | `snapshot()` | `env_core.cpp` (default-initialised) |
| `current_module_path_` | `snapshot()` | `env_module_support.cpp:223-225` |
| `source_directory_` | `snapshot()` | `env_module_support.cpp:227-229` |

**Invariant CC-09** (`env_core.cpp:102-122`): The snapshot constructor initialises
`type_var_counter_` to 0, not to the source's counter value. This means type variable IDs are
local to each file's body-checking pass. A type variable from file A with ID 5 has no
relationship with a type variable from file B with ID 5. Downstream (HIR lowering, MIR building)
must never transfer a `TypeVar` across file boundaries.

### 2.4 Checker Instance State (Per-Function)

The `TypeChecker` class (`checker.hpp`) maintains per-function state that is reset for every
function body:

| Field | Meaning | Reset per |
|---|---|---|
| `current_return_type_` | Expected return type for current function | Function |
| `current_self_type_` | `This` type in an impl block | Function |
| `current_type_params_` | Generic type params in scope | Function |
| `current_const_params_` | Const generic params in scope | Function |
| `current_associated_types_` | Associated type bindings | Function |
| `in_async_func_` | Whether inside an async function | Function |
| `in_lowlevel_` | Whether inside a `lowlevel` block | Block |
| `loop_depth_` | Nesting depth of loops | Incremented/decremented per loop |
| `returned_in_block_` | Whether a return has been seen | Block (save/restore) |
| `read_vars_` | Variables that have been read | Block |
| `const_values_` | Evaluated const values | Function |

**Invariant CC-10** (`expr_special.cpp:131-153`): `in_lowlevel_` is saved and restored around
`lowlevel` block processing (save to `was_in_lowlevel`, restore after block). Nesting of
`lowlevel` blocks is therefore supported and the inner block inherits the outer's `true` state.
The `UnaryOp::Ref` and `UnaryOp::RefMut` operators produce `PtrType` (not `RefType`) when
`in_lowlevel_` is `true`. Codegen depends on this distinction to emit pointer vs. reference IR.

**Invariant CC-11** (`control.cpp:146-167`): `loop_depth_` is incremented before entering a
`loop` body and decremented after. `break` outside a loop (`loop_depth_ == 0`) produces error
T030. `for` also increments and decrements `loop_depth_`. A TML implementation must maintain
this counter accurately across nested loops.

---

## 3. Error Recovery Behaviour

### 3.1 Error Model

The type checker uses a *continue-on-error* strategy: most error paths push a `TypeError` into
`errors_` and return a fallback type (usually `make_unit()`) rather than aborting. This allows
the checker to report multiple independent errors in a single pass.

**Invariant CC-12** (`resolve.cpp:359-369`): The `error()` overloads always append to `errors_`.
They never throw or call `abort()`. Code that calls `error()` and then returns `make_unit()` is
performing error recovery. The returned `make_unit()` is a *sentinel type* that propagates
through the AST without generating further cascading errors because `types_compatible` returns
`true` for `Unit` in most contexts.

**Invariant CC-13** (`expr_ops.cpp:57-65`): On a binary type mismatch, the left-hand type is
returned after recording the error. This means the result type of a malformed binary expression
is the *left operand type*, not `Unit`. Downstream code that uses the binary expression's result
type as context will use the left operand's type, not an unknown type.

**Invariant CC-14** (`control.cpp:38-52`): `check_if` does *not* return a common supertype of
`then_type` and `else_type`. It returns `then_type` when both branches are present, or `Unit`
when there is no else. This is a simplification: if the two branches have different types,
an error is reported but the result is `then_type`, not `Never` or a unified type. A self-hosted
checker that tries to compute a more precise type here (e.g., `Never` when both branches diverge)
will produce a different `TypeEnv` that may or may not be compatible with the HIR builder's
expectations.

**Invariant CC-15** (`control.cpp:101-143`): `check_when` applies a `Never` coercion rule:
- If the accumulated `result_type` is `Never`, it is overwritten by the next arm's type.
- If an arm type is `Never`, `result_type` is left unchanged.
- If both accumulated type and arm type are non-`Never` and differ, an error is reported but
  `result_type` stays as the first non-`Never` arm type.
This means the result type of a `when` expression is the type of its *first non-diverging arm*.

**Invariant CC-16** (`helpers.cpp:91-228`): `types_compatible` accepts:
- `TypeVar` on either side (unresolved inference variable matches everything)
- Any integer vs any integer (no width check — `I8` is compatible with `I64`)
- Any float vs any float
- `Ptr[Unit]` (null literal) vs any pointer type
- `[T; N]` (array) vs `[T]` (slice) — coercion allowed
- `[T1; N]` vs `[T2; N]` when element types are compatible (recursive check)
- `[T; N]` vs `List[T]` when element types are compatible
- `ClosureType` vs `FuncType` with matching signatures
- `NamedType` vs `ImplBehaviorType` (bidirectional — no actual behavior check)

The last rule is a known approximation. A concrete type passes the `impl Behavior` check without
verifying that the type actually implements the behavior. The real implementation check happens
in the impl processing phase (Phase 3). Body checking trusts Phase 3 has already confirmed
that all used types implement their declared behaviors.

### 3.2 What a Partial `TypeEnv` Looks Like After Errors

After body checking with errors, `TypeEnv` may contain:
1. Variables bound to `Unit` as sentinel types (`bind_pattern` fallback)
2. Functions with `Unit` return types (error recovery in `resolve_type`)
3. Struct fields with `Unit` types (unknown type name resolution fallback)
4. Type variable substitutions that were never resolved (unevaluated inference)

**Invariant CC-17** (`resolve.cpp:354-357`): When `resolve_type_path` encounters an unknown name,
it returns `NamedType{name, "", {}}` without recording an error (unless it was an undefined
variable via `check_path`). This means an unknown *type name* silently becomes a forward reference
that the checker never validates. HIR lowering will encounter this unknown named type and must
either handle it gracefully or abort.

---

## 4. Scope Chain Rules

### 4.1 Scope Structure

Scopes are singly-linked nodes (`env_scope.cpp:31`). Each node holds a symbol map and a pointer
to its parent. The chain terminates at the root scope created by `TypeEnv()` (`env_core.cpp:33`).

**Invariant CC-18** (`env_scope.cpp:37-45`): `Scope::lookup(name)` walks the chain from child to
root. The *first* match wins. Inner scopes shadow outer scopes. There is no "ambiguity" error for
shadowed names; the inner binding always wins silently.

**Invariant CC-19** (`env_scope.cpp:48-54`): `Scope::lookup_local(name)` checks *only* the
current scope node. It is used by closure capture analysis (`types_checker.cpp:363-365`) to
distinguish parameters (local to closure scope) from captured variables (in parent scope).

**Invariant CC-20** (`expr_ops.cpp:67-83`): Mutability checking for assignment (`BinaryOp::Assign`)
looks up the variable in `env_.current_scope()->lookup(ident.name)` and checks
`sym->is_mutable`. If the variable is an immutable `let` binding but its resolved type is
`RefType{is_mut=true}`, assignment *through* the reference is allowed even though the binding
itself is immutable. The rule: immutability of the *binding* and mutability of the *referenced
value* are independent.

**Invariant CC-21** (`expr_ops.cpp:558-613`): `check_block` pushes a new scope at entry and pops
it at exit. The `returned_in_block_` flag is saved before the push and restored after the pop.
This ensures that a `return` inside a nested block does not mark the outer block as having
returned. After the scope is popped, the checker warns about unused variables (S014) by iterating
`env_.current_scope()->symbols()` before the pop.

**Invariant CC-22** (`control.cpp:86-99, 107-143`): `check_if_let` and `check_when` each call
`env_.push_scope()` before `bind_pattern()` and `env_.pop_scope()` after the arm body. Pattern
bindings from one arm do not leak into subsequent arms, and pattern bindings from all arms are not
visible after the `if let` or `when` expression completes.

**Invariant CC-23** (`control.cpp:145-168`): `check_loop` with a `loop_var` declaration calls
`push_scope()` before defining the loop variable and `pop_scope()` after the body. The loop
variable is visible inside the loop body but not after the loop.

**Invariant CC-24** (`control.cpp:170-234`): `check_for` always pushes and pops a scope. The
loop pattern variable (bound by `bind_pattern`) is defined inside this scope and is not visible
after the loop.

---

## 5. Builtin Type Registration

### 5.1 The `builtins_` Map

`builtins_` is a `std::unordered_map<std::string, TypePtr>` that maps builtin type names to their
canonical `TypePtr` values. It is populated by `init_builtin_types` (`builtins/types.cpp:49-65`).

**Invariant CC-25** (`builtins/types.cpp:49-65`): The 16 primitive type names in `builtins_` are:
`I8`, `I16`, `I32`, `I64`, `I128`, `U8`, `U16`, `U32`, `U64`, `U128`, `F32`, `F64`, `Bool`,
`Char`, `Str`, `Unit`. Each maps to a `PrimitiveType` node (or `make_unit()` for `Unit`).
`Never` is *not* in `builtins_` — it is produced at call sites via `make_never()`.

**Invariant CC-26** (`resolve.cpp:274-277`): `resolve_type_path` looks up `builtins_` by the
*last segment* of a multi-segment path. So `core::I32` resolves the same as `I32`. The module
prefix is discarded. This allows imports to bring in fully-qualified names that still resolve to
builtin primitive types.

**Invariant CC-27** (`builtins/types.cpp:67-107`): Four core enums are registered unconditionally
by `init_builtin_types`:
- `Ordering` — 3 unit variants (`Less`, `Equal`, `Greater`)
- `Maybe[T]` — `Just(T)`, `Nothing`
- `Outcome[T, E]` — `Ok(T)`, `Err(E)`
- `Poll[T]` — `Ready(T)`, `Pending`

These are registered via `define_enum` into `enums_`, not into `builtins_`. They are therefore
not found by the `builtins_` lookup but ARE found by `lookup_enum`.

**Invariant CC-28** (`builtins/types.cpp:109-183`): Four behaviors are registered in `behaviors_`:
`Future`, `Drop`, `Send`, `Sync`. `Future` has an associated type `Output` and a method `poll`.
`Drop` has a method `drop`. `Send` and `Sync` are marker behaviors (no methods). These are visible
to `lookup_behavior` calls from the body-checking phase.

**Invariant CC-29** (`builtins/types.cpp:187-248`): All 10 integer types, 2 float types, `Bool`,
`Char`, `Str`, and `Unit` are registered as implementing `Send` and `Sync`. The `Ordering` enum
is also registered as `Send` and `Sync`. `Maybe`, `Outcome`, `Poll` are NOT registered for
`Send`/`Sync` at construction time; these are expected to be registered by their user-space
module definitions.

### 5.2 Functions Registered at Construction

`init_builtin_io` registers: `print`, `println`, `panic`, `assert`, plus overloads of `assert_eq`
and `assert_ne` for `I32`, `I64`, `Bool`, `Str`, `F64` (`builtins/io.cpp:27-206`).

`init_builtin_mem` registers: `mem_alloc`, `mem_alloc_zeroed`, `mem_realloc`, `mem_free`,
`mem_copy`, `mem_move`, `mem_set`, `mem_zero`, `mem_compare`, `mem_eq`, `alloc` (two overloads),
`dealloc`, `read_i32`, `write_i32`, `ptr_offset`, `ptr_read[T]`, `ptr_write[T]`,
`ptr_read_volatile[T]`, `ptr_write_volatile[T]`, `ptr_read_unaligned[T]`,
`ptr_write_unaligned[T]`, `memcpy`, `memmove`, `memset`, `copy_nonoverlapping`, `copy`,
`write_bytes`, `store_byte` (`builtins/mem.cpp:31-299`).

`init_builtin_atomic` registers: `atomic_load`, `atomic_store`, `atomic_add`, `atomic_sub`,
`atomic_exchange`, `atomic_cas`, fence functions (`builtins/atomic.cpp`).

`init_builtin_sync` registers ONLY: `spin_lock`, `spin_unlock`, `spin_trylock`
(`builtins/sync.cpp:41-65`). Thread, channel, mutex, and waitgroup primitives were removed in
Phase 24 and moved to `@extern("c")` FFI.

`init_builtin_math` registers: `sqrt` (I32 and F64 overloads), `pow` (three overloads), `abs`,
`floor`, `ceil`, `round`, `black_box`, `black_box_i64`, `black_box_f64` (`builtins/math.cpp`).

`init_builtin_async` registers: six overloads of `block_on` for `Poll[I32]`, `Poll[I64]`,
`Poll[F64]`, `Poll[Bool]`, `Poll[Str]`, `Poll[Unit]` (`builtins/async.cpp:32-116`).

**Important**: `init_builtin_string` and `init_builtin_time` were removed. String operations go
through the `try_gen_builtin_string()` codegen path, not through `functions_` entries. Time
functions are now `@extern("c")` FFI declarations in `lib/std/src/time.tml`.

---

## 6. Operator Checking

### 6.1 Binary Operators

**Invariant CC-30** (`expr_ops.cpp:44-55`): For arithmetic operators (`Add`, `Sub`, `Mul`, `Div`,
`Mod`), the left operand type is passed as the *expected type* when checking the right operand.
This allows an unsuffixed integer literal on the right to infer the same type as the left
operand (e.g., `x * 3` where `x: I32` makes `3` infer as `I32`). For all other operators, the
right operand is checked without an expected type.

The type propagation direction is exclusively left-to-right for arithmetic. Comparison and
logical operators do not propagate types.

**Invariant CC-31** (`expr_ops.cpp:86-113`): Pointer arithmetic is special-cased:
- `ptr + integer` → result type is the pointer type (unchanged)
- `ptr - integer` → result type is the pointer type (unchanged)
- `ptr - ptr` → result type is `I64` (pointer difference)

These rules apply when the *left* operand resolves to `PtrType`. They bypass the
`check_binary_types` check and return without an error. A TML self-hosted checker must replicate
these cases before the general type-equality check.

**Invariant CC-32** (`expr_ops.cpp:145-183`): Assignment operators return `Unit`. `BinaryOp::Assign`
has special logic: if the LHS resolves to `mut ref T`, the RHS is checked against `T` (the inner
type), not against the reference type itself. All compound assignment operators (`+=`, `-=`, etc.)
call `check_assignable()` but do not check type compatibility of operands — they only verify the
LHS is mutable.

### 6.2 Unary Operators

(`expr_ops.cpp:186-267`)

| Operator | Normal context | `lowlevel` context |
|---|---|---|
| `Neg` | Returns operand type | Same |
| `Not` | Returns `Bool` | Same |
| `BitNot` | Returns operand type | Same |
| `Ref` (`&`) | Returns `ref T` (reborrow-aware) | Returns `*T` (raw pointer) |
| `RefMut` (`&mut`) | Returns `mut ref T` | Returns `*mut T` (raw mutable pointer) |
| `Deref` (`*`) | Unwraps `RefType` or `PtrType` | Same |
| `Inc`, `Dec` | Returns operand type | Same |

**Invariant CC-33** (`expr_ops.cpp:202-226`): Reborrow semantics: applying `&` to a `ref T` yields
`ref T` (not `ref ref T`). Applying `&mut` to a `mut ref T` yields `mut ref T`. Applying `&mut`
to a `ref T` (non-mut) also yields `mut ref T` — the checker does not reject upgrading a shared
reference to a mutable reference at the type level. (Borrow checking is responsible for enforcing
mutability exclusivity.)

**Invariant CC-34** (`expr_ops.cpp:233-262`): The `Deref` operator recognises a hard-coded list of
smart pointer type names: `Arc`, `Rc`, `Box`, `Heap`, `Shared`, `Sync`, `MutexGuard`,
`RwLockReadGuard`, `RwLockWriteGuard`, `Ref`, `RefMut`, and `Ptr`. For these, `*smart_ptr`
returns the first type argument (`T` in `Arc[T]`). This list is duplicated identically in
`check_field_access` for auto-deref on field access. Any new smart pointer type added to the
standard library must be added to both lists.

---

## 7. Statement Checking

### 7.1 `let` and `var`

**Invariant CC-35** (`stmt.cpp:61-101`): `check_let` follows this logic:
1. If a type annotation is present, resolve it first, then check the initialiser with the
   annotation type as the expected type, then verify compatibility.
2. If no annotation, infer from the initialiser.
3. Always calls `bind_pattern` to add bindings to the current scope.
4. Returns `Unit` (not the inferred type).

**Invariant CC-36** (`stmt.cpp:104-135`): `check_var` is identical to `check_let` except it
calls `env_.current_scope()->define(var.name, var_type, true, ...)` directly (no `bind_pattern`
call). `var` does not support destructuring patterns — it always defines a single mutable
variable by name.

**Invariant CC-37** (`stmt.cpp:137-177`): `check_let_else` (the `let X = e else { block }`
form): the `else` block is type-checked but its result type is discarded. The checker does not
verify that the else block actually diverges (has type `Never`). This check is noted as deferred
to HIR lowering. Pattern bindings are defined in the *parent* scope (visible after the `let-else`
statement), not inside a new scope.

### 7.2 Nested Declarations

(`stmt.cpp:179-217`) Nested `const` declarations inside function bodies ARE type-checked and
bound in the current scope. Nested `func` and other declarations are silently ignored (no type
checking, no registration). This is a known limitation.

### 7.3 Pattern Binding

**Invariant CC-38** (`stmt.cpp:219-443`): `bind_pattern` handles six pattern variants:
- `IdentPattern`: defines the name in current scope, reports `T008` for duplicates
- `TuplePattern`: recursively destructures; reports `T068` for non-tuple, `T036` for arity mismatch
- `WildcardPattern`: does nothing (no binding)
- `EnumPattern`: resolves type aliases before looking up enum; applies generic substitution to
  payload types; reports `T023` for unknown enum, `T024` for unknown variant, `T034` for
  arity/presence mismatches
- `StructPattern`: looks up struct fields by name; reports `T070` for non-struct, `T022` for
  unknown struct, `T072` for unknown field
- `RangePattern`, `ArrayPattern`: range binds nothing; array binds each element and optional
  rest pattern

**Invariant CC-39** (`stmt.cpp:259-317`): `EnumPattern` resolution performs a two-level type
alias lookup:
1. Check `env_.lookup_type_alias(name)` for local aliases.
2. If not found locally, search all loaded modules in `env_.module_registry()`.
This means pattern matching against a type alias defined in an imported module works without
the alias being in the local scope, as long as the module is loaded.

---

## 8. Control Flow Typing

### 8.1 Return Type Propagation

**Invariant CC-40** (`control.cpp:270-290`): `check_return` uses `current_return_type_` to
verify the returned value's type. If `current_return_type_` is null (checking outside a function
— not possible in normal flow), no check is performed. The return expression is checked with
`current_return_type_` as the expected type, allowing array literals in return position to infer
their size from the declared return type.

`check_return` always returns `make_never()`. This is the sole source of `Never` type in normal
control flow (along with `check_break` and `panic` calls).

**Invariant CC-41** (`control.cpp:292-300`): `check_break` returns `make_never()` (it diverges).
It validates `loop_depth_ > 0` before accepting the break.

**Invariant CC-42** (`expr_ops.cpp:559-613`): `check_block` saves `returned_in_block_` before
entering the block and restores it after. Within the block, after each statement, if the
statement was a `ReturnExpr`, `returned_in_block_` is set to `true`. Subsequent statements in
the same block emit warning S016 ("Unreachable code after return") but are still type-checked.
The trailing expression (implicit return) also emits S016 if a prior return was seen.

### 8.2 Never-Type Coercion

**Invariant CC-43** (`control.cpp:63-79`): In ternary expressions, `Never` coerces to the other
branch's type. If the true branch is `Never`, the result is the false branch type. If the false
branch is `Never`, the result is the true branch type. Both branches `Never` is allowed (the
result is `Never`).

**Invariant CC-44** (`control.cpp:117-142`): In `when` arms, `Never` is absorbed: a diverging
arm contributes no type to the accumulated result. The first non-`Never` arm type becomes the
result type for all subsequent non-`Never` arms to match against.

### 8.3 Loop Return Types

**Invariant CC-45** (`control.cpp:145-168, 170-234`): Both `check_loop` and `check_for` always
return `make_unit()`. The value produced by a loop body or a `break` expression is discarded at
the type level. TML does not support `break with value` as a loop return type (unlike Rust).

### 8.4 Range Expressions

**Invariant CC-46** (`control.cpp:236-265`): `check_range` returns `Range[T]` or
`RangeInclusive[T]` where `T` is determined by the start expression (or end expression if start
is absent, defaulting to `I64` if both are absent). The `for` loop's element type extraction
(`check_for:184-186`) handles both `Range[T]` and `RangeInclusive[T]` identically.

---

## 9. Constant Evaluation

### 9.1 What Is Evaluated at Check Time

**Invariant CC-47** (`const_eval.cpp:53-285`): `evaluate_const_expr` handles:
- Integer literals (signed `I64` or unsigned `U64` based on `is_signed_integer(expected_type)`)
- Bool literals
- Char literals (truncated to `char` from `char32_t`)
- Unary: `Neg` (sign flip), `Not` (bool invert), `BitNot` (bitwise complement)
- Binary: all arithmetic, bitwise, and comparison operators for both signed and unsigned integer
  operands and bool operands

The function returns `std::nullopt` (not an error) when it cannot evaluate an expression
statically — e.g., a runtime variable reference or an unrecognised expression form. Array size
expressions and const generic arguments that return `nullopt` defer to monomorphization.

**Invariant CC-48** (`const_eval.cpp:92-107`): Identifier expressions in const context are
resolved in two steps:
1. Check `current_const_params_` — if the name is a const generic parameter, return `nullopt`
   (defer to monomorphization, not an error).
2. Check `const_values_` — if the name has a previously evaluated constant value, return it.

`const_values_` is populated when a `const` declaration is evaluated. This means forward
references in const expressions (referencing a constant before it is declared in source order)
are NOT supported in the current implementation: `const B = A + 1; const A = 10;` would fail
to evaluate `B`.

**Invariant CC-49** (`resolve.cpp:97-130`): When resolving a generic type argument marked as
`is_const`, `evaluate_const_expr` is called with `make_i64()` as the expected type. The
resulting `ConstValue::type` (the type associated with the evaluated constant) is used as the
type argument. This means const generic arguments always produce an integer type at the type
level, regardless of whether they are used as array sizes or other const generics.

### 9.2 Division by Zero at Check Time

**Invariant CC-50** (`const_eval.cpp:171-180`): Division and modulo in const expressions check
for a zero *right operand*. If the right operand evaluates to zero (exact `int64_t(0)` or
`uint64_t(0)`), error T020 is emitted and `nullopt` is returned. There is no similar check for
*runtime* division — only the literal-zero check in binary expressions at non-const sites
(T052, `expr_ops.cpp:120-127`).

---

## 10. Type Resolution Algorithm

### 10.1 `resolve_type` Dispatch Table

(`resolve.cpp:42-220`) The `resolve_type` function dispatches on the parser type variant:

| Parser type | Semantic type produced | Notes |
|---|---|---|
| `NamedType` | `NamedType` or alias expansion | Calls `resolve_type_path`, then handles generics |
| `RefType` | `RefType` | Recursively resolves inner type |
| `PtrType` | `PtrType` | Recursively resolves inner type |
| `ArrayType` | `ArrayType` | Size evaluated at check time; const generic param tracked |
| `SliceType` | `SliceType` | Recursively resolves element type |
| `InferType` (`_`) | `TypeVar` (fresh) | Creates new inference variable |
| `DynType` | `DynBehaviorType` | Validates object safety (no generic methods) |
| `ImplBehaviorType` | `ImplBehaviorType` | Validates behavior existence |
| `FuncType` | `FuncType` | Resolves all params and return type |
| `TupleType` | `TupleType` | Resolves all element types |

**Invariant CC-51** (`resolve.cpp:121-148`): `dyn Behavior` (dynamic dispatch) fails with error
T201 if the behavior has any method with generic type parameters. The object safety check is
done at resolution time, not at usage time. This means a `dyn Behavior` type annotation is
rejected before the behavior is ever used.

### 10.2 Associated Type Resolution

**Invariant CC-52** (`resolve.cpp:226-264`): Two-segment paths (`A::B`) are resolved in three
steps:
1. If `A == "This"` and `current_self_type_` is set, look up `B` in `current_associated_types_`.
2. If `A` is a type parameter in `current_type_params_`, look up `A::B` as a where-clause
   equality key in `current_associated_types_`.
3. If neither, produce a placeholder `NamedType{"A::B", "", {}}` for later substitution.

The critical note in the source (`resolve.cpp:254-263`) warns that using
`current_associated_types_` for inner type parameters (case 2) would cause incorrect substitution
if an inner generic type has the same parameter name as the impl's own associated type. The
placeholder approach avoids this by deferring resolution.

### 10.3 Type Variable Inference

**Invariant CC-53** (`env_core.cpp:57-63`): `unify(a, b)` records `substitutions_[a.id] = b`
if `a` is a `TypeVar`, or `substitutions_[b.id] = a` if `b` is. It does NOT perform full HM
occurs-check. Cycles in the substitution chain are detected lazily by `resolve_impl` using a
`visited` set (`env_core.cpp:74-91`). If a cycle is detected, `resolve` returns the `TypeVar`
node itself (no infinite loop, but the type remains unresolved).

---

## 11. Helpers and the Resolve Algorithm

### 11.1 `get_all_known_names` and `find_similar_names`

(`helpers.cpp:262-323`) These are used by `check_path` to generate "did you mean?" suggestions
when an identifier cannot be resolved. `get_all_known_names` collects:
1. All variable names from the current scope chain
2. All function names from `env_.all_func_names()`
3. All struct names from `env_.all_structs()`
4. All behavior names from `env_.all_behaviors()`
5. All enum names from `env_.all_enums()`

`find_similar_names` uses the Levenshtein distance with a threshold of `max(2, len/2)` and
returns at most the first 3 candidates sorted by distance.

**Invariant CC-54**: "Did you mean?" suggestions are computed on every `T207` (undefined
identifier) error. This is an O(N × M) operation where N is the total number of known names and
M is the length of the unknown name. For large programs this could be slow. No caching is
performed between calls.

### 11.2 `substitute_type`

Referenced throughout but defined in `type.cpp` (covered in Section 1). Called from:
- `check_field_access` for struct field types with generic type params (`expr_ops.cpp:334, 441,
  507, 517`)
- `bind_pattern` for enum variant payload types (`stmt.cpp:372`)
- `resolve_type` for type alias generic substitution (`resolve.cpp:82`)

The pattern is always: build `std::unordered_map<std::string, TypePtr>` from type parameter
names to concrete types, then call `substitute_type(field_type, subs)`.

---

## 12. Return Coverage Analysis

(`resolve.cpp:372-450`) Three recursive functions analyse whether blocks/statements/expressions
contain return paths:

- `block_has_return` — true if any statement has a return OR there is a trailing expression
  (even a non-return trailing expression is considered a "return path" because the block's value
  propagates out)
- `stmt_has_return` — delegates to `expr_has_return` for the statement's expression(s)
- `expr_has_return` — true for:
  - `ReturnExpr` (explicit return)
  - `BlockExpr` (delegates to `block_has_return`)
  - `IfExpr` — only if BOTH branches have returns
  - `WhenExpr` — only if ALL arms have returns
  - `LoopExpr` — if body has return
  - `TernaryExpr` — only if both branches have returns

**Invariant CC-55** (`resolve.cpp:380-388`): `block_has_return` returns `true` if the block has
a trailing expression. This is intentionally broad: a block like `{ 42 }` (trailing `42`, no
explicit return) is considered as "having a return" for coverage analysis. This means the checker
uses a relaxed definition of return coverage that does not distinguish between "exits via return
statement" and "exits via trailing expression value". A function with a trailing expression in
its body block passes the return coverage check.

---

## 13. Import Conflict Handling

**Invariant CC-56** (`env_module_support.cpp:244-278`): `import_symbol` detects conflicts when
the same local name (after alias resolution) is imported from two different module paths or with
two different original names. The conflict is recorded in `import_conflicts_[local_name]` (a
`std::set<std::string>` of `"module_path::original_name"` strings). After recording the
conflict, the import proceeds and the *last import wins* (the symbol map is overwritten). Errors
are NOT generated at import time; they must be generated during symbol resolution when
`has_import_conflict` is checked.

**Invariant CC-57** (`env_module_support.cpp:350-424`): Glob re-exports from modules are resolved
*transitively* but only to two levels of nesting. A module that re-exports a glob from a module
that itself re-exports a glob has its nested re-exports processed. A module that has a third
level of nesting would not be visited. The two-level limit is not enforced explicitly; it follows
from the code structure (one level of `for (const auto& nested_re_export : source_module->re_exports)`
inside the outer loop, without further recursion).

---

## 14. `lowlevel` Block Semantics

**Invariant CC-58** (`expr_special.cpp:131-153`): A `lowlevel` block:
1. Sets `in_lowlevel_ = true`
2. Pushes a new scope
3. Checks all statements (in the `lowlevel` scope)
4. Checks the trailing expression
5. Pops the scope
6. Restores `in_lowlevel_` to its prior value

The `in_lowlevel_` flag is visible to:
- `UnaryOp::Ref` — returns `PtrType` instead of `RefType`
- `UnaryOp::RefMut` — returns mutable `PtrType` instead of mutable `RefType`

No other type-checking behaviour is altered by `in_lowlevel_`. Borrow checking, mutability
rules, and all other checks run normally inside `lowlevel` blocks.

---

## 15. `await` Expression Typing

**Invariant CC-59** (`expr_special.cpp:73-129`): `.await` is only valid inside an `async` function
(checked via `in_async_func_`, error T032 if outside). The type extraction uses three fallback
cases:
1. If the awaited expression is a `NamedType` and the type implements `Future`, the first type
   argument is the output type.
2. If the awaited expression is a `FuncType` with `is_async = true`, the function's return type
   is the output type.
3. If the awaited expression is an `ImplBehaviorType` for `Future`, the first type argument is
   the output type.
4. Fallback: return the expression type unchanged (no error emitted).

This means `.await` on a non-Future type silently passes type checking without error.

---

## 16. Closure Capture Analysis

**Invariant CC-60** (`types_checker.cpp:262-317`): `check_closure` processes captures *before*
defining parameters. The capture analysis (`collect_captures_from_expr`) uses a *temporary empty
scope* as the closure scope — at the time of analysis, no parameters have been defined yet in
this scope. A variable that matches a parameter name would therefore be incorrectly identified
as a captured variable during this initial analysis. However, since `collect_captures_from_expr`
checks `closure_scope->lookup_local(e.name)` and the scope is empty, all references in the body
are treated as captures at this stage.

This is an approximation: after parameters are defined in the same scope (`temp_scope`), the
body is re-checked by `check_expr(*closure.body)`, which produces the correct types. The
`captured_vars` list stored in the AST is used by HIR builder for environment construction.

**Invariant CC-61** (`types_checker.cpp:309-316`): Inferred closure parameter types and the
inferred return type are stored back into the `ClosureExpr` AST node via `shared_ptr<void>` type
erasure (to avoid circular header dependencies). HIR builder reads these back by casting to
`TypePtr`. This is the primary mechanism by which type checker results are communicated to HIR
for untyped closures (closures without explicit type annotations).

---

## 17. Summary Table of All Invariants

| ID | Category | File | Key Claim |
|---|---|---|---|
| CC-01 | Phase ordering | `env_core.cpp:32-35` | `init_builtins()` called unconditionally in ctor |
| CC-02 | Phase ordering | `builtins/register.cpp:38-52` | Builtin init order is fixed and meaningful |
| CC-03 | Phase ordering | `env_core.cpp:102-122` | Snapshot shares definitions, resets local state |
| CC-04 | Phase ordering | `resolve.cpp:274-277` | Calling `resolve_type` before builtins = silent errors |
| CC-05 | Type lookup | `resolve.cpp:279-282` | Builtins shadow user type aliases |
| CC-06 | Type lookup | `resolve.cpp:284-299` | Struct > class > enum > behavior priority |
| CC-07 | Global state | `env_definitions.cpp:27-37` | Last define wins for structs/enums/behaviors |
| CC-08 | Global state | `env_definitions.cpp:39-61` | Functions append to overload vector |
| CC-09 | Global state | `env_core.cpp:102-122` | Type var IDs are file-local |
| CC-10 | Checker state | `expr_special.cpp:131-153` | `in_lowlevel_` is saved/restored |
| CC-11 | Checker state | `control.cpp:146-167` | `loop_depth_` accurately tracks nesting |
| CC-12 | Error recovery | `resolve.cpp:359-369` | Error = append, not throw; fallback = Unit |
| CC-13 | Error recovery | `expr_ops.cpp:57-65` | Binary mismatch returns left operand type |
| CC-14 | Error recovery | `control.cpp:38-52` | `if` result is `then_type` even on mismatch |
| CC-15 | Error recovery | `control.cpp:101-143` | `when` result is first non-Never arm |
| CC-16 | Error recovery | `helpers.cpp:91-228` | `types_compatible` accepts integer/float widening |
| CC-17 | Error recovery | `resolve.cpp:354-357` | Unknown type name = silent `NamedType` placeholder |
| CC-18 | Scope | `env_scope.cpp:37-45` | Inner scope shadows outer, no ambiguity error |
| CC-19 | Scope | `env_scope.cpp:48-54` | `lookup_local` used for closure capture detection |
| CC-20 | Scope | `expr_ops.cpp:67-83` | Binding mutability != referenced value mutability |
| CC-21 | Scope | `expr_ops.cpp:558-613` | `check_block` saves/restores `returned_in_block_` |
| CC-22 | Scope | `control.cpp:86-99, 107-143` | Pattern bindings scope to arm only |
| CC-23 | Scope | `control.cpp:145-168` | Loop variable scoped to loop body |
| CC-24 | Scope | `control.cpp:170-234` | `for` pattern scoped to loop body |
| CC-25 | Builtins | `builtins/types.cpp:49-65` | 16 primitive names in `builtins_`; `Never` absent |
| CC-26 | Builtins | `resolve.cpp:274-277` | Builtin lookup by last path segment |
| CC-27 | Builtins | `builtins/types.cpp:67-107` | Core enums in `enums_`, not `builtins_` |
| CC-28 | Builtins | `builtins/types.cpp:109-183` | Core behaviors in `behaviors_` |
| CC-29 | Builtins | `builtins/types.cpp:187-248` | Primitives + Ordering are Send/Sync |
| CC-30 | Operators | `expr_ops.cpp:44-55` | Arithmetic propagates left type to right literal |
| CC-31 | Operators | `expr_ops.cpp:86-113` | Pointer arithmetic type rules |
| CC-32 | Operators | `expr_ops.cpp:145-183` | Assignment operators return `Unit` |
| CC-33 | Operators | `expr_ops.cpp:202-226` | Reborrow semantics for `&` and `&mut` |
| CC-34 | Operators | `expr_ops.cpp:233-262` | Hard-coded deref smart pointer list |
| CC-35 | Statements | `stmt.cpp:61-101` | `let` annotation-first resolution |
| CC-36 | Statements | `stmt.cpp:104-135` | `var` uses `define` directly, not `bind_pattern` |
| CC-37 | Statements | `stmt.cpp:137-177` | `let-else` doesn't verify divergence |
| CC-38 | Patterns | `stmt.cpp:219-443` | Six pattern kinds and their error codes |
| CC-39 | Patterns | `stmt.cpp:259-317` | Enum pattern type alias lookup across modules |
| CC-40 | Control flow | `control.cpp:270-290` | `return` checks against `current_return_type_` |
| CC-41 | Control flow | `control.cpp:292-300` | `break` validates `loop_depth_ > 0` |
| CC-42 | Control flow | `expr_ops.cpp:559-613` | Unreachable code after return = S016 warning |
| CC-43 | Never | `control.cpp:63-79` | Ternary: Never coerces to other branch |
| CC-44 | Never | `control.cpp:117-142` | `when`: diverging arms absorbed from result |
| CC-45 | Loops | `control.cpp:145-168, 170-234` | Loops always return `Unit` |
| CC-46 | Ranges | `control.cpp:236-265` | Range result type is `Range[T]`/`RangeInclusive[T]` |
| CC-47 | Const eval | `const_eval.cpp:53-285` | Const-evaluable expression kinds |
| CC-48 | Const eval | `const_eval.cpp:92-107` | Forward const references not supported |
| CC-49 | Const eval | `resolve.cpp:97-130` | Const generics always produce integer type |
| CC-50 | Const eval | `const_eval.cpp:171-180` | Const division by zero = T020 at check time |
| CC-51 | Type resolve | `resolve.cpp:121-148` | `dyn` object safety check at resolution time |
| CC-52 | Type resolve | `resolve.cpp:226-264` | Associated type resolution: 3-step algorithm |
| CC-53 | Inference | `env_core.cpp:57-63` | Unify without occurs-check; cycle = lazy detect |
| CC-54 | Suggestions | `helpers.cpp:262-323` | "Did you mean?" = O(N×M), no caching |
| CC-55 | Returns | `resolve.cpp:380-388` | Trailing expression = "has return" for coverage |
| CC-56 | Imports | `env_module_support.cpp:244-278` | Import conflict: last wins, deferred error |
| CC-57 | Imports | `env_module_support.cpp:350-424` | Transitive glob re-exports: two levels only |
| CC-58 | lowlevel | `expr_special.cpp:131-153` | `lowlevel` scope/flag lifecycle |
| CC-59 | Async | `expr_special.cpp:73-129` | `.await` fallback: no error on non-Future |
| CC-60 | Closures | `types_checker.cpp:262-317` | Capture analysis before param definition |
| CC-61 | Closures | `types_checker.cpp:309-316` | Inferred closure types stored in AST via `void*` |

---

## 18. Surprising Findings for Self-Hosting Implementers

The following are non-obvious behaviours discovered during this audit that are likely to cause
bugs if not explicitly preserved:

1. **`Never` is not in `builtins_`** (CC-25). A self-hosted type resolver that looks up `Never`
   in the builtins map will miss it. `Never` must be synthesised on demand by `make_never()`.

2. **Arithmetic binary operators propagate the left operand type to the right operand as an
   expected type** (CC-30). This is not true for comparison or logical operators. The asymmetry
   is intentional for integer literal inference.

3. **`types_compatible` accepts any `NamedType` as a valid implementation of any `ImplBehaviorType`
   without checking actual behavior registration** (CC-16, last rule). Behavior conformance is
   verified in Phase 3, not in body checking.

4. **The `let-else` else block is not verified to diverge** (CC-37). The divergence requirement
   is documented in the code but not enforced. HIR lowering performs this enforcement.

5. **Const forward references are silently unevaluatable** (CC-48). `const B = A + 1; const A = 10;`
   produces `nullopt` for `B`'s evaluation, which may cause a downstream error when `B` is used
   as an array size or const generic.

6. **Block trailing expressions count as "has return" for coverage analysis** (CC-55). This
   means `block_has_return` returns `true` for any block with a trailing expression, even if
   that expression is a literal `0`. Coverage analysis therefore reports "all paths return" for
   `func foo() -> I32 { 0 }` — correctly — but also for any block containing a non-return value
   expression.

7. **Closure capture analysis uses an empty scope as the "closure scope"** (CC-60). Parameters
   are not yet in scope during capture analysis. This can over-report captures. The captured
   variable list is used by HIR builder for environment allocation; over-reporting is safe (it
   wastes a slot) but under-reporting would be incorrect (missing a capture = wrong IR).

8. **The `deref_types` list** (CC-34) is hard-coded in both `check_unary` and
   `check_field_access` as a static set. If a new smart pointer type is added, both lists must
   be updated. There is no shared definition.

9. **Glob re-export resolution is limited to two levels of nesting** (CC-57). A module that
   re-exports from a module that re-exports from a third module will have the third module's
   symbols omitted from glob imports.

10. **FFI function registration** (`env_definitions.cpp:44-60`): FFI functions (`@extern("c")`)
    that have an `ffi_module` field are registered both in `functions_` (for direct call
    resolution) AND in the module registry under the FFI module name. This enables both
    `init()` and `SDL2::init()` to resolve to the same function. A self-hosted implementation
    must replicate this dual-registration.
