# Section 1 — Type Registration Invariants

**Task**: phase12c_typechecker-invariants
**Section**: 1 of 6
**Phase**: Type Registration
**Source files analyzed**:
- `compiler/include/types/type.hpp` (427 LOC)
- `compiler/src/types/type.cpp` (965 LOC)
- `compiler/include/types/env.hpp` (807 LOC)
- `compiler/src/types/env_core.cpp` (128 LOC)
- `compiler/src/types/env_definitions.cpp` (139 LOC)
- `compiler/src/types/env_lookups.cpp` (1265 LOC, partial)
- `compiler/src/types/checker/core.cpp` (1412 LOC)
- `compiler/src/types/builtins/register.cpp` (53 LOC)
- `compiler/src/types/builtins/types.cpp` (251 LOC)
- `compiler/src/types/builtins_cache.cpp` (33 LOC)
- `compiler/include/types/builtins_cache.hpp` (55 LOC)

---

## 1. The `Type` Struct

### 1.1 Variant Enumeration

The semantic type representation is a single `struct Type` whose content is stored in a `std::variant` field named `kind`. The complete set of variant types, in declaration order, is (`type.hpp:294`):

| Index | Variant           | Represents                                         |
|-------|-------------------|----------------------------------------------------|
| 0     | `PrimitiveType`   | I8–I128, U8–U128, F32, F64, Bool, Char, Str, Unit, Never |
| 1     | `NamedType`       | User-defined struct or enum (by name + module path) |
| 2     | `RefType`         | `ref T` or `mut ref T` (immutable or mutable reference) |
| 3     | `PtrType`         | `*T` or `*mut T` (raw pointer) |
| 4     | `ArrayType`       | Fixed-size `[T; N]` or `[T; N@param]` (const generic) |
| 5     | `SliceType`       | `[T]` (slice, dynamically-sized) |
| 6     | `TupleType`       | `(A, B, C)` (zero or more element types) |
| 7     | `FuncType`        | `func(A, B) -> R` (function pointer) |
| 8     | `ClosureType`     | `do(A) R` with explicit captured variables |
| 9     | `TypeVar`         | Inference variable `?N` (resolved via `substitutions_`) |
| 10    | `GenericType`     | Unresolved generic type parameter (e.g., `T`) |
| 11    | `ConstGenericType`| Const generic parameter `const N: U64` |
| 12    | `DynBehaviorType` | `dyn Behavior[T]` (trait object, fat pointer) |
| 13    | `ImplBehaviorType`| `impl Behavior` (opaque return type) |
| 14    | `ClassType`       | OOP class instance (by name + module path) |
| 15    | `InterfaceType`   | OOP interface (by name + module path) |

**Implementation note**: The variant index is significant. The `TypeEnv::types_match()` static method uses `a->kind.index() != b->kind.index()` as its first discriminator (`env_lookups.cpp:142`). This means a `NamedType{"I64"}` and a `PrimitiveType{I64}` are NOT equal under `types_match`, even though they name the same concept. This asymmetry matters during overload resolution.

### 1.2 Type Identity Field

Every `Type` carries a `uint64_t id` field initialized to `0` in the struct definition (`type.hpp:298`). Factory functions assign a globally unique monotonically increasing integer from `next_type_id` (`type.cpp:36-37`). The ID counter is a file-scope static inside `type.cpp`; it is NOT per-compilation-unit but rather process-global.

**Invariant 1** (type.cpp:36-44): Every `TypePtr` produced by a factory function (`make_primitive`, `make_ref`, etc.) has a unique non-zero `id`. Two independently-created `TypePtr` objects representing the same logical type (e.g., two calls to `make_i64()`) will have different `id` values.

**Invariant 2** (type.cpp:317): `types_equal(a, b)` returns `true` if `a->id == b->id` (pointer sharing shortcut) OR if the variants have the same structure recursively. It does NOT rely solely on `id` equality for correctness. This means the `id` field is only a fast-path cache, not an identity proof.

**Invariant 3** (env_core.cpp:65-92): The `resolve()` method in `TypeEnv` is the only function that dereferences `TypeVar` substitutions. `types_equal()` in `type.cpp` does NOT resolve type variables; it compares `TypeVar` variants by their `id` field (`type.cpp:387-390`). Therefore, calling `types_equal` on unresolved type variables before `resolve()` is run will return false even if they would unify to the same type.

### 1.3 Field Details by Variant

**`PrimitiveType`** (`type.hpp:75-77`):
- `kind: PrimitiveKind` — one of 17 enum values: I8, I16, I32, I64, I128, U8, U16, U32, U64, U128, F32, F64, Bool, Char, Str, Unit, Never.

**`NamedType`** (`type.hpp:80-84`):
- `name: std::string` — unqualified type name (e.g., `"Maybe"`, `"HashMap"`)
- `module_path: std::string` — fully qualified module path, may be empty for locally-defined types
- `type_args: std::vector<TypePtr>` — generic arguments, empty for non-generic types

**Invariant 4** (type.cpp:329-336): `types_equal` for `NamedType` requires **both** `name` and `module_path` to match, plus recursive equality of all `type_args`. Two types with the same name but different module paths are different types.

**Invariant 5** (type.cpp:476-479): `substitute_type()` treats a `NamedType` with no `type_args` whose `name` matches a substitution key as a type parameter (i.e., the same as `GenericType`). This is because the type checker sometimes encodes type parameters as bare `NamedType{name, "", {}}` rather than `GenericType{name}`. Both forms participate in substitution. The `GenericType` variant takes priority (checked first), but `NamedType` with empty args and empty module path is also substituted.

**`RefType`** (`type.hpp:86-91`):
- `is_mut: bool` — false for `ref T`, true for `mut ref T`
- `inner: TypePtr` — the referenced type
- `lifetime: std::optional<std::string>` — always `std::nullopt` at type-check time; lifetimes are inferred, never syntax-visible

**`PtrType`** (`type.hpp:93-96`):
- `is_mut: bool` — false for `*T`, true for `*mut T`
- `inner: TypePtr`

**Invariant 6** (type.cpp:339-342): `types_equal` for `RefType` checks `is_mut` and recursively checks `inner`, but does NOT compare `lifetime`. This means `ref[a] T` and `ref[b] T` are considered equal regardless of lifetime annotation. This is consistent with TML's "lifetimes always inferred, never syntax-visible" design principle.

**`ArrayType`** (`type.hpp:99-105`):
- `element: TypePtr`
- `size: size_t` — concrete array size when known; 0 when parameterized by const generic
- `const_generic_param: std::string` — name of the const generic param (e.g., `"N"`), empty when size is concrete

**`TupleType`** (`type.hpp:112-115`):
- `elements: std::vector<TypePtr>` — empty vector represents Unit `()`

**Implementation note**: The zero-element tuple and `PrimitiveType{Unit}` are distinct types in the type system. The `make_unit()` factory creates `PrimitiveType{Unit}`, not `TupleType{}`. These are NOT equal under `types_equal`. In practice, the checker always uses `make_unit()` for unit returns; the `TupleType{}` form is not generated by any current checker path.

**`FuncType`** (`type.hpp:117-121`):
- `params: std::vector<TypePtr>` — parameter types in declaration order
- `return_type: TypePtr` — return type; never null (defaults to `make_unit()`)
- `is_async: bool`

**Invariant 7** (type.cpp:355-366): `types_equal` for `FuncType` checks `is_async` first, then return type, then parameters. An `async func` and a sync `func` with identical signatures are DIFFERENT types.

**`ClosureType`** (`type.hpp:131-136`):
- `params: std::vector<TypePtr>`
- `return_type: TypePtr`
- `captures: std::vector<CapturedVar>` — ordered list of captured variables with name, type, and mutability

**Invariant 8** (type.cpp:367-386): `types_equal` for `ClosureType` checks parameters, return type, AND captures (name, type, mutability for each). Two closures with the same signature but different capture lists are different types. This has implications for method resolution: a closure stored in a struct field and later called will be recognized only if its capture list is identical to what was stored.

**`TypeVar`** (`type.hpp:141-145`):
- `id: uint32_t` — unique inference variable identifier, assigned by `fresh_type_var()` from `type_var_counter_` in `TypeEnv`
- `bound: std::optional<TypePtr>` — always `std::nullopt` in current implementation; upper bound tracking is declared but not used

**`GenericType`** (`type.hpp:148-151`):
- `name: std::string` — type parameter name (e.g., `"T"`, `"K"`, `"V"`)
- `bounds: std::vector<TypePtr>` — behavior bounds declared in the generic parameter list

**Invariant 9** (type.cpp:388-390): `types_equal` for `GenericType` compares only `name`, not `bounds`. Two generic parameters `T: Clone` and `T` are considered equal if they have the same name.

**`ConstGenericType`** (`type.hpp:218-222`):
- `name: std::string` — parameter name (e.g., `"N"`)
- `value_type: TypePtr` — the type of the const (e.g., `U64`, `I32`)
- `resolved_value: std::optional<int64_t>` — concrete value after monomorphization; `std::nullopt` before resolution

**Invariant 10** (type.cpp:391-393): `types_equal` for `ConstGenericType` compares `name` AND `value_type`, but NOT `resolved_value`. A const generic parameter `const N: U64` with different `resolved_value` (before vs. after monomorphization) is still considered equal if name and value_type match.

**`DynBehaviorType`** (`type.hpp:224-231`):
- `behavior_name: std::string`
- `type_args: std::vector<TypePtr>`
- `is_mut: bool` — true for `dyn mut Behavior`

**`ImplBehaviorType`** (`type.hpp:237-240`):
- `behavior_name: std::string`
- `type_args: std::vector<TypePtr>`

**`ClassType`** (`type.hpp:263-267`):
- `name: std::string`
- `module_path: std::string`
- `type_args: std::vector<TypePtr>`

**`InterfaceType`** (`type.hpp:282-286`):
- `name: std::string`
- `module_path: std::string`
- `type_args: std::vector<TypePtr>`

**Invariant 11** (type.cpp:415-434): `types_equal` for both `ClassType` and `InterfaceType` requires both `name` and `module_path` to match, plus `type_args`. This mirrors the `NamedType` equality rule.

---

## 2. The `TypeEnv` Structure

### 2.1 Field Layout

`TypeEnv` (`env.hpp:440-803`) contains the following private fields, grouped by concern:

**Type definition tables** (persist across snapshot copies; `env.hpp:757-773`):
```
structs_:            unordered_map<string, StructDef>
enums_:              unordered_map<string, EnumDef>
behaviors_:          unordered_map<string, BehaviorDef>
functions_:          unordered_map<string, vector<FuncSig>>    // supports overloads
behavior_impls_:     unordered_map<string, vector<string>>     // type -> [behavior names]
type_aliases_:       unordered_map<string, TypePtr>
type_alias_generics_: unordered_map<string, vector<string>>
builtins_:           unordered_map<string, TypePtr>
```

**OOP type definition tables** (persist across snapshot copies; `env.hpp:770-774`):
```
classes_:          unordered_map<string, ClassDef>
interfaces_:       unordered_map<string, InterfaceDef>
class_interfaces_: unordered_map<string, vector<string>>  // class -> [interface names]
```

**Per-file inference state** (reset by `snapshot()`; `env.hpp:777-779`):
```
current_scope_:     shared_ptr<Scope>            // current lexical scope chain
type_var_counter_:  uint32_t                     // next fresh TypeVar id
substitutions_:     unordered_map<uint32_t, TypePtr>  // TypeVar resolutions
```

**Per-file module state** (reset by `snapshot()`; `env.hpp:782-790`):
```
module_registry_:    shared_ptr<ModuleRegistry>
current_module_path_: string
source_directory_:   string
imported_symbols_:   unordered_map<string, ImportedSymbol>
import_conflicts_:   unordered_map<string, set<string>>
abort_on_module_error_: bool  (NOT reset — copied from source)
loading_modules_:    unordered_set<string>       // cycle detection during module loading
```

**Cycle detection** (mutable, not part of snapshot; `env.hpp:752-755`):
```
type_needs_drop_visiting_: mutable unordered_set<string>
```

### 2.2 Which Fields Are Valid at Which Phase

| Phase | Valid Fields | Invalid / Not Yet Populated |
|-------|-------------|----------------------------|
| Before `TypeEnv()` constructor | None | All |
| After `TypeEnv()` constructor | `builtins_`, `enums_` (Ordering, Maybe, Outcome, Poll), `behaviors_` (Future, Drop, Send, Sync), `behavior_impls_` for all primitives, `functions_` (IO, mem, atomic, sync, math, async), `current_scope_` (root) | `structs_`, `classes_`, `interfaces_`, `imported_symbols_`, `module_registry_`, paths |
| After Phase 0 (imports) | `imported_symbols_`, `module_registry_` loaded | `structs_` (user types), function bodies |
| After Phase 1 (type decls) | `structs_`, `enums_`, `behaviors_`, `type_aliases_`, `classes_`, `interfaces_` for current module | `functions_` (user functions), `behavior_impls_` (user impls) |
| After Phase 2 (func decls + impls) | `functions_` (user-defined), `behavior_impls_` (user), default method registrations | Function bodies unchecked |
| After Phase 3 (body checking) | All fields valid; `substitutions_` populated, then resolved | TypeVar nodes in expressions should be resolved |

**Invariant 12** (env_core.cpp:32-35): The `TypeEnv()` constructor unconditionally calls `init_builtins()`. This means the builtin type tables are populated before any user code is registered. There is no two-step initialization; the constructor leaves the env in a ready state for Phase 0 use.

**Invariant 13** (builtins_cache.cpp:17-25): In the `TypeChecker`, the `TypeEnv` is NOT created directly by `TypeEnv()`. Instead, `TypeChecker::TypeChecker()` calls `BuiltinsSnapshot::instance().create_env()` (`core.cpp:191`). On the first call, `BuiltinsSnapshot` creates one `TypeEnv` (which runs `init_builtins()`), then subsequent calls return `snapshot()` copies. The `snapshot()` constructor (`env_core.cpp:102-122`) copies all type definition tables but resets all per-file inference/module state. This means every compilation unit starts with an identical, pre-populated set of builtin definitions and a fresh scope/inference state.

**Invariant 14** (env_core.cpp:102-122): After `snapshot()`, the following per-file fields are in their zero/empty state:
- `current_scope_` — a new root scope (no parent, no symbols)
- `type_var_counter_` — 0 (next TypeVar will have id=0)
- `substitutions_` — empty (no type variables resolved)
- `module_registry_` — a fresh empty `ModuleRegistry` (no modules loaded)
- `current_module_path_` — empty string
- `source_directory_` — empty string
- `imported_symbols_` — empty
- `import_conflicts_` — empty
- `loading_modules_` — empty (not mentioned; default-constructed)

The `abort_on_module_error_` flag IS copied from the source, not reset.

---

## 3. Registration Order

### 3.1 Builtin Initialization Order

The `init_builtins()` function is called from the `TypeEnv()` constructor (`env_core.cpp:34`) and runs these initializers in strict order (`builtins/register.cpp:38-52`):

1. `init_builtin_types()` — primitives, core enums, core behaviors, primitive behavior impls
2. `init_builtin_io()` — I/O functions (print, println, panic, assert)
3. `init_builtin_mem()` — memory functions (mem_alloc, mem_free, mem_copy)
4. `init_builtin_atomic()` — atomic operations
5. `init_builtin_sync()` — synchronization primitives
6. `init_builtin_math()` — math functions
7. `init_builtin_async()` — async runtime (block_on)

Within `init_builtin_types()` (`builtins/types.cpp:48-249`), the sub-order is:

1. Primitive types into `builtins_` — I8 through Str and Unit (16 entries)
2. `Ordering` enum (variants: Less, Equal, Greater)
3. `Maybe[T]` enum (variants: Just(T), Nothing)
4. `Outcome[T,E]` enum (variants: Ok(T), Err(E))
5. `Poll[T]` enum (variants: Ready(T), Pending)
6. `Future` behavior (with associated type `Output` and method `poll`)
7. `Drop` behavior (with method `drop`)
8. `Send` marker behavior (no methods)
9. `Sync` marker behavior (no methods)
10. `behavior_impls_` for integer types (I8–U128): Eq, Ord, Numeric, Hash, Display, Debug, Default, Duplicate
11. `behavior_impls_` for float types (F32, F64): Eq, Ord, Numeric, Display, Debug, Default, Duplicate
12. `behavior_impls_` for Bool: Eq, Ord, Hash, Display, Debug, Default, Duplicate
13. `behavior_impls_` for Char: Eq, Ord, Hash, Display, Debug, Duplicate
14. `behavior_impls_` for Str: Eq, Ord, Hash, Display, Debug, Duplicate
15. `behavior_impls_` for all primitives + Ordering: Send, Sync

**Invariant 15** (builtins/types.cpp:69-107): The four core enums `Ordering`, `Maybe`, `Outcome`, and `Poll` are registered into `enums_` during `init_builtin_types()`, before any user-defined type registration. Their definitions are available for all phases of type checking without any `use` statement.

**Invariant 16** (builtins/types.cpp:110-184): The behaviors `Future`, `Drop`, `Send`, and `Sync` are registered into `behaviors_` during `init_builtin_types()`. They are available from the start of Phase 1. No `use` statement is required to reference them.

### 3.2 Per-Module Registration Order

`TypeChecker::check_module()` processes declarations in four sequential passes over the same `module.decls` list (`core.cpp:193-261`):

**Pass 0 — Import processing** (`core.cpp:203-208`):
Iterates all declarations; for each `UseDecl`, calls `process_use_decl()`. This pass:
- Loads referenced native modules via `env_.load_native_module()`
- Registers symbols into `imported_symbols_` via `env_.import_symbol()` or `env_.import_all_from()`
- Does NOT register any types, functions, or behavior impls in the current module

**Pass 1 — Type declaration registration** (`core.cpp:209-229`):
Iterates all declarations; dispatches to:
- `register_struct_decl()` → `env_.define_struct()` → `structs_[name]`
- `register_union_decl()` → `env_.define_struct()` with `is_union=true` → `structs_[name]`
- `register_enum_decl()` → `env_.define_enum()` → `enums_[name]`
- `register_trait_decl()` → `env_.define_behavior()` → `behaviors_[name]`
- `register_type_alias()` → `env_.define_type_alias()` → `type_aliases_[name]`
- `register_interface_decl()` → `env_.define_interface()` → `interfaces_[name]`
- `register_class_decl()` → `env_.define_class()` → `classes_[name]`
- `register_namespace_decl()` → recursively processes nested declarations in all 3 passes

**Pass 2 — Function signature and impl registration** (`core.cpp:231-244`):
- `check_func_decl()` → `env_.define_func()` → `functions_[name]` (appends to overload vector)
- `check_impl_decl()` → registers methods as `TypeName::method_name` in `functions_`, registers behavior impls in `behavior_impls_`, registers default method implementations
- `check_const_decl()` → evaluates const expr, defines in current scope
- `check_class_decl()` → registers class method signatures
- `check_interface_decl()` → registers interface method signatures

**Pass 3 — Body checking** (`core.cpp:246-255`):
- `check_func_body()` → type-checks function body expressions
- `check_impl_body()` → type-checks impl method bodies
- `check_class_body()` → type-checks class method bodies

**Invariant 17** (core.cpp:203-255): The four passes are strictly sequential with no interleaving. Pass N+1 cannot begin until Pass N completes for ALL declarations. This guarantees that all types are registered before any function signature is resolved (Pass 1 before Pass 2), and all function signatures are registered before any body is checked (Pass 2 before Pass 3).

**Invariant 18** (core.cpp:209-229): During Pass 1, `resolve_type()` is called on struct field types, enum variant payload types, behavior method signatures, and type alias right-hand sides. This means type fields are resolved using only the types available at that point: builtins plus types registered earlier in the same pass. Within a single module, a struct that refers to another struct defined later in the same file CAN be registered before its referenced type is registered, because `resolve_type()` on `NamedType` simply records the name string — it does not validate that the referenced type exists. This is NOT a two-pass resolution; referencing an undefined type in a struct field does not error at this point.

**Invariant 19** (core.cpp:342-453): `register_trait_decl()` calls `resolve_type()` on all method parameter and return types. Super behaviors are extracted from the AST by reading `decl.super_traits` and storing only the behavior name strings in `BehaviorDef.super_behaviors`. The super behavior definitions do NOT need to be registered before the sub-behavior is registered; only the name string is stored.

### 3.3 Impl Block Registration Order

`check_impl_decl()` (`core.cpp:1143-1375`) performs its own internal ordering:

1. Resolve `impl.self_type` to determine `type_name` and `specialized_type_name`
2. Set `current_self_type_` and `current_associated_types_` (for resolving `This::Item` in method signatures)
3. Register constants as scope bindings under `TypeName::const_name`
4. Register all methods as `TypeName::method_name` in `functions_` via `env_.define_func()`
5. If a specialized impl key exists (e.g., `Pin[Heap]`), register again under `Pin[Heap]::method_name`
6. If `impl.trait_type` is present (i.e., this is `impl Behavior for Type`):
   a. Call `env_.register_impl(type_name, behavior_name)` → adds to `behavior_impls_[type_name]`
   b. Look up `behavior_def` via `env_.lookup_behavior(behavior_name)`
   c. If the behavior is not yet registered, attempt on-demand load from binary cache (`builtins_cache`) for a hardcoded set of well-known behaviors (Iterator, Display, etc.)
   d. For each default method in `behavior_def` that is NOT overridden in `impl.methods`, register the default implementation substituting `This` and associated types

**Invariant 20** (core.cpp:1283-1327): `check_impl_decl()` will attempt to load a behavior definition from the binary module cache if `env_.lookup_behavior()` returns nothing. This is a fallback for well-known behaviors (hardcoded list at `core.cpp:1296-1313`). The set is:
```
Iterator, IntoIterator, FromIterator, Display, Debug, Duplicate, Hash, Default,
Error, From, Into, TryFrom, TryInto, PartialEq, Eq, PartialOrd, Ord
```
This fallback only applies during Pass 2. During Pass 1, no such fallback is attempted for super-behaviors.

**Invariant 21** (core.cpp:1232-1268): All methods in an impl block are registered under BOTH `TypeName::method_name` (always) and `SpecializedTypeName::method_name` (only if the impl is specialized, e.g., `impl[T] Pin[Heap[T]]`). Both entries point to the same `FuncSig`. The specialized name key is built as `"TypeName[InnerTypeName]"` for `NamedType` inner args, or `"TypeName[ref]"` / `"TypeName[mut_ref]"` for `RefType` inner args.

---

## 4. Invariants After Each Phase

### 4.1 Invariants After Builtin Initialization (before any user module)

1. `builtins_` contains exactly 16 entries: I8, I16, I32, I64, I128, U8, U16, U32, U64, U128, F32, F64, Bool, Char, Str, Unit.
2. `enums_` contains exactly 4 entries: Ordering, Maybe, Outcome, Poll.
3. `behaviors_` contains exactly 4 entries: Future, Drop, Send, Sync.
4. `behavior_impls_` is populated for all 16 primitive types plus Ordering.
5. `functions_` contains entries for all IO, mem, atomic, sync, math, and async builtins.
6. `structs_`, `classes_`, `interfaces_`, `type_aliases_` are all empty.
7. `current_scope_` is the root scope with no symbols.
8. `type_var_counter_` is 0.
9. `substitutions_` is empty.

### 4.2 Invariants After Pass 0 (imports)

1. `imported_symbols_` is fully populated with all symbols imported by `use` statements.
2. `module_registry_` contains all modules that were successfully loaded (native modules).
3. Any `use` statement that references a module not found in the native registry will have emitted an error (error code `T027`, `core.cpp:556`).
4. No type definitions, function signatures, or behavior impls from the current module are registered yet.

### 4.3 Invariants After Pass 1 (type declarations)

1. All user-defined structs from the current module are in `structs_`.
2. All user-defined enums are in `enums_`.
3. All user-defined behaviors are in `behaviors_`.
4. All type aliases are in `type_aliases_` and `type_alias_generics_`.
5. All OOP classes and interfaces are in `classes_` and `interfaces_`.
6. Reserved primitive type names (I8, I64, etc.) and reserved behavior names (Eq, Ord, etc.) cannot be registered by user code — any attempt emits error `T038` and returns without modifying the env (`core.cpp:344-349`, `core.cpp:457-462`).
7. Function signatures, behavior implementations, and method bodies from the current module are NOT yet registered.

### 4.4 Invariants After Pass 2 (function signatures + impls)

1. All top-level functions are in `functions_` under their plain name.
2. All impl methods are in `functions_` under `TypeName::methodName`.
3. All specialized impl methods are ALSO in `functions_` under `SpecializedTypeName::methodName`.
4. All `impl Behavior for Type` registrations are in `behavior_impls_[typeName]`.
5. Default method implementations (from behavior definitions) have been backfilled for any `impl Behavior for Type` block that does not provide the method explicitly.
6. Const values from `const` declarations are evaluated and stored in `const_values_` (TypeChecker private member, not in TypeEnv).
7. Function bodies have NOT been checked; `substitutions_` may be empty or contain only leftover state from const evaluation.

### 4.5 Invariants After Pass 3 (body checking)

1. All function bodies have been type-checked; type errors have been added to `errors_`.
2. The `substitutions_` map contains all TypeVar resolutions from the current file.
3. `current_scope_` has returned to the root scope (each `check_func_body` calls `push_scope`/`pop_scope`).
4. `current_return_type_`, `in_async_func_`, `current_where_constraints_` are all cleared/reset to their pre-body-check state.

---

## 5. Phase Dependencies

### 5.1 What Must Be Registered Before What

| Registration action | Depends on |
|---------------------|-----------|
| Any struct field type resolution | Builtins (for primitives), Pass 0 imports (for imported types) |
| Any enum variant payload resolution | Same as struct fields |
| Any behavior method signature | Same as struct fields; behavior super-names need not be registered |
| Any type alias RHS | Same as struct fields |
| Any impl method signature | Pass 1 types (to resolve `This`, associated types) |
| Any `impl Behavior for Type` | The behavior must be in `behaviors_` OR in the binary cache for the hardcoded set |
| Any function signature | Pass 0 imports; Pass 1 types for param/return type resolution |
| Any function body | Pass 2 signatures (for call resolution) |

### 5.2 Forward References and Mutual Recursion

**Forward references within Pass 1 are permitted but not validated**. If `struct A { b: B }` and `struct B { a: A }` appear in the same file, both will be registered in Pass 1 without error. The `B` name stored in `A`'s field type is just a `NamedType{"B", "", {}}` string reference. No existence check is performed at registration time.

**Mutual recursion between modules is NOT handled by Pass 0 alone**. If module X imports module Y and Y imports X, `load_native_module()` uses `loading_modules_` as a cycle guard (`env.hpp:789-790`) to prevent infinite recursion. The cycle guard means the second load of X returns without re-registering types, so any type in X that Y needs must already be registered at the time Y is loaded. This creates an implicit ordering constraint: circular imports are supported but only if the loading order satisfies dependencies.

### 5.3 Namespace Handling

Namespaces (`register_namespace_decl`, `core.cpp:278-334`) run all three internal passes sequentially before returning to the outer pass. This means a namespace is fully processed (types + functions + bodies) before the next top-level declaration is processed. This deviates from the global 3-pass ordering: namespaces are self-contained.

**Invariant 22** (core.cpp:278-334): A `namespace` block runs its own Pass 1 → Pass 2 → Pass 3 sequence inline during the outer Pass 1 (the outer `register_namespace_decl` is called from the outer Pass 1 loop). This means that by the time the outer Pass 2 begins, all declarations inside namespace blocks are fully processed including their bodies.

---

## 6. Reserved Names

### 6.1 Reserved Type Names (cannot be user-defined)

The set `RESERVED_TYPE_NAMES` (`core.cpp:46-69`) contains exactly:
- All 16 primitives: I8, I16, I32, I64, I128, U8, U16, U32, U64, U128, F32, F64, Bool, Char, Str, Unit, Never
- `StringBuilder`
- `Future`

Attempting to define a struct, enum, or type alias with any of these names emits error `T038` and the definition is silently dropped.

**Implementation note**: `Never` is in `RESERVED_TYPE_NAMES` but is NOT in `builtins_`. The `make_never()` factory exists but there is no `builtins_["Never"]` entry in `init_builtin_types()`. The `Never` type is synthesized inline when needed (e.g., for `return`, `panic`, diverging expressions), not looked up from `builtins_`.

**Implementation note**: `StringBuilder` is reserved but there is no `StructDef` for it pre-registered in `init_builtin_types()`. It is handled entirely at codegen level (like Rust's string builder pattern).

### 6.2 Reserved Behavior Names (cannot be user-defined)

The set `RESERVED_BEHAVIOR_NAMES` (`core.cpp:72-113`) contains 23 entries:
- Comparison: Eq, Ord, PartialEq, PartialOrd
- Hashing: Hash
- Display: Display, Debug
- Numeric: Numeric
- Default: Default
- Clone/Duplicate: Duplicate
- Iteration: Iterator, IntoIterator, FromIterator
- Conversion: Into, From, TryInto, TryFrom
- Indexing: Index, IndexMut
- Functions: Fn, FnMut, FnOnce
- Drop: Drop
- Sized: Sized
- Concurrency: Send
- Async: Future

**Invariant 23** (core.cpp:344-349): `register_trait_decl()` checks `RESERVED_BEHAVIOR_NAMES` before calling `env_.define_behavior()`. Reserved behavior names silently emit error `T038` and return without registering. This means the behaviors like `Drop`, `Iterator`, etc. that ARE pre-registered in `init_builtin_types()` cannot be replaced or overridden by user code.

---

## 7. Type Equality Summary

### 7.1 `types_equal` (structural, in `type.cpp`)

`types_equal(a, b)` (`type.cpp:311-440`) is the canonical equality check. It:
1. Handles null pointers (both null → true; one null → false)
2. Short-circuits on `a->id == b->id` (same pointer or same factory object)
3. Requires same variant kind (no implicit coercions)
4. Recurses structurally for compound types

This function does NOT resolve TypeVar bindings. It is suitable for comparing fully-resolved types or comparing structural types that contain no TypeVars.

### 7.2 `TypeEnv::types_match` (partial, in `env_lookups.cpp`)

`types_match(a, b)` (`env_lookups.cpp:138-166`) is a lighter equality check used only in overload resolution. It:
1. Checks `kind.index()` for variant type match
2. For `PrimitiveType`: compares `kind` enum value
3. For `NamedType`: compares only `name` (NOT `module_path`, NOT `type_args`)
4. For `RefType`: compares `is_mut` and recurses on `inner`
5. For `FuncType`: compares param count, each param, and return type
6. For all other variants: returns `false`

**Invariant 24**: `types_match` is strictly weaker than `types_equal`. `types_match(A, B)` may return `true` when `types_equal(A, B)` returns `false` (e.g., two `NamedType{"List"}` from different modules). This is intentional: `types_match` is used for overload candidate selection, where a structural sketch match is sufficient; `types_equal` is used for definitive type compatibility checks.

### 7.3 Type Variable Resolution

`TypeEnv::resolve(type)` (`env_core.cpp:65-92`) follows the TypeVar substitution chain:
1. If `type` is not a `TypeVar`, returns it unchanged
2. If `type` is a `TypeVar`, looks up `substitutions_[id]`
3. If found, recurses on the result with cycle detection via a `visited` set
4. Cycle detection: if the same TypeVar id is visited twice, returns the TypeVar unresolved

`TypeEnv::unify(a, b)` (`env_core.cpp:57-63`) is a simple one-directional assignment:
- If `a` is a TypeVar: `substitutions_[a.id] = b`
- Else if `b` is a TypeVar: `substitutions_[b.id] = a`
- Otherwise: no-op (no structural unification for compound types)

**Invariant 25** (env_core.cpp:57-63): The `unify()` method does NOT perform occurs-check. It will allow `TypeVar ?0 -> TypeVar ?0` if called as `unify(?0, ?0)`, but only if `?0` is already substituted. The cycle detection in `resolve_impl` prevents infinite loops during resolution. However, a malformed substitution chain created by unusual call sequences COULD produce incorrect types if the cycle guard triggers.

---

## 8. Known Deviations from Hindley-Milner

1. **No occurs-check in unification** (`env_core.cpp:57-63`): Standard HM unification rejects `T = Maybe[T]` (occurs check). TML's `unify()` simply records `substitutions_[T.id] = Maybe[T]` without checking if `T` appears in `Maybe[T]`. The cycle guard in `resolve_impl` prevents infinite loops but does not produce a type error. This could produce unsound behavior for certain recursive type patterns.

2. **Single-directional unification** (`env_core.cpp:57-63`): Standard HM has bidirectional unification. TML's `unify(a, b)` only records a substitution if one of `a` or `b` is a TypeVar. If both are compound types (e.g., unifying `List[I32]` with `List[T]`), unify does nothing. The actual structural matching is done at check sites, not generically.

3. **Generic parameters as `NamedType`**: Standard HM uses a separate kind for universal quantifiers. TML uses `GenericType{name}` for explicit generic parameters, but `substitute_type()` also treats `NamedType{name, "", {}}` (bare named type with no module and no args) as a type variable in the substitution map (`type.cpp:476-479`). This dual encoding is an implementation-level optimization that would need to be replicated in the self-hosted type checker to maintain compatibility.

4. **No principal types for overloaded functions**: `lookup_func()` returns the first overload when called without argument types (`env_lookups.cpp:168-177`). The type checker must call `lookup_func_overload()` with concrete argument types for correct overload resolution. If the caller uses `lookup_func()` in a context where multiple overloads exist, it silently picks the first registered one.

5. **Type variable counter is per-snapshot**: Each `snapshot()` starts with `type_var_counter_ = 0`. This means TypeVar IDs are reused across different compilation units. TypeVars should never be stored in the persistent type tables (structs_, behaviors_, etc.) — they are only valid within a single compilation unit's type-checking session.

---

## 9. Source File Cross-Reference Index

| Claim | Source | Lines |
|-------|--------|-------|
| `Type` variant list | `type.hpp` | 294-297 |
| `next_type_id` global | `type.cpp` | 36-37 |
| Factory functions assign unique IDs | `type.cpp` | 39-43 |
| `types_equal` shortcut on id | `type.cpp` | 317 |
| `types_equal` for NamedType | `type.cpp` | 329-336 |
| `types_equal` for RefType (ignores lifetime) | `type.cpp` | 339-340 |
| `types_equal` for FuncType (is_async matters) | `type.cpp` | 355-366 |
| `types_equal` for ClosureType (captures matter) | `type.cpp` | 367-386 |
| `types_equal` for TypeVar (id only) | `type.cpp` | 387-390 |
| `types_equal` for GenericType (name only) | `type.cpp` | 388-390 |
| `substitute_type` treats NamedType as type param | `type.cpp` | 476-479 |
| `TypeEnv` private field layout | `env.hpp` | 757-790 |
| TypeEnv constructor calls `init_builtins()` | `env_core.cpp` | 32-35 |
| `snapshot()` private constructor | `env_core.cpp` | 102-122 |
| `fresh_type_var()` counter | `env_core.cpp` | 51-55 |
| `unify()` single-directional | `env_core.cpp` | 57-63 |
| `resolve_impl()` cycle detection | `env_core.cpp` | 74-92 |
| `BuiltinsSnapshot::create_env()` | `builtins_cache.cpp` | 17-25 |
| `TypeChecker()` uses snapshot | `checker/core.cpp` | 191 |
| Pass 0–3 loop structure | `checker/core.cpp` | 203-255 |
| RESERVED_TYPE_NAMES | `checker/core.cpp` | 46-69 |
| RESERVED_BEHAVIOR_NAMES | `checker/core.cpp` | 72-113 |
| `T038` error for reserved names | `checker/core.cpp` | 345, 457 |
| Pass 1: struct, enum, behavior, alias | `checker/core.cpp` | 209-229 |
| Pass 2: func_decl, impl_decl | `checker/core.cpp` | 231-244 |
| Pass 3: body checking | `checker/core.cpp` | 246-255 |
| Namespace inline 3-pass | `checker/core.cpp` | 278-334 |
| `check_impl_decl()` method registration | `checker/core.cpp` | 1231-1268 |
| Specialized impl key construction | `checker/core.cpp` | 1155-1177 |
| On-demand behavior load from binary cache | `checker/core.cpp` | 1295-1327 |
| `register_impl()` in behavior_impls_ | `env_definitions.cpp` | 460-462 |
| `define_func()` appends to overload vector | `env_definitions.cpp` | 39-61 |
| `define_struct/enum/behavior` | `env_definitions.cpp` | 27-37 |
| `lookup_struct/enum/behavior` fallback to registry | `env_lookups.cpp` | 37-136 |
| `types_match` partial comparison | `env_lookups.cpp` | 138-166 |
| `lookup_func()` returns first overload | `env_lookups.cpp` | 168-177 |
| `behavior_inherits_from()` helper | `env_lookups.cpp` | 465-488 |
| `type_implements()` super-behavior traversal | `env_lookups.cpp` | 491-562 |
| Send/Sync auto-derivation for structs | `env_lookups.cpp` | 512-558 |
| Primitive type registrations | `builtins/types.cpp` | 48-68 |
| Core enum registrations | `builtins/types.cpp` | 67-107 |
| Core behavior registrations | `builtins/types.cpp` | 109-184 |
| Primitive behavior_impls | `builtins/types.cpp` | 186-248 |
| `init_builtins()` call order | `builtins/register.cpp` | 38-52 |
