# TML Type Checker Invariants — Consolidated Reference

**Version**: 1.0  
**Date**: 2026-04-06  
**Status**: Complete  
**Source audit**: `compiler/src/types/` + `compiler/include/types/`  
**Output path**: `docs/specs/typechecker-invariants.md`

---

## Purpose

This document is the authoritative reference for the invariants that the TML type checker maintains. It serves two audiences:

1. **Self-hosting implementers** — engineers writing a TML-language replacement for the C++ type checker. Every invariant here is something the TML implementation must preserve to produce a `TypeEnv` that the downstream C++ stages (HIR lowering, THIR lowering, MIR building, codegen) accept without modification.

2. **Compiler maintainers** — engineers modifying the existing C++ type checker. Invariants marked **(latent bug)** describe known-incorrect behaviour that the self-hosted checker must replicate for compatibility until a coordinated fix is made.

The section files in `.rulebook/tasks/phase12c_typechecker-invariants/specs/` contain the full source-cited audit and remain as primary references. This document consolidates findings, adds cross-references, and adds Section 6 (the self-hosting contract).

---

## Table of Contents

1. [Type Registration Invariants](#section-1-type-registration-invariants) (25 invariants)
2. [Module Resolution Invariants](#section-2-module-resolution-invariants) (23 invariants)
3. [Impl Processing Invariants](#section-3-impl-processing-invariants) (33 invariants)
4. [Body Checking and Inference Invariants](#section-4-body-checking-and-inference-invariants) (34 invariants)
5. [Cross-Cutting Invariants](#section-5-cross-cutting-invariants) (61 invariants)
6. [Self-Hosting Contract](#section-6-self-hosting-contract)
7. [Appendix A — Unified Invariant Index](#appendix-a--unified-invariant-index)
8. [Appendix B — Latent Bugs and Surprising Findings](#appendix-b--latent-bugs-and-surprising-findings)
9. [Appendix C — Known Gaps](#appendix-c--known-gaps)
10. [Appendix D — Terminology Glossary](#appendix-d--terminology-glossary)

---

## Terminology Note

| Term used in this doc | Aliases seen in section files | Meaning |
|---|---|---|
| `behavior` | trait | TML analog of a Rust trait |
| `behavior impl` | trait impl, impl block | `impl Behavior for Type { ... }` |
| `extend block` | inherent impl | `impl Type { ... }` — no `trait_type` |
| `TypeVar` | inference variable | Unresolved `?N` node in the type tree |
| `FQN` | fully qualified name | Module-qualified identifier, e.g., `core::io::Write` |
| `short name` | unqualified name | Last path segment only, e.g., `Write` |
| `snapshot` | per-file TypeEnv | A `TypeEnv` produced by `TypeEnv::snapshot()` |
| `GlobalModuleCache` | GMC, process cache | Process-scoped singleton caching library modules |
| `BuiltinsSnapshot` | builtins cache | Singleton that pre-builds a TypeEnv with all builtins |

---


---

## Section 1 — Type Registration Invariants

> **Full source audit**: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section1_type_registration.md`  
> **Source files**: `type.hpp`, `type.cpp`, `env.hpp`, `env_core.cpp`, `env_definitions.cpp`, `env_lookups.cpp`, `checker/core.cpp`, `builtins/register.cpp`, `builtins/types.cpp`, `builtins_cache.cpp`


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


---

## Section 2 — Module Resolution Invariants

> **Full source audit**: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section2_module_resolution.md`  
> **Source files**: `env_module_loading.cpp` (875 LOC), `env_module_load.cpp` (508 LOC), `env_module_load_decls.cpp` (1253 LOC), `env.hpp`, `module.hpp`, `module_binary.hpp`, `checker/core.cpp`


## Table of Contents

1. [Overview and Component Roles](#1-overview-and-component-roles)
2. [Data Structures](#2-data-structures)
3. [Module Load Sequence](#3-module-load-sequence)
4. [Filesystem Resolution Algorithm](#4-filesystem-resolution-algorithm)
5. [The Three-Layer Cache](#5-the-three-layer-cache)
6. [Import Resolution During Module Extraction](#6-import-resolution-during-module-extraction)
7. [pub use Re-Export Semantics](#7-pub-use-re-export-semantics)
8. [Circular Import Handling](#8-circular-import-handling)
9. [Visibility Rules and Enforcement Points](#9-visibility-rules-and-enforcement-points)
10. [TypeChecker Phase 0 — process_use_decl](#10-typechecker-phase-0--process_use_decl)
11. [TypeEnv State After Module Loading](#11-typeenv-state-after-module-loading)
12. [Invariant Catalogue](#12-invariant-catalogue)
13. [Surprising Findings](#13-surprising-findings)

---

## 1. Overview and Component Roles

Module resolution in TML involves three distinct files with clearly separated responsibilities:

| File | Responsibility |
|------|---------------|
| `env_module_loading.cpp` | Filesystem path resolution; cache layer decisions; dispatching to `load_module_from_file` |
| `env_module_load.cpp` | Parsing source files; directory module expansion; invoking `extract_module_declarations` |
| `env_module_load_decls.cpp` | Walking AST nodes; filling a `Module` struct with typed signatures; two-pass processing |

The entry point for all callers is `TypeEnv::load_native_module(module_path, silent)` in `env_module_loading.cpp:173`. Everything flows from there.

```
load_native_module(path)          [env_module_loading.cpp:173]
  ├── module_registry_->has_module()       # early-out if already loaded
  ├── GlobalModuleCache::get()            # in-process memory cache
  │     └── validate hash vs .tml.meta
  ├── load_module_from_cache()            # .tml.meta binary cache
  └── load_module_from_file(path, fs_path) [env_module_load.cpp:298]
        ├── loading_modules_.count()      # circular-dep guard
        ├── parse all .tml files in dir OR single file
        └── extract_module_declarations() [env_module_load_decls.cpp:352]
              ├── Pass 1: walk decls → fill Module struct
              └── Pass 2: register default behavior methods
```

---

## 2. Data Structures

### 2.1 Module struct (`module.hpp:66`)

Every loaded module is represented as a `Module` value. The struct contains:

| Field | Type | Purpose |
|-------|------|---------|
| `name` | `string` | Canonical module path (e.g., `"std::sync"`) |
| `file_path` | `string` | Filesystem path to source (for codegen re-parsing) |
| `functions` | `unordered_map<string, FuncSig>` | Public + lowlevel + generic impl methods |
| `structs` | `unordered_map<string, StructDef>` | Public structs only |
| `internal_structs` | `unordered_map<string, StructDef>` | Private structs (visible within module) |
| `enums` | `unordered_map<string, EnumDef>` | Public enums only |
| `internal_enums` | `unordered_map<string, EnumDef>` | Private enums |
| `behaviors` | `unordered_map<string, BehaviorDef>` | Public behaviors (traits) |
| `type_aliases` | `unordered_map<string, TypePtr>` | Public type aliases |
| `type_alias_generics` | `unordered_map<string, vector<string>>` | Generic params for aliases |
| `submodules` | `unordered_map<string, string>` | `pub mod` name → submodule path |
| `constants` | `unordered_map<string, ConstantInfo>` | Scalar and tuple constants |
| `classes` | `unordered_map<string, ClassDef>` | OOP class definitions |
| `interfaces` | `unordered_map<string, InterfaceDef>` | OOP interface definitions |
| `re_exports` | `vector<ReExport>` | Records of `pub use` declarations |
| `private_imports` | `vector<string>` | Module paths from private `use` declarations |
| `behavior_impls` | `unordered_map<string, vector<string>>` | type → behaviors for Drop detection |
| `source_code` | `string` | Combined preprocessed source (for modules with TML bodies) |
| `has_pure_tml_functions` | `bool` | True when module contains non-extern function bodies |

### 2.2 ModuleRegistry (`module.hpp:174`)

A per-`TypeEnv` map from module path to `Module`. Created once per compilation unit. The `TypeEnv` holds one `shared_ptr<ModuleRegistry>` (`env.hpp:782`).

### 2.3 GlobalModuleCache (`module.hpp:126`)

A process-scoped singleton holding `Module` values for library modules (`core::*`, `std::*`, `test`). Thread-safe via `shared_mutex`. Validated against source hash before use.

### 2.4 TypeEnv private fields relevant to module loading (`env.hpp:757–790`)

```cpp
std::shared_ptr<ModuleRegistry> module_registry_;    // per-TypeEnv registry
std::string current_module_path_;                    // module being compiled
std::string source_directory_;                       // for local module resolution
std::unordered_map<string, ImportedSymbol> imported_symbols_;
std::unordered_map<string, set<string>> import_conflicts_;
bool abort_on_module_error_ = true;                  // controls fatal vs silent errors
std::unordered_set<string> loading_modules_;         // cycle detection
```

---

## 3. Module Load Sequence

### 3.1 The full sequence for a cache-cold load

When a `use` statement triggers `load_native_module("std::sync", false)` with an empty caches:

1. **Duplicate check** — `module_registry_->has_module(path)` → `false`, continue.
   (`env_module_loading.cpp:179`)

2. **GlobalModuleCache check** — `GlobalModuleCache::should_cache(path)` determines whether to try the in-memory process cache. Only `core::*`, `std::*`, and `test` return true. User and local modules skip this entire block.
   (`env_module_loading.cpp:184`)

3. **Hash validation** (if GlobalModuleCache hit) — The cached `Module` is considered valid only if the source file's CRC32C hash matches the hash stored in the corresponding `.tml.meta` file. A mismatch invalidates the in-memory entry.
   (`env_module_loading.cpp:215–237`)

4. **Binary meta cache check** — `load_module_from_cache(module_path, source_path)` attempts to read a `.tml.meta` file from `build/debug/cache/meta/<module-path>.tml.meta`. The reader verifies the magic number, version, and source hash. On hit, the serialized `Module` is deserialized and placed into both `GlobalModuleCache` and the local `ModuleRegistry`.
   (`env_module_loading.cpp:310–390`)

5. **Path resolution** — `load_native_module` resolves the module path to a filesystem path. For library modules this calls `resolve_lib_module_path(lib_subdir, "src", fs_path)` which tries `<lib_root>/<lib_subdir>/src/<fs_path>.tml` and `<lib_root>/<lib_subdir>/src/<fs_path>/mod.tml` in that order.
   (`env_module_loading.cpp:146–171`)

6. **Source loading** — `load_module_from_file(module_path, fs_path)` is called.
   (`env_module_load.cpp:298`)

7. **Circular-dependency guard** — `loading_modules_.count(module_path)` prevents re-entrant loading of the same module. Returns `true` immediately if the module is already being loaded.
   (`env_module_load.cpp:309`)

8. **Parsing** — For a directory module (when `fs_path.stem() == "mod"`), all `.tml` files in the directory are parsed. For a single-file module, only that file is parsed.
   (`env_module_load.cpp:340–433`)

9. **Declaration extraction** — `extract_module_declarations(module_path, file_path, all_parsed, mod)` walks the AST and fills `mod` in two passes.
   (`env_module_load.cpp:464`, `env_module_load_decls.cpp:352`)

10. **Module registration** — `module_registry_->register_module(module_path, std::move(mod))`.
    (`env_module_load.cpp:475`)

11. **Transitive re-export loading** — For each `ReExport::source_path` recorded in the module, `load_native_module(source_path, silent=true)` is called recursively.
    (`env_module_load.cpp:482–484`)

12. **Transitive private import loading** — For each private import path, `load_native_module` is attempted. If it fails, the last `::` segment is stripped and the parent path is tried, since the path may be a symbol path (`core::option::Maybe`) rather than a module path (`core::option`).
    (`env_module_load.cpp:493–502`)

### 3.2 Declaration extraction within extract_module_declarations

`extract_module_declarations` performs two passes over `all_parsed`:

**Pass 1** (`env_module_load_decls.cpp:359–1071`) — Walks every declaration in every parsed file:

- `FuncDecl`: registered if `vis == Public`, or `extern_abi.has_value()`, or `is_unsafe`. All three conditions bypass the visibility filter to ensure codegen can emit `declare` statements. Qualified name is the bare function name.

- `StructDecl`: public structs go to `mod.structs`, private to `mod.internal_structs`. Both are populated.

- `EnumDecl`: same split as structs. Public → `mod.enums`, private → `mod.internal_enums`.

- `ImplDecl`: extracts methods under the qualified key `TypeName::method`. Additionally, for specialized impls (e.g., `impl[T] Pin[Heap[T]]`), a discriminated key `TypeName[Heap]::method` is also registered. The choice between private method inclusion and exclusion depends on `is_generic_impl || is_behavior_impl`: if either is true, all methods are included regardless of visibility.

- `ConstDecl` (module-level): only if a compile-time scalar or tuple can be extracted.

- `InterfaceDecl`, `ClassDecl`: registered only for `vis == Public`.

- `TypeAliasDecl`: registered only for `vis == Public`.

- `ModDecl`: `pub mod foo` inserts `foo → "parent::foo"` into `mod.submodules`.

- `UseDecl`: If `vis == Public`, creates a `ReExport` entry and appends to `mod.re_exports`. If private, appends the module path to `mod.private_imports`. For both visibility levels, the referenced module is eagerly loaded if it is from `mod.tml` (`is_from_mod_file == true`) or is an intra-module reference. Sibling `.tml` files (not `mod.tml`) do NOT eagerly load their external dependencies.

- `TraitDecl`: registered only for `vis == Public`.

**Pass 2** (`env_module_load_decls.cpp:1073–1158`) — For each `ImplDecl` that has a `trait_type` (i.e., is a behavior impl), looks up the `BehaviorDef` (first in `mod.behaviors`, then in all registered modules) and registers default behavior methods not overridden by the impl. This ensures types that rely on defaults show the correct method set.

After both passes, a third sweep determines `mod.has_pure_tml_functions` and collects `mod.source_code`.

---

## 4. Filesystem Resolution Algorithm

### 4.1 Module path to filesystem path translation

The `::` separator in module paths maps to `/` in filesystem paths. The translation is performed before any filesystem probe:

```
"core::str"          → "str"           → lib/core/src/str.tml or lib/core/src/str/mod.tml
"std::collections"   → "collections"   → lib/std/src/collections.tml or .../mod.tml
"std::http::server"  → "http/server"   → lib/std/src/http/server.tml or .../mod.tml
```

(`env_module_loading.cpp:626–640` for core, `694–704` for std, `460–475` for test)

### 4.2 Resolution priority order (library modules)

For `core::*`, `std::*`, `test::*`, `backtrace::*`:

1. **Path resolution cache** — `s_resolved_paths[module_path]` (process-scoped, never invalidated within a process). Hit → use cached path directly.
   (`env_module_loading.cpp:116–123`)

2. **Known-not-found cache** — `s_not_found_paths` prevents repeated filesystem probes for paths already known to not exist.
   (`env_module_loading.cpp:126–129`)

3. **Cached library root (2 probes)** — `resolve_lib_module_path(lib_subdir, "src", fs_path)` tries exactly two paths: `<lib_root>/<lib_subdir>/src/<fs_path>.tml` and `<lib_root>/<lib_subdir>/src/<fs_path>/mod.tml`.
   (`env_module_loading.cpp:146–171`)

4. **Full fallback search (10–12 probes)** — A vector of relative and absolute paths is tried in order. Paths are relative to CWD, two levels up (for `build/debug/bin`), and hardcoded to `F:/Node/hivellm/tml/lib/`.
   (`env_module_loading.cpp:644–658` for core example)

### 4.3 Local module resolution

Modules with paths that do not begin with `core::`, `std::`, `test::`, or `backtrace::` are resolved as local or external packages. The resolution sequence is:

1. **External package detection** — If `lib_root/<package_name>/src/` exists, the path is treated as an external package and resolved within that tree. (`env_module_loading.cpp:751–824`)

2. **Source-directory-relative** — `source_directory_ / (fs_path + ".tml")` and `source_directory_ / fs_path / "mod.tml"`. (`env_module_loading.cpp:840–855`)

3. **CWD-relative** — Same patterns relative to `std::filesystem::current_path()`. (`env_module_loading.cpp:858–869`)

### 4.4 Windows case-sensitivity workaround

On Windows, `std::filesystem::exists()` is case-insensitive (NTFS). This means `lib/std/src/List.tml` would match when resolving `std::collections::list` (expecting `list.tml`). The helper `exists_case_sensitive` iterates the parent directory and verifies the filename byte-for-byte against the expected name.
(`env_module_loading.cpp:55–67`)

This has a correctness consequence: `std::collections::List` resolves to `List.tml` (a symbol, not a file), which correctly fails, causing `std::collections` to be loaded as the parent directory module. Without the case check, `List.tml` would be matched and `std::collections`'s sibling files (including `behaviors.tml` defining `ListIter`) would be skipped.

---

## 5. The Three-Layer Cache

### 5.1 Layer 1: ModuleRegistry (per-TypeEnv, compile-unit scoped)

The `ModuleRegistry` held by a `TypeEnv` is checked first. It is populated exclusively by `register_module`. It never evicts entries. It is destroyed when the `TypeEnv` goes out of scope at the end of the compilation unit.

### 5.2 Layer 2: GlobalModuleCache (process-scoped singleton)

The `GlobalModuleCache` stores `Module` values for library modules across the lifetime of the compiler process. It is the primary speedup for multi-file builds and test suites: once a library module is loaded by one test file's `TypeEnv`, subsequent test files find it in the process cache without filesystem access.

The cache is read with `shared_lock` and written with `unique_lock` (`module.hpp:165–166`).

Hash validation occurs before a cached entry is used. The `Module.source_code` stored in the cache contains preprocessed source, not raw source. If the source hash changes between builds (meta file regenerated), the in-process cache is bypassed.
(`env_module_loading.cpp:193–238`)

### 5.3 Layer 3: Binary meta cache (.tml.meta files)

Binary `.tml.meta` files are written once per library module compilation and read on subsequent runs. The format is:

```
Header (24 bytes):
  [0..4)    magic: 0x544D4D54 ("TMMT")
  [4..6)    version_major: 8
  [6..8)    version_minor: 0
  [8..16)   source_hash: u64 (CRC32C of source files)
  [16..24)  timestamp: u64
```

(`module_binary.hpp:9–27`)

The current format version is **8** (incremented when Module struct layout changes). Older cache files from a version mismatch are silently rejected.
(`module_binary.hpp:55`)

Cache path computation: `"core::mem"` → `<build_root>/cache/meta/core/mem.tml.meta`. The build root is discovered by walking up from CWD looking for `build/debug/` or `build/release/` structures.
(`module_binary.hpp:74–79`)

### 5.4 Cache relationship to behavior_impls

When a module is loaded from either the binary cache or GlobalModuleCache, its `behavior_impls` map is re-registered into the current `TypeEnv` via `register_impl`. This is essential for `is_trivially_destructible()` to correctly identify `Drop` implementations on imported library types.

If the cache entry predates the addition of `behavior_impls` (old format), a fallback scans `module.functions` for names ending in `::drop` and infers `Drop` implementations from them.
(`env_module_loading.cpp:256–272`, `359–367`)

---

## 6. Import Resolution During Module Extraction

### 6.1 When imports are resolved relative to declarations

Within `extract_module_declarations`, `UseDecl` nodes are processed in Pass 1 along with all other declarations. There is no separate import-first phase within this function. This means a `use` statement appearing after a `struct` declaration in source order is processed after the struct. However, because `extract_module_declarations` only populates the `Module` struct (not a `TypeEnv`), the ordering within a single module file has no effect on symbol availability.

The module-level type checker (`checker/core.cpp:193–261`) does process `UseDecl` before all other declarations (Phase 0 precedes Phase 1).

### 6.2 Eager loading of dependencies

During Pass 1 of `extract_module_declarations`, `UseDecl` nodes trigger `load_native_module` only under specific conditions:

- **Always loaded** if the declaration is from `mod.tml` (`is_from_mod_file == true`).
- **Always loaded** if the import path is intra-module (`use_path.find(module_path + "::") == 0 || use_path == module_path`).
- **Not loaded** if the declaration is from a sibling `.tml` file and the import is external.

This optimization avoids loading `std::zlib` just because `encoding.tml` (a sibling file in `std::http`) imports it, since `encoding.tml`'s types are only needed when the file is explicitly compiled.
(`env_module_load_decls.cpp:926–955`)

### 6.3 Two-step module resolution for symbol paths

When a `use` path like `core::option::Maybe` is processed, the code first tries to load `core::option::Maybe` as a module path. When this fails (no file `option/Maybe.tml`), the last `::` segment is stripped and `core::option` is loaded as the base module.
(`env_module_load.cpp:493–501`, `env_module_load_decls.cpp:939–953`)

### 6.4 Glob imports (use_decl.is_glob)

In `extract_module_declarations`, glob imports (`use foo::*`) create a `ReExport` with `is_glob = true` and an empty `symbols` vector. No special treatment is applied during extraction; the expansion happens later when `TypeEnv::import_all_from` is called.

In the type checker's `process_use_decl`, `env_.import_all_from(module_path)` is called after loading the module, which copies every public symbol from the module into `imported_symbols_`.

---

## 7. pub use Re-Export Semantics

### 7.1 ReExport struct layout

```cpp
struct ReExport {
    string source_path;          // Resolved module path for the source
    bool is_glob;                // True for pub use foo::*
    vector<string> symbols;      // Empty for glob; specific names otherwise
    optional<string> alias;      // For pub use foo as bar
};
```
(`module.hpp:58–63`)

### 7.2 Single-symbol re-export extraction

When `pub use foo::bar::SymbolName` is encountered (no `{}` group, no `*`), the code splits off the last `::` segment:

```cpp
re_source_path = use_path without last segment;  // "foo::bar"
re_symbols = {symbol_name};                      // {"SymbolName"}
```

(`env_module_load_decls.cpp:967–974`)

This means the `ReExport.source_path` is the module containing the symbol, not the full import path.

### 7.3 Re-export chain loading (transitive)

After a module is registered, `load_module_from_file` loads every `re_export.source_path` recursively:

```cpp
for (const auto& source_path : re_export_sources) {
    load_native_module(source_path, silent=true);
}
```
(`env_module_load.cpp:482–484`)

The same pattern occurs for binary cache hits (`env_module_loading.cpp:276–280`) and GlobalModuleCache hits (`env_module_loading.cpp:278–280`). All three load paths are symmetric with respect to re-export chain loading.

### 7.4 Re-export resolution in ModuleRegistry

`ModuleRegistry::lookup_function_impl`, `lookup_struct_impl`, `lookup_enum_impl`, `lookup_behavior_impl`, and `lookup_constant_impl` all follow `re_exports` chains. Each uses an `unordered_set<string>& visited` parameter to prevent infinite recursion in diamond re-export patterns.
(`module.hpp:255–277`)

This means a module consumer can access `foo::Bar` even though `Bar` was originally defined in `foo::internal::Bar`, as long as `foo` has `pub use foo::internal::Bar`. The lookup traverses the re-export chain to find the original definition.

### 7.5 Re-export via process_use_decl (type checker level)

In `process_use_decl` (`checker/core.cpp:586–607`), when a symbol group import (`use mod::{A, B}`) is processed, the code additionally loads all re-export source modules that might contain any of the requested symbols. This is a proactive load to ensure that cross-module re-exported enums and constants are available when their types are resolved.

---

## 8. Circular Import Handling

### 8.1 The loading_modules_ guard

`loading_modules_` is an `unordered_set<string>` stored on `TypeEnv` (`env.hpp:789–790`). Before loading a module from file, `load_module_from_file` inserts the module path into this set. If a recursive `load_native_module` call encounters a path already in the set, it returns `true` immediately without loading.

```cpp
if (loading_modules_.count(module_path) > 0) {
    TML_DEBUG_LN("[MODULE] Skipping circular dependency: " << module_path);
    return true; // Return true to allow compilation to proceed
}
loading_modules_.insert(module_path);
```
(`env_module_load.cpp:309–315`)

The RAII guard `LoadingGuard` removes the path from the set on any return path before registration. After successful registration, `loading_guard.mark_completed()` prevents the guard from removing the entry, and the entry is then manually erased.
(`env_module_load.cpp:318–331`, `477–479`)

### 8.2 Behaviour during a circular detection event

When a circular dependency is detected, the module is not yet registered. The `load_native_module` call returns `true` to the caller, signaling success. This is intentional: it prevents the compilation from aborting, since the circular dependency will be resolved once the outer load completes.

This means that code which depends on a circularly-referenced module must not require that module's types to be available during the loading of the outer module. The type checker body-checking phase (Phase 3) runs after all imports complete (Phase 0), so this is safe in the two-pass architecture.

### 8.3 No cycle detection at the binary cache level

The binary meta cache and GlobalModuleCache paths do not use `loading_modules_`. Since these paths load pre-serialized `Module` structs (no recursive parsing), they cannot trigger circular dependencies. Only `load_module_from_file` can trigger re-entrant loading.

---

## 9. Visibility Rules and Enforcement Points

### 9.1 Visibility values

The parser defines:

```cpp
enum class Visibility { Public, Private };       // for module-level declarations
enum class MemberVisibility { Private, Protected, Public };  // for class members
```

In all module extraction code, the only test for public visibility is `vis == parser::Visibility::Public`.

### 9.2 Per-declaration-kind enforcement

The table below maps declaration kinds to the visibility enforcement applied in `extract_module_declarations`:

| Declaration | Visibility check | Private result |
|-------------|-----------------|----------------|
| `FuncDecl` | `vis == Public` OR `extern_abi` OR `is_unsafe` | Not registered |
| `StructDecl` | `vis == Public` | Goes to `internal_structs` |
| `EnumDecl` | `vis == Public` | Goes to `internal_enums` |
| `ImplDecl` method | `vis == Public` OR `is_behavior_impl` OR `is_generic_impl` | Not registered |
| `ImplDecl` constant | `vis == Public` | Not registered |
| `InterfaceDecl` | `vis == Public` | Not registered |
| `ClassDecl` | `vis == Public` | Not registered |
| `TypeAliasDecl` | `vis == Public` | Not registered |
| `ModDecl` | `vis == Public` | Not registered |
| `UseDecl` | `vis == Public` → re-export; private → private_imports | |
| `TraitDecl` (behavior) | `vis == Public` | Not registered |

(`env_module_load_decls.cpp:363–1069` — per-branch analysis)

### 9.3 Asymmetry: internal structs and enums are always populated

Public structs and enums go to `mod.structs` / `mod.enums`. Private structs and enums go to `mod.internal_structs` / `mod.internal_enums`. Both maps are always populated regardless of visibility.

The `internal_structs` and `internal_enums` maps are present so that impl methods within the same module can use private types without needing them in the public API surface.

### 9.4 pub(crate) is not implemented

The parser recognizes `pub(crate)` as a syntax extension, but `extract_module_declarations` only tests for `parser::Visibility::Public`. There is no separate `Crate` variant in the enum. A `pub(crate)` item is either parsed as `Public` or rejected at the parser level. The type environment has no representation of crate-scoped visibility distinct from fully public.

### 9.5 Enforcement at import time

When `TypeEnv::import_symbol` and `TypeEnv::import_all_from` are called during Phase 0 of the type checker, they consult the `Module` struct populated by `extract_module_declarations`. Since private types are absent from `Module::structs`, `Module::enums`, etc., they cannot be imported from outside the module. The enforcement is structural: private items are simply not present in the public symbol tables.

The exception is `internal_structs` / `internal_enums`: these are stored in the module but are not consulted by `lookup_struct` / `lookup_enum` externally (lookup follows `ModuleRegistry::lookup_struct_impl` which queries `module.structs`, not `module.internal_structs`).

---

## 10. TypeChecker Phase 0 — process_use_decl

### 10.1 Phase ordering in check_module

`TypeChecker::check_module` performs exactly four passes over the declarations:
(`checker/core.cpp:193–261`)

```
Pass 0: UseDecl → process_use_decl()
Pass 1: StructDecl, EnumDecl, TraitDecl, TypeAliasDecl, InterfaceDecl, ClassDecl
        → register_*_decl()
Pass 2: FuncDecl, ImplDecl, ConstDecl, ClassDecl, InterfaceDecl
        → check_func_decl(), check_impl_decl(), check_const_decl()
Pass 3: FuncDecl, ImplDecl, ClassDecl body checking
        → check_func_body(), check_impl_body(), check_class_body()
```

Pass 0 is always complete before Pass 1 begins. This means all modules referenced by `use` declarations are loaded (and their `TypeEnv` symbols imported) before any type registration for the current module occurs.

### 10.2 process_use_decl resolution algorithm

`process_use_decl` in `checker/core.cpp:472` handles three import forms:

**Glob imports** (`use mod::*`):
1. `load_native_module(module_path, silent=true)`
2. If not found, try `try_resolve_via_parent_reexports(module_path)` — walks parent module's `re_exports` for a matching source path suffix.
3. If found, `env_.import_all_from(module_path)`.
4. If still not found, emit error `T027`.

**Symbol group imports** (`use mod::{A, B, C}`):
1. `load_native_module(module_path, silent=true)`
2. If not found, try parent re-export resolution.
3. If found, load all re-export source modules for the requested symbols.
4. Call `env_.import_symbol(module_path, symbol, nullopt)` for each symbol.

**Single path imports** (`use foo::bar::Baz`):
1. Try `load_native_module(full_path)` — treats the full path as a module.
2. If not found, strip last segment and try `load_native_module(base_path)` — treats last segment as a symbol name.
3. If base not found, try grandparent re-export resolution (2-level re-export following).
4. Import the symbol (last segment) from the resolved module.

### 10.3 Re-export resolution in process_use_decl

`try_resolve_via_parent_reexports` in `checker/core.cpp:490–541` implements a fallback for paths like `std::http::chunked` that do not correspond to filesystem files but are re-exported from `std::http`. It:

1. Loads the parent module (`std::http`).
2. Scans its `re_exports` for a `source_path` ending with `::chunked`.
3. Loads and returns the actual source module.

A two-level variant (searching the grandparent) handles cases where even the parent path doesn't exist on disk but is reachable via grandparent re-exports.
(`checker/core.cpp:637–680`)

---

## 11. TypeEnv State After Module Loading Completes

After `check_module` returns (the full 4-pass sequence is complete), the following state is guaranteed:

### 11.1 Module registry

Every module path referenced by a `use` statement in the source being compiled, and all transitive re-export source paths from those modules, are registered in `module_registry_`. The registry is complete — no lazy loading occurs during type-checking phases after Phase 0.

### 11.2 TypeEnv type tables

After Pass 1 (`register_*_decl`):
- `structs_`, `enums_`, `behaviors_`, `type_aliases_` contain all declarations from the currently-compiled module.
- They do NOT contain declarations from imported modules (those remain in the `ModuleRegistry`).

After Pass 2 (`check_impl_decl`, `check_func_decl`):
- `functions_` contains signatures for all functions and methods declared in the current module.
- `behavior_impls_` contains all `(type, behavior)` pairs registered via `register_impl` during extraction of the current module AND all imported modules.

### 11.3 imported_symbols_

`imported_symbols_` maps local symbol names to `ImportedSymbol` (original name + module path). It is populated by `import_symbol` and `import_all_from` during Phase 0. After Phase 0, the set is complete.

The lookup chain for resolving a name during body type checking is:
1. Current lexical scope chain (local variables).
2. `imported_symbols_` (module-level imports).
3. `structs_` / `enums_` / `behaviors_` / `functions_` (current module's own declarations).
4. `module_registry_` (cross-module lookup for qualified paths).

### 11.4 substitutions_ (type inference)

`substitutions_` is populated incrementally during Pass 3 (body checking) via `unify()`. It contains no entries before Pass 3 begins. The `resolve()` method follows the substitution chain to ground a type variable.

### 11.5 loading_modules_

After a successful `check_module` call, `loading_modules_` should be empty. All modules loaded during Phase 0 were inserted and then removed by the RAII guard. A non-empty `loading_modules_` at the end of compilation indicates a circular import that was silently swallowed.

---

## 12. Invariant Catalogue

This section enumerates all invariants discovered during the audit, each traceable to a specific file and line range.

---

**INV-01**: A module path is registered in `ModuleRegistry` at most once per `TypeEnv`. The check `module_registry_->has_module(path)` is performed before any loading work begins and returns `true` immediately on a hit.

*Source*: `env_module_loading.cpp:179`, `env_module_load.cpp:304`

---

**INV-02**: A module path appears in `loading_modules_` for the duration of its loading and is removed (by RAII guard or manual erase) before the caller can observe its absence. A path present in `loading_modules_` is not present in `module_registry_` and vice versa.

*Source*: `env_module_load.cpp:309–331`, `477–479`

---

**INV-03**: `GlobalModuleCache::should_cache(path)` returns `true` if and only if the path begins with `"core::"`, `"std::"`, or equals `"test"` or begins with `"test::"`. User-defined and local modules are never placed in the GlobalModuleCache.

*Source*: `env_module_loading.cpp:184` (usage pattern), `module.hpp:155` (static method declaration)

---

**INV-04**: When a module is loaded from GlobalModuleCache or binary meta cache, its `behavior_impls` map is re-registered into the current `TypeEnv` before the module is registered into `ModuleRegistry`. This means `TypeEnv::type_implements` returns correct results for library types as soon as the module is accessed, not after a separate registration step.

*Source*: `env_module_loading.cpp:256–260` (GlobalModuleCache path), `352–356` (binary cache path)

---

**INV-05**: A binary `.tml.meta` cache file is accepted only when: (a) the magic number matches `0x544D4D54`, (b) the version major equals `8`, and (c) the CRC32C hash of the current source files equals the hash stored in the header. Any mismatch causes the cache file to be ignored and the module to be loaded from source.

*Source*: `module_binary.hpp:46–56`, `env_module_loading.cpp:219–233`

---

**INV-06**: The filesystem path resolution cache (`s_resolved_paths`, `s_not_found_paths`) is process-scoped and never invalidated during the lifetime of the compiler process. A path cached as "not found" will never be retried even if the file is created mid-process.

*Source*: `env_module_loading.cpp:74–141` (static storage), `606–609` (not-found cache population example for test modules)

---

**INV-07**: Directory modules (modules with `mod.tml`) parse ALL `.tml` files in the directory, not only those declared with `pub mod`. Parse errors in individual sibling files do not abort loading if `abort_on_module_error_ == false`. Declarations from all successfully-parsed files are registered.

*Source*: `env_module_load.cpp:340–392`

---

**INV-08**: Sibling `.tml` files (not `mod.tml`) do NOT eagerly load their external `use` dependencies. Only `mod.tml` and intra-module `use` paths trigger eager loading. This prevents transitive dependency bloat.

*Source*: `env_module_load_decls.cpp:926–930` (the `is_from_mod_file || is_intra_module` condition)

---

**INV-09**: Private structs and enums are stored in `Module::internal_structs` and `Module::internal_enums`. They are not present in `Module::structs` and `Module::enums`. The `ModuleRegistry::lookup_struct` chain does not consult `internal_structs`, so private types cannot be accessed from outside their module.

*Source*: `env_module_load_decls.cpp:445–453` (struct), `493–501` (enum), `module.hpp:73–76` (field declarations)

---

**INV-10**: Methods in generic impl blocks (`!impl_decl.generics.empty()`) are always registered regardless of their declared visibility. Methods in non-generic, non-behavior impl blocks are registered only if `vis == Public`.

*Source*: `env_module_load_decls.cpp:557–561`

---

**INV-11**: For specialized impls (e.g., `impl[T] Pin[Heap[T]]`), two entries are created in `Module::functions`: one under the base key (`Pin::method`) and one under the discriminated key (`Pin[Heap]::method`). The base key may be overwritten by later impl registrations for the same type.

*Source*: `env_module_load_decls.cpp:643–651`

---

**INV-12**: Default behavior methods are only registered in Pass 2 of `extract_module_declarations`, after Pass 1 has populated `mod.behaviors`. A default method is registered only if the impl block does not already provide it (name-based check in `impl_method_names`). Default methods are registered under `TypeName::method` in `mod.functions`.

*Source*: `env_module_load_decls.cpp:1073–1156`

---

**INV-13**: The `TypeEnv::check_module` (type-checker) processes `UseDecl` in Phase 0 before any type registration. All modules referenced by `use` declarations are loaded (ModuleRegistry-registered) and their symbols imported into `imported_symbols_` before any `struct`, `enum`, or `behavior` from the current module is registered.

*Source*: `checker/core.cpp:203–261`

---

**INV-14**: When `load_native_module` is called with `silent=false` and the module is not found, an error is logged. When called with `silent=true`, no error is logged. The `abort_on_module_error_` flag controls whether parse errors during directory module loading abort the load or continue with successfully-parsed files.

*Source*: `env_module_loading.cpp:502–507` (test silent), `env_module_load.cpp:396–429` (abort_on_module_error_ branch)

---

**INV-15**: After `load_module_from_file` registers a module, it loads all `re_exports[i].source_path` modules and all `private_imports[i]` modules recursively. Both are loaded with `silent=true`. This makes re-export chains and private transitive dependencies available in the local `ModuleRegistry` without requiring explicit `use` statements from the consumer.

*Source*: `env_module_load.cpp:481–503`

---

**INV-16**: Re-export source path loading is symmetric across all three load paths (GlobalModuleCache hit, binary meta cache hit, source file load). All three paths execute the same re-export and private-import loading logic after registering the module.

*Source*: `env_module_loading.cpp:244–295` (GlobalModuleCache), `343–388` (binary cache), `env_module_load.cpp:481–503` (source file)

---

**INV-17**: `Module::has_pure_tml_functions` is set to `true` if the module contains any function with a body, any behavior impl method with a body, any OOP class method with a body, or any public constant. When `false`, the module contains only `@extern` declarations and no codegen source is needed.

*Source*: `env_module_load_decls.cpp:1177–1224`

---

**INV-18**: `Module::source_code` is populated only when `has_pure_tml_functions == true`. It contains the concatenation of all parsed files' preprocessed source code (not raw source). Preprocessor directives have already been evaluated; the stored string is ready for re-lexing by the codegen.

*Source*: `env_module_load_decls.cpp:1246–1250`

---

**INV-19**: The `UseDecl` path in `extract_module_declarations` handles relative imports by prepending the current module path when the path does not start with `core::`, `std::`, or `test`. This means `use helpers` inside `myapp::utils` resolves to `myapp::utils::helpers`.

*Source*: `env_module_load_decls.cpp:909–913`

---

**INV-20**: `process_use_decl` in the type checker implements two-level re-export resolution: if a direct module path is not found on disk, it searches the parent module's `re_exports` (and for three-segment paths, also the grandparent's `re_exports`). This allows logical module paths like `std::http::chunked` to be used even when the file is physically at `std::http::encoding::chunked`.

*Source*: `checker/core.cpp:490–541` (parent), `637–680` (grandparent)

---

**INV-21**: The type checker's `process_use_decl` emits a `T027` error ("Module not found") only for glob imports and grouped symbol imports when the module cannot be resolved through any fallback. Single-path imports (`use foo::bar::Baz`) do not emit `T027` — they silently fail if the module is not found, because the last segment is assumed to be a symbol rather than a module path.

*Source*: `checker/core.cpp:554–558` (glob error), `578–582` (group error); compare with `617–693` (single path, no error emitted)

---

**INV-22**: The `s_lib_root` path is resolved once per process by examining several candidate directories. Once resolved (or determined to be unresolvable), the result is cached in `s_lib_root` behind `s_lib_root_resolved`. All subsequent calls return the cached value. A failed resolution returns an empty string, causing all resolution to fall back to the full relative-path search.

*Source*: `env_module_loading.cpp:83–113`

---

**INV-23**: `FuncSig::is_lowlevel` is set from `parser::FuncDecl::is_unsafe`. In TML, `lowlevel` blocks are represented as `is_unsafe` in the AST. The type environment stores this flag so that codegen can distinguish between safe and lowlevel functions without re-examining the source.

*Source*: `env_module_load_decls.cpp:408`, `639`

---

## 13. Surprising Findings

### 13.1 Duplicate definitions at translation unit scope

`ParsedModuleFile`, `get_tml_type_name`, `format_float_const`, `try_extract_scalar_const_value`, `try_extract_module_const_value`, and `ParseResult` are defined once in `env_module_load.cpp` and again in `env_module_load_decls.cpp`. These are static (internal linkage) or file-local struct definitions. They are binary-identical copies. The design relies on the C++ ODR exemption for `static` functions and the fact that the structs have the same layout in both translation units. This is fragile — a future change to one copy that is not mirrored in the other will introduce silent divergence.

*Source*: `env_module_load.cpp:46–235`, `env_module_load_decls.cpp:13–204`

### 13.2 Base key collision for multiple specialized impls

When multiple impl blocks exist for the same base type (e.g., `impl[T] Pin[ref T]` and `impl[T] Pin[Heap[T]]`), both register methods under `Pin::method` in `mod.functions`. The second registration overwrites the first (plain `unordered_map::operator[]`). Only the specialized keys (`Pin[ref]::method`, `Pin[Heap]::method`) survive both impls.

A callee looking up `Pin::method` by its base key will find the last-written impl's signature. If the caller is trying to select the correct impl based on the receiver type, it must use the discriminated key. This is not documented in the code — callers that use the base key for dispatch may silently pick the wrong impl.

*Source*: `env_module_load_decls.cpp:643–650`

### 13.3 abort_on_module_error_ is temporarily disabled during import loading

Within `extract_module_declarations`, when a `UseDecl` triggers `load_native_module`, the code saves and restores `abort_on_module_error_`:

```cpp
bool prev_abort_on_error = abort_on_module_error_;
abort_on_module_error_ = false;
bool loaded = load_native_module(use_path, silent=true);
abort_on_module_error_ = prev_abort_on_error;
```

This means that even in normal compilation mode (`abort_on_module_error_ == true`), a module path that fails to parse during import resolution does not trigger an error. The failure is silently swallowed. The error is only surfaced if the caller explicitly requests the symbol that failed to load.

*Source*: `env_module_load_decls.cpp:933–955`

### 13.4 source_code stores preprocessed output, not raw source

`Module::source_code` is populated from `parsed_file.source_code`, which is set to `pp_result.output` in `parse_tml_file`. The preprocessor has already evaluated all `#if` directives. When codegen re-parses this source, it receives the post-preprocessor form. This is intentional (avoids running the preprocessor twice) but means the stored source code may differ from what a developer sees in the file.

*Source*: `env_module_load.cpp:288–290` (source_code = pp_result.output), `env_module_load_decls.cpp:1248`

### 13.5 Library root hardcoded fallback

`find_lib_root` includes a hardcoded absolute path `"F:/Node/hivellm/tml/lib"` as a fallback candidate. This is a developer-machine-specific path that will fail on any other machine. If the CWD-relative paths also fail (e.g., because the binary is run from a non-standard directory), the library root resolution falls back to the full relative-path search (10–12 probes per module). The hardcoded path is harmless on other machines but it reveals an assumption about where development occurs.

*Source*: `env_module_loading.cpp:97`

### 13.6 Circular detection returns true, not false

When a circular import is detected, `load_module_from_file` returns `true` (`env_module_load.cpp:311`). The caller (`load_native_module`) propagates this `true` back to the type checker's Phase 0. The type checker receives `true` (success) for a module that is not yet registered and has no symbols. If a symbol from the circularly-referenced module is needed before the outer load completes, the lookup will fail silently with no error — the symbol simply won't be found.

This is an inherent limitation of the single-registry architecture: circular dependencies are "resolved" by pretending success, but partial module availability during loading is not tracked.

*Source*: `env_module_load.cpp:309–312`

### 13.7 Fallback type for unknown types is I32, not an error

`resolve_simple_type` returns `make_primitive(PrimitiveKind::I32)` for type expressions it cannot resolve. There is even a duplicate `return` statement suggesting a copy-paste:

```cpp
return make_primitive(PrimitiveKind::I32);
TML_DEBUG_LN("[MODULE] Warning: Could not resolve type, using I32 as fallback"); // unreachable
return make_primitive(PrimitiveKind::I32);                                        // unreachable
```

Any type that has no explicit mapping in `resolve_simple_type` silently becomes `I32`. This affects the method signatures stored in `Module::functions` for imported modules, and consequently affects type inference and codegen for those methods.

*Source*: `env_module_load_decls.cpp:344–349`

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Invariants documented | 23 |
| Files analysed | 6 (3 primary + env.hpp + module.hpp + module_binary.hpp + checker/core.cpp) |
| Surprising findings | 7 |
| Approximate page count | ~18 pages |

---

*Document written by spec-engineer agent, 2026-04-06. Read-only audit — no code changes.*


---

## Section 3 — Impl Processing Invariants

> **Full source audit**: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section3_impl_processing.md`  
> **Source files**: `checker/core_oop.cpp` (1067 LOC), `checker/decl_struct.cpp` (1207 LOC), `env_lookups.cpp` (1265 LOC), `env_definitions.cpp`, `env.hpp`, `checker/core.cpp` (impl sections), `env_module_load_decls.cpp` (impl sections)


## Table of Contents

1. [Definitions and Data Model](#1-definitions-and-data-model)
2. [Behavior Impl Registration](#2-behavior-impl-registration)
3. [Method Registration — Naming Convention](#3-method-registration--naming-convention)
4. [Method Resolution Order](#4-method-resolution-order)
5. [Coherence Rules — Which Impl Wins](#5-coherence-rules--which-impl-wins)
6. [`extend` Block Semantics](#6-extend-block-semantics)
7. [Struct and Enum Field Lookup Algorithms](#7-struct-and-enum-field-lookup-algorithms)
8. [Generic Impl Matching](#8-generic-impl-matching)
9. [Known Bug — Short-Name vs FQN Keying](#9-known-bug--short-name-vs-fqn-keying)
10. [Invariant Summary](#10-invariant-summary)

---

## 1. Definitions and Data Model

### 1.1 Key TypeEnv Fields

`TypeEnv` (declared in `compiler/include/types/env.hpp`) maintains the following
maps that are directly involved in impl processing:

```
structs_          : unordered_map<string, StructDef>
enums_            : unordered_map<string, EnumDef>
behaviors_        : unordered_map<string, BehaviorDef>
functions_        : unordered_map<string, vector<FuncSig>>
behavior_impls_   : unordered_map<string, vector<string>>
classes_          : unordered_map<string, ClassDef>
interfaces_       : unordered_map<string, InterfaceDef>
class_interfaces_ : unordered_map<string, vector<string>>
```

`env.hpp:758–774`

All maps are keyed by **short name** (unqualified identifier), not by FQN. This is
the root cause of the known name-collision bug described in Section 9.

### 1.2 Terminology

| Term | Meaning |
|------|---------|
| **behavior** | TML analog of a Rust trait. Declared with `behavior Name { ... }`. |
| **impl block** | `impl BehaviorName for TypeName { ... }` — provides behavior impl. |
| **extend block** | `impl TypeName { ... }` — inherent methods, no `trait_type`. |
| **behavior_impls_** | Maps type name → list of behavior names that type implements. |
| **functions_** | Maps qualified method name (`Type::method`) → list of FuncSig overloads. |
| **BehaviorDef** | Struct holding method signatures, super behaviors, default methods. |
| **FuncSig** | Struct holding name, params, return_type, type_params, is_async. |

### 1.3 Two-Pass Module Loading

Impls in imported modules are processed by `env_module_load_decls.cpp` in a
**two-pass sequence**:

- **Pass 1** (`env_module_load_decls.cpp:503–1069`): For each `ImplDecl` in the
  parsed file, extract the type name from `self_type`, the behavior name from
  `trait_type.path.segments.back()`, and call `register_impl(type_name, behavior_name)`.
  Also registers method FuncSigs into the module's `functions` map.
  Behavior definitions (`BehaviorDecl`) are stored into `mod.behaviors[trait_decl.name]`
  at `env_module_load_decls.cpp:1066`.

- **Pass 2** (`env_module_load_decls.cpp:1073+`): Processes default method
  registrations after behaviors are fully populated from Pass 1.

**Invariant I-1**: When the type checker processes an `impl BehaviorName for TypeName`
block, `BehaviorDef` for `BehaviorName` must already be registered in `behaviors_`
(or discoverable via `GlobalModuleCache`) before default methods can be propagated.
If the behavior is not found, default method registration is silently skipped.
`core.cpp:1285–1327`

---

## 2. Behavior Impl Registration

### 2.1 The `register_impl` Function

```cpp
// env_lookups.cpp:460–462
void TypeEnv::register_impl(const std::string& type_name,
                             const std::string& behavior_name) {
    behavior_impls_[type_name].push_back(behavior_name);
}
```

This is the sole mechanism for recording that a type implements a behavior. It
appends to a `vector<string>`; it does **not** deduplicate. Calling `register_impl`
twice for the same pair results in the behavior appearing twice in the list.

**Invariant I-2**: `behavior_impls_[T]` is a `vector`, not a `set`. Duplicate
entries are possible when a behavior impl is registered from multiple sources
(module loading pass + checker pass). `type_implements` uses `std::find` which
stops at the first match, so duplicates are harmless for correctness but waste
memory. `env_lookups.cpp:460–462`, `env_lookups.cpp:496–498`

### 2.2 Sources of `register_impl` Calls

| Source | File | When |
|--------|------|------|
| `check_impl_decl` (user code `impl B for T`) | `core.cpp:1283` | During body checking pass |
| `register_struct_decl` via `@derive(X)` | `decl_struct.cpp:175–464` | During struct registration |
| `register_enum_decl` via `@derive(X)` | `decl_struct.cpp:828–1181` | During enum registration |
| `register_enum_decl` via `@flags` | `decl_struct.cpp:828–853` | During enum registration |
| Module loading pass 1 | `env_module_load_decls.cpp:526` | When loading `.tml` modules |
| Module loading via binary | `env_module_loading.cpp:258, 269, 354, 364` | Loading `.bin` caches |
| Builtin type init | `builtins/types.cpp:193, 203–248` | At TypeEnv construction |

**Invariant I-3**: Builtin behavior impls (primitives implementing `Eq`, `Ord`,
`Hash`, `Display`, etc.) are registered at `TypeEnv` construction time, before any
user code is processed. `builtins/types.cpp:193–248`

### 2.3 Behavior Registration from Checker vs Module Loading

When the checker processes a user-written `impl B for T` block, the behavior name
is extracted from `impl.trait_type->as<parser::NamedType>().path.segments.back()` —
the **last path segment only**. `core.cpp:1274–1278`

When module loading processes the same code (loaded as an imported module),
`env_module_load_decls.cpp:525` also uses `.path.segments.back()`.

**Invariant I-4**: Both the checker and the module loader extract the behavior name
using only the last path segment (`segments.back()`). Given `impl core::io::Write
for Foo`, the registered behavior name is `"Write"`, not `"core::io::Write"`. This
is the key keying bug described in Section 9.

### 2.4 Default Method Propagation

When `register_impl` registers `type_name → behavior_name`, the checker also
propagates any default methods from the behavior's `methods_with_defaults` set.
`core.cpp:1329–1374`

The propagation process:
1. Looks up `BehaviorDef` for `behavior_name` via `env_.lookup_behavior(behavior_name)`.
2. If not found, attempts on-demand load from a hardcoded `behavior_modules` map.
   `core.cpp:1296–1327`
3. For each method in the behavior not provided by the impl, if the method name is
   in `methods_with_defaults`, registers `type_name::method_name` as a `FuncSig` with
   `This`/`Self`/associated-type substitution applied. `core.cpp:1345–1372`

**Invariant I-5**: Default method propagation occurs during `register_impl_decl` in
the checker, not during `register_impl`. The module loading pass (`env_module_load_decls.cpp`)
does not propagate default methods during pass 1; it does so in pass 2 (line 1073+)
after all behaviors in the module are registered.

**Invariant I-6**: The hardcoded `behavior_modules` map `core.cpp:1296–1314` lists
14 behaviors by short name. Any behavior not in this map that is referenced by an
impl block in user code but not explicitly imported will silently have no default
methods propagated.

### 2.5 `@derive` Registration

`@derive` decorators are processed during `register_struct_decl` and
`register_enum_decl`. Each `@derive(X)` both calls `register_impl(type_name, X)` and
immediately registers the derived method FuncSigs. For structs,
`env_.define_func(FuncSig{.name = full_name + "::" + method_name, ...})`.

**Invariant I-7**: `@derive` registration is guarded by `decl.generics.empty()`.
Generic types (structs or enums with type parameters) are **not** given derived
method implementations at registration time. The comment reads "Skip generic types —
they need instantiation first". `decl_struct.cpp:173, 210, 235, 259, 284, 304, 334,
360, 385, 410, 435, 461`

For generic types with `@derive`, neither `register_impl` nor `define_func` is called.
The derived behavior is silently absent from `behavior_impls_` and `functions_` for
generic instantiations until those instantiations are explicitly monomorphized.

**Invariant I-8**: For non-generic structs, the `full_name` passed to `define_func`
uses `qualified_name(decl.name)` which prepends namespace segments with `.` separator.
For non-generic enums, `decl.name` (unqualified) is used. This is an asymmetry:
`decl_struct.cpp:92` (struct), `decl_struct.cpp:736` (enum). In namespace-free code
both produce the same result.

---

## 3. Method Registration — Naming Convention

### 3.1 Inherent Method Key Format

For both behavior impls and extend blocks, methods are registered into `functions_`
using the key `"TypeName::method_name"`. `core.cpp:1233`

```cpp
std::string qualified_name = type_name + "::" + method.name;
env_.define_func(FuncSig{.name = qualified_name, ...});
```

The `type_name` here is the **short name** extracted from `impl.self_type` by taking
`named.path.segments.back()`. `core.cpp:1149–1164`

**Invariant I-9**: Method FuncSigs in `functions_` are keyed by
`"TypeName::method_name"` where `TypeName` is the short type name, never the FQN.
Two types with the same short name from different modules will collide in `functions_`.
This is part of the same short-name bug as Section 9.

### 3.2 Specialized Impl Method Key (Double Registration)

For specialized impls like `impl[T] Pin[Heap[T]]`, the checker also registers
methods under a **specialized key** in addition to the base-type key.

```cpp
// core.cpp:1259–1268
if (!specialized_type_name.empty()) {
    std::string spec_qualified = specialized_type_name + "::" + method.name;
    env_.define_func(FuncSig{.name = spec_qualified, ...});
}
```

The `specialized_type_name` is a string like `"Pin__Heap__T"` that encodes the
full self-type structure. Both `"Pin::method"` and `"Pin__Heap__T::method"` are
registered. Call sites try the specialized key first.

**Invariant I-10**: For specialized impls (impl on a specific generic instantiation),
methods are registered twice: once under `"BaseType::method"` and once under
`"SpecializedType::method"`. The specialized key must be tried before the base key
to avoid dispatching the wrong impl to a different specialization.

### 3.3 Class Method Registration

Class (`ClassDecl`) methods are not registered into `functions_` during
`register_class_decl`. They are stored directly in `ClassDef.methods` and accessed
via `lookup_class(name)->methods`. `core_oop.cpp:264–322`

**Invariant I-11**: Class methods reside in `ClassDef.methods`, not in `functions_`.
Callers that perform method lookup on class types must use `env_.lookup_class(name)`
and search `ClassDef.methods`, not `env_.lookup_func("ClassName::method")`.

Interface methods are similarly stored in `InterfaceDef.methods`. `core_oop.cpp:159–192`

---

## 4. Method Resolution Order

### 4.1 `lookup_method` and `lookup_func`

`lookup_func` (`env_lookups.cpp:168–289`) performs the following search sequence
for a name `"TypeName::method"`:

1. **Local `functions_` map**: `functions_.find(name)`, returns `second[0]` (first
   overload). `env_lookups.cpp:174–177`
2. **Imported symbol resolution**: Calls `resolve_imported_symbol(name)` to resolve
   through the current module's import table. `env_lookups.cpp:178–212`
3. **Module-name prefix matching**: If name contains `"::"`, treats the prefix as a
   module short name and searches all loaded modules for the function.
   `env_lookups.cpp:218–233`
4. **All-module fallback**: Searches every loaded module's `functions` map.
   `env_lookups.cpp:238–244`
5. **GlobalModuleCache fallback**: Searches the global preloaded cache.
   `env_lookups.cpp:250–287`

**Invariant I-12**: `lookup_func` returns only the **first** overload registered
under the name. There is no arity-based disambiguation. If multiple FuncSigs are
registered under the same key (`functions_[name]` is a vector), `lookup_func` always
returns `second[0]` — the one registered first. `env_lookups.cpp:175–177`

**Invariant I-13**: `lookup_func_overload` (`env_lookups.cpp:292–333`) performs
arity + type matching against the vector of overloads in `functions_[name]`. It is
the correct function to call for overload resolution. However, it does NOT fall
through to the GlobalModuleCache fallback that `lookup_func` reaches at step 5.
`env_lookups.cpp:332` — returns `nullopt` without GlobalModuleCache search.

### 4.2 Behavior Method vs Inherent Method Priority

When both a behavior method and an inherent method share the same key (e.g., both
`impl B for Foo` and `impl Foo` define `"Foo::bar"`), the one registered **later**
in the `functions_` vector appears at a higher index. `lookup_func` returns `[0]`,
which is the **earlier** registration.

**Invariant I-14**: If an inherent method and a behavior-provided method share the
same `"TypeName::method"` key, the one registered first wins when `lookup_func` is
used. There is no explicit "inherent methods take priority over behavior methods"
rule enforced by the lookup. Priority is determined by registration order.

The checker's `register_impl_decl` processes the impl's explicit methods first
(registering them into `functions_`), then propagates behavior defaults for methods
not in the impl. `core.cpp:1181–1374`. This means user-written methods are registered
before defaults, so `lookup_func` returns the user-written method when both exist.

### 4.3 Super-Behavior Inheritance in `type_implements`

`type_implements(type_name, behavior_name)` (`env_lookups.cpp:491–562`) checks:

1. Explicit `behavior_impls_[type_name]` vector for an exact string match.
2. Whether any explicitly-implemented behavior transitively inherits from
   `behavior_name` via `behavior_inherits_from`. `env_lookups.cpp:504–509`
3. Auto-derivation of `Send`/`Sync` by structural inspection of fields.
   `env_lookups.cpp:515–558`

`behavior_inherits_from` (`env_lookups.cpp:465–488`) walks the `super_behaviors`
list of a `BehaviorDef` recursively with cycle detection.

**Invariant I-15**: `type_implements` answers the question "does this type satisfy
a bound on `behavior_name`?" It does **not** return which specific impl provides the
satisfaction. The result is a `bool`. Callers cannot distinguish "implements directly"
from "implements via super-behavior inheritance".

**Invariant I-16**: `Send` and `Sync` are auto-derived structurally for structs,
enums, and classes whose fields all implement `Send`/`Sync`. This auto-derivation
occurs inside `type_implements`, not during registration. It means a type can
satisfy `Send`/`Sync` bounds without an explicit `register_impl` call.
`env_lookups.cpp:515–558`

### 4.4 Method Resolution for Behavior-Provided Methods

When the checker encounters `receiver.method()` and the receiver type is `T`, it
constructs the key `"T::method"` and calls `lookup_func`. If the type implements
a behavior that provides `method`, that method must have been previously registered
into `functions_["T::method"]` by either:
- An explicit `impl B for T { func method() ... }` call to `define_func`, or
- Default method propagation in `register_impl_decl`.

**Invariant I-17**: There is no runtime vtable walk during `lookup_func`. All
behavior methods reachable through impls must be pre-registered as entries in
`functions_` keyed by `"TypeName::method_name"`. If a method is not registered,
`lookup_func` returns `nullopt` even if the type declares `impl B for T` and `B`
provides the method.

---

## 5. Coherence Rules — Which Impl Wins

TML's current type checker does not enforce the Rust orphan rule. The following rules
describe what the implementation enforces:

### 5.1 No Duplicate Impl Detection

`register_impl` appends to a `vector` without checking for existing entries.
`define_func` appends to `functions_[name]` without checking for existing overloads.
There is no "only one impl of behavior B per type T" enforcement.

**Invariant I-18**: The type checker does not reject duplicate `impl B for T` blocks.
If two impl blocks both implement the same behavior for the same type, the methods
from both are appended to `functions_["T::method"]`. `lookup_func` returns the
first-registered method. No error is emitted. This is a latent coherence gap.

### 5.2 Sealed Class Override Coherence

For class types, override validation enforces a coherence rule: a method may only
be marked `override` if a `virtual` or `abstract` method with the same name exists
in some base class in the hierarchy. `core_oop.cpp:677–789`

The validator `validate_override` walks the base class chain and requires:
1. The method exists in the chain. (`core_oop.cpp:786–788` — T065 if not found)
2. The found method is `virtual` or `abstract`. (`core_oop.cpp:709–713` — T064 if not)
3. Return types match exactly. (`core_oop.cpp:723–728` — T016 if mismatch)
4. Parameter types match by count and by exact type. (`core_oop.cpp:741–769` — T004, T058)

**Invariant I-19**: Override matching compares return types using `types_equal`, not
subtype compatibility. A covariant return type change (returning a subclass) is
rejected with T016. `core_oop.cpp:723–728`

### 5.3 Interface Impl Completeness Check

`validate_interface_impl` (`core_oop.cpp:792–885`) iterates all interface methods
and verifies that the class provides an implementation. It skips methods with
`has_default = true`.

**Invariant I-20**: For classes implementing interfaces, completeness is checked at
the `check_class_decl` call site (pass 2). The check is by method name only — it
finds the first class method whose name matches the interface method name.
`core_oop.cpp:815`. If two interface methods have the same name, only the first
match is examined. Disambiguation by signature is not performed.

### 5.4 Abstract Method Check

`validate_abstract_methods` (`core_oop.cpp:545–613`) verifies that non-abstract
classes implement all abstract methods from the inheritance chain. The walk starts
from `cls.extends.value().segments.back()` and ascends. `core_oop.cpp:549`

**Invariant I-21**: A concrete class is checked for abstract method coverage only
if `!cls.is_abstract && cls.extends.has_value()`. A class that has `@value` or
`@pool` and no base class is never checked for abstract methods even if it claims
to implement an interface. `core_oop.cpp:456–459`

---

## 6. `extend` Block Semantics

### 6.1 Syntax Distinction

In TML's AST, both behavior impls and inherent method blocks use `parser::ImplDecl`.
The distinction is:
- **Behavior impl**: `impl.trait_type != nullptr` — the `for Behavior` part is present.
- **Extend block (inherent)**: `impl.trait_type == nullptr` — methods added directly
  to the type with no behavior association.

### 6.2 Impl Processing Path

`register_impl_decl` (`core.cpp:1138+`) handles both cases:

```cpp
// core.cpp:1272–1283 (simplified)
std::string behavior_name;
if (impl.trait_type && impl.trait_type->is<parser::NamedType>()) {
    behavior_name = named.path.segments.back();
}
if (!behavior_name.empty()) {
    env_.register_impl(type_name, behavior_name);
    // ... default method propagation ...
}
```

When `trait_type` is null (extend block), `behavior_name` is empty. The `if`
block is skipped, so:
- No entry is added to `behavior_impls_`.
- No default methods are propagated.
- Methods are still registered into `functions_["TypeName::method"]`.

**Invariant I-22**: Extend blocks (inherent methods) register their methods into
`functions_["TypeName::method"]` identically to behavior impls. They do not add to
`behavior_impls_`. A type with only inherent methods via extend blocks reports
`behavior_impls_[type_name]` as empty or missing.

### 6.3 Module Loading — Inherent vs Behavior

In module loading, the distinction is the same: `env_module_load_decls.cpp:522`
only calls `register_impl` when `impl_decl.trait_type != nullptr`. Inherent method
FuncSigs are still registered into `mod.functions` for both cases.
`env_module_load_decls.cpp:552–646`

### 6.4 Visibility Rules

In `env_module_load_decls.cpp:557–559`, non-public methods are skipped during
module loading **unless** the impl is a behavior impl or a generic impl:

```cpp
// env_module_load_decls.cpp:557–559
bool is_generic_impl = !impl_decl.generics.empty();
if (func.vis != parser::Visibility::Public && !is_behavior_impl &&
    !is_generic_impl) {
```

**Invariant I-23**: Private methods in extend blocks of imported modules are **not**
registered into the module's function map. Private methods in behavior impl blocks or
generic impl blocks **are** registered, because the checker may need them for default
method dispatch or generic instantiation. `env_module_load_decls.cpp:557–562`

---

## 7. Struct and Enum Field Lookup Algorithms

### 7.1 Struct Field Lookup

Struct fields are stored in `StructDef.fields` as a `vector<StructFieldDef>`.
`StructDef` is returned by `lookup_struct(name)`. There is no field-specific lookup
function; callers iterate `struct_def->fields` and find by name.

**Invariant I-24**: Struct field lookup is linear in the number of fields. No hash
map or index exists for field lookup by name. The ordering of fields in `StructDef.fields`
matches the declaration order in source. `decl_struct.cpp:66–79`

### 7.2 Enum Variant Lookup

Enum variants are stored in `EnumDef.variants` as `vector<pair<string, vector<TypePtr>>>`.
`EnumDef` is returned by `lookup_enum(name)`. Variant lookup is also linear.

**Invariant I-25**: Enum variant data is stored as `(name, payload_types_list)`.
Unit variants (no payload) have an empty `payload_types_list`. Tuple variants have
one or more payload types. Struct variants are not currently supported as a distinct
case — the AST representation converts them to `struct_fields` which are not yet
handled in `register_enum_decl`. `decl_struct.cpp:532–539`

### 7.3 Infinite-Size Self-Reference Check

`register_enum_decl` performs a recursive self-reference check before calling
`define_enum`. It detects enums whose variants directly contain the enum type
itself without pointer indirection. `decl_struct.cpp:543–598`

The check uses a recursive lambda `contains_direct_self_ref`. Types that provide
pointer indirection and break the cycle are: `Heap`, `Shared`, `Sync`, `List`,
`Vec`, `ArrayList`, `RefType`, `PtrType`. `decl_struct.cpp:546–547, 572–573`

**Invariant I-26**: A direct recursive enum (variant holds the enum type without
indirection) is rejected with error T085 and `define_enum` is never called.
An indirect recursive enum (variant holds `Heap[Self]` or `List[Self]`) is
accepted. `decl_struct.cpp:589–598`

### 7.4 `lookup_enum` Fallback Behavior

`lookup_enum` (`env_lookups.cpp:55–87`) has an unusually broad fallback: after
checking the local `enums_` map and the import table, it searches **all loaded
modules** including `internal_enums`. `env_lookups.cpp:74–84`

This fallback exists because "library code is re-parsed during codegen and the
import context isn't available". `env_lookups.cpp:71–73`

**Invariant I-27**: `lookup_enum` searches all modules unconditionally as a fallback.
This means an enum defined in any loaded module is globally visible to `lookup_enum`
even without an explicit `use` statement. Contrast with `lookup_struct`, which
does not have this all-module fallback. `env_lookups.cpp:55–87` vs `env_lookups.cpp:37–53`

### 7.5 `lookup_behavior` Triple Fallback

`lookup_behavior` (`env_lookups.cpp:89–136`) has three fallback layers:

1. Local `behaviors_` map. `env_lookups.cpp:90–92`
2. Import table → module registry lookup. `env_lookups.cpp:93–101`
3. All-module scan of `module_registry_`. `env_lookups.cpp:103–119`
4. `GlobalModuleCache` scan. `env_lookups.cpp:121–133`

The GlobalModuleCache fallback comment: "handles standalone files (no 'use' imports)
that reference behaviors like Hash, PartialEq, Display, etc. in generic type bounds".

**Invariant I-28**: `lookup_behavior` will find a behavior from any loaded module
without an explicit `use` statement, due to the GlobalModuleCache fallback. This
makes behavior lookup effectively module-global. In a self-hosting port,
`lookup_behavior` must replicate this broad search or tests will fail.

---

## 8. Generic Impl Matching

### 8.1 Monomorphization Triggers

Generic impls are not matched against concrete types at impl registration time.
Matching happens downstream, in HIR lowering and codegen monomorphization. The
type checker records the generic impl's method FuncSigs under
`"GenericType::method"` with `type_params` populated.

**Invariant I-29**: For `impl[T] Container[T]`, the method FuncSig is registered as
`"Container::method"` with `type_params = ["T"]`. Call sites with `Container[I32]`
look up `"Container::method"` and get the generic signature. The concrete
`I32`-substituted version is not created during type checking; it is created during
HIR monomorphization or MIR codegen. `core.cpp:1233–1268`

### 8.2 Specialized Impl Key Strategy

When `resolved_self` is a non-trivial generic type (e.g., `Pin[Heap[T]]`), a
`specialized_type_name` is derived and the method is also registered under that key.
`core.cpp:1256–1268`

The `specialized_type_name` encoding uses `"__"` as a separator between the base
type and type arguments, producing strings like `"Pin__Heap__T"`. This matches the
mangled type names used in codegen.

**Invariant I-30**: The specialized impl method key format is
`"BaseType__TypeArg1__TypeArg2::method_name"`. The same separator (`"__"`) is used
in `is_trivially_destructible` for parsing mangled generic type names.
`env_lookups.cpp:925–984`

### 8.3 `impl_self_type_args` Field

`FuncSig.impl_self_type_args` is a `vector<TypePtr>` set by `register_impl_decl`
at `core.cpp:1254`. It captures the resolved type arguments from the impl's
self-type (e.g., for `impl[T] Pin[ref T]`, captures `[ref T]`). Codegen uses this
field to match call sites to the right specialized impl.

**Invariant I-31**: `impl_self_type_args` is populated only for specialized impls
where the self type is a `NamedType` with non-empty `type_args`. For plain inherent
impls like `impl Foo`, it is empty. `core.cpp:1226–1229`

### 8.4 Generic Type Bounds (Where Clauses)

Where clauses are processed in `register_impl_decl` at `core.cpp:1203–1214`. Type
equalities of the form `where I::Item = ref T` are added to
`current_associated_types_` with the key `"I::Item"`. These bindings are only
live during the processing of that one impl block; they are not persisted to
`TypeEnv` after `register_impl_decl` returns.

**Invariant I-32**: Where-clause type equalities are ephemeral. They are inserted
into `current_associated_types_` for the duration of one impl block's registration
and cleared at the start of the next. `core.cpp:1196–1214`

---

## 9. Known Bug — Short-Name vs FQN Keying

**Tracked in**: `phase0i_behavior-fqn-keying`

### 9.1 Affected Maps

The following three maps are keyed by **short name**, not by FQN. This causes
silent last-write-wins collisions when two items from different modules share a
short name:

| Map | File:Line | Key Used | Collision Example |
|-----|-----------|----------|-------------------|
| `TypeEnv::behaviors_` | `env.hpp:760`, `env_definitions.cpp:36` | `def.name` | `core::io::Write` vs `core::fmt::Write` both become `"Write"` |
| `TypeEnv::functions_` | `env.hpp:761–762`, `env_definitions.cpp:40–41` | `sig.name` (short) | `File::write_str` vs any other `write_str` |
| `LLVMIRGen::trait_decls_` | `codegen/llvm/core/generate.cpp:770` | `def.name` | same Write collision at codegen layer |

`proposal.md` for `phase0i_behavior-fqn-keying` identifies the exact insertion sites:
- `compiler/src/types/env_definitions.cpp:36` — `behaviors_[def.name]`
- `compiler/src/codegen/llvm/core/generate.cpp:770`
- `compiler/src/codegen/llvm/core/generate_first_pass.cpp:216`
- `compiler/src/codegen/llvm/core/generate_function_bodies.cpp:531`
- `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1186, 1271, 1521`
- `compiler/src/codegen/llvm/core/runtime_modules.cpp:846`
- `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp:796`

### 9.2 Lookup Extraction Site

Both the checker and module loader extract behavior names using:

```cpp
// core.cpp:1277
behavior_name = named.path.segments.back();

// env_module_load_decls.cpp:525
std::string behavior_name = trait_named.path.segments.back();
```

Both always take `.back()`. The `BehaviorDef` is inserted with `def.name` (short name).
`env_definitions.cpp:36`:

```cpp
void TypeEnv::define_behavior(BehaviorDef def) {
    behaviors_[def.name] = std::move(def);   // def.name is always the short name
}
```

### 9.3 Manifestation

When a program uses both `core::io::Write` and `core::fmt::Write`:

1. First-loaded behavior (`core::io::Write`) is stored as `behaviors_["Write"]`.
2. Second-loaded behavior (`core::fmt::Write`) overwrites `behaviors_["Write"]`.
3. `dyn.cpp` calls `env_.lookup_behavior("Write")` and gets the **second** behavior.
4. Default methods from the wrong behavior are emitted for the type that implements
   the first behavior.
5. LLVM error: "base element of getelementptr must be sized" because the wrong
   struct type is used in GEP instructions.

### 9.4 Implication for Self-Hosting

A self-hosting TML type checker **must** reproduce this bug exactly (keying by short
name) to remain compatible with the current codegen layer — or fix both layers
simultaneously. The proposal for `phase0i` recommends migrating all maps to FQN
keying at the same time.

**Invariant I-33 (Known Bug)**: `behaviors_` is keyed by short name. Any two
behaviors from different modules with the same short name silently collide. The
last-loaded behavior overwrites the first. No error is emitted. This is tracked in
`phase0i_behavior-fqn-keying`. `env_definitions.cpp:35–37`, `env.hpp:760`

---

## 10. Invariant Summary

The following table consolidates all invariants defined in this document.

| ID | Invariant | File:Line |
|----|-----------|-----------|
| I-1 | BehaviorDef for behavior B must exist before default methods are propagated for impls of B | `core.cpp:1285–1327` |
| I-2 | `behavior_impls_[T]` is a `vector`, not a `set`; duplicates are possible and harmless | `env_lookups.cpp:460–462` |
| I-3 | Builtin behavior impls (primitives) are registered at TypeEnv construction, before user code | `builtins/types.cpp:193–248` |
| I-4 | Behavior name registration always uses `segments.back()` — only the short name, never FQN | `core.cpp:1277`, `env_module_load_decls.cpp:525` |
| I-5 | Default method propagation occurs in the checker's `register_impl_decl`, not in `register_impl` | `core.cpp:1329–1374` |
| I-6 | Only 14 behaviors listed in the hardcoded `behavior_modules` map are auto-loaded for default propagation | `core.cpp:1296–1314` |
| I-7 | `@derive` does not register impls or methods for generic (parameterized) types | `decl_struct.cpp:173, 210, 235, 259` |
| I-8 | Struct `full_name` uses `qualified_name()` with `.` namespace separator; enum uses plain short name | `decl_struct.cpp:92`, `736` |
| I-9 | Methods in `functions_` are keyed by `"TypeName::method_name"` using the short type name | `core.cpp:1233` |
| I-10 | Specialized impl methods are registered twice: under base key and specialized key | `core.cpp:1256–1268` |
| I-11 | Class methods are in `ClassDef.methods`, NOT in `functions_` | `core_oop.cpp:264–322` |
| I-12 | `lookup_func` returns `second[0]` — the first registered overload, ignoring arity | `env_lookups.cpp:175–177` |
| I-13 | `lookup_func_overload` does NOT search GlobalModuleCache; `lookup_func` does | `env_lookups.cpp:292–333` |
| I-14 | When inherent and behavior methods share a key, the first-registered wins via `lookup_func` | `env_lookups.cpp:175–177` |
| I-15 | `type_implements` returns `bool`; it cannot distinguish direct vs inherited implementation | `env_lookups.cpp:491–562` |
| I-16 | `Send`/`Sync` are auto-derived structurally without `register_impl`; no explicit registration needed | `env_lookups.cpp:515–558` |
| I-17 | Behavior methods must be pre-registered in `functions_`; `lookup_func` does not walk vtables | `env_lookups.cpp:168–289` |
| I-18 | No duplicate impl detection — two `impl B for T` blocks silently coexist | `env_definitions.cpp:39–41`, `env_lookups.cpp:460–462` |
| I-19 | Override return type comparison uses exact type equality, not subtype compatibility | `core_oop.cpp:723–728` |
| I-20 | Interface method completeness is checked by name only, not by signature | `core_oop.cpp:815` |
| I-21 | Abstract method coverage check only fires for classes with `!is_abstract && extends.has_value()` | `core_oop.cpp:456–459` |
| I-22 | Extend blocks (inherent impls, `trait_type == nullptr`) do not add to `behavior_impls_` | `core.cpp:1272–1283` |
| I-23 | Private methods in behavior impls and generic impls of imported modules ARE registered; private inherent methods are not | `env_module_load_decls.cpp:557–562` |
| I-24 | Struct field lookup is linear (no hash index); field order matches declaration order | `decl_struct.cpp:66–79` |
| I-25 | Enum variant payloads are `vector<TypePtr>`; unit variants have empty payloads | `decl_struct.cpp:532–539` |
| I-26 | Direct recursive enum variants are rejected with T085 before `define_enum` is called | `decl_struct.cpp:589–598` |
| I-27 | `lookup_enum` searches ALL loaded modules as fallback; `lookup_struct` does not | `env_lookups.cpp:55–87` vs `37–53` |
| I-28 | `lookup_behavior` searches GlobalModuleCache as final fallback, making behaviors module-globally visible | `env_lookups.cpp:121–133` |
| I-29 | Generic impl methods are registered with `type_params` set but no concrete substitution at registration time | `core.cpp:1233–1268` |
| I-30 | Specialized impl key format uses `"__"` separator matching codegen mangled names | `core.cpp:1260`, `env_lookups.cpp:925` |
| I-31 | `impl_self_type_args` is only populated for specialized impls with non-empty self-type type_args | `core.cpp:1226–1229` |
| I-32 | Where-clause type equalities in `current_associated_types_` are ephemeral, cleared per impl block | `core.cpp:1196–1214` |
| I-33 (BUG) | `behaviors_` is keyed by short name — same-short-name behaviors from different modules silently collide | `env_definitions.cpp:36`, tracked in `phase0i_behavior-fqn-keying` |

---

## Appendix A — Registration Sequence for a Typical Behavior Impl

Given user code:

```tml
@derive(PartialEq, Hash)
struct Point {
    x: I64,
    y: I64,
}

impl Display for Point {
    func to_string(ref this) -> Str { ... }
}
```

The registration sequence in the type checker is:

1. **Pass 1 (type registration)**: `register_struct_decl` called for `Point`.
   - `env_.define_struct(StructDef{.name = "Point", .fields = [x:I64, y:I64]})` called.
     `decl_struct.cpp:164–169`
   - `@derive(PartialEq)` guard: `decl.generics.empty()` → true, so:
     - `env_.register_impl("Point", "PartialEq")` called. `decl_struct.cpp:212`
     - `env_.define_func(FuncSig{.name = "Point::eq", .params = [ref Point, ref Point], .return_type = Bool})` called.
       `decl_struct.cpp:225–230`
   - `@derive(Hash)` guard: same → `register_impl("Point", "Hash")`, `define_func("Point::hash", ...)`.
     `decl_struct.cpp:261–279`

2. **Pass 2 (impl bodies)**: `register_impl_decl` called for `impl Display for Point`.
   - `type_name = "Point"` extracted. `core.cpp:1149–1164`
   - Method `to_string` registered as `define_func(FuncSig{.name = "Point::to_string", .params = [ref Point], .return_type = Str})`.
     `core.cpp:1233–1253`
   - `behavior_name = "Display"` extracted from `trait_type`. `core.cpp:1277`
   - `env_.register_impl("Point", "Display")` called. `core.cpp:1283`
   - `env_.lookup_behavior("Display")` → finds `BehaviorDef` (from builtins or GlobalModuleCache).
   - Default methods for `Display` checked: any methods in `methods_with_defaults`
     not already in `impl_method_names` are registered. `core.cpp:1345–1372`

After this sequence:
- `structs_["Point"]` = StructDef with fields.
- `behavior_impls_["Point"]` = `["PartialEq", "Hash", "Display"]`.
- `functions_["Point::eq"]` = `[FuncSig{params=[ref Point, ref Point], ret=Bool}]`.
- `functions_["Point::hash"]` = `[FuncSig{params=[ref Point], ret=I64}]`.
- `functions_["Point::to_string"]` = `[FuncSig{params=[ref Point], ret=Str}]`.

---

## Appendix B — Class vs Behavior Impl System

The type checker contains **two parallel impl systems**:

| Feature | Behavior / Struct System | Class / Interface System |
|---------|--------------------------|--------------------------|
| Type declaration | `struct`, `enum` | `class` |
| Interface mechanism | `behavior` | `interface` |
| Impl mechanism | `impl B for T` | `class C implements I` |
| Method storage | `functions_["T::method"]` | `ClassDef.methods` |
| Impl record | `behavior_impls_["T"]` | `class_interfaces_["C"]` |
| Lookup function | `lookup_func("T::method")` | `lookup_class(C)->methods` |
| Override validation | None | `validate_override` |
| Completeness check | `type_implements` | `validate_interface_impl` |

These systems are independent. A `class` type does NOT participate in
`behavior_impls_`. An `impl B for class-name` block is syntactically valid but
unusual; it would register into `behavior_impls_` but not into `class_interfaces_`.
`core_oop.cpp:100–422`

---

*Document generated by spec-engineer agent, 2026-04-06.*  
*Source files read: `core_oop.cpp` (1067 LOC), `decl_struct.cpp` (1207 LOC),*  
*`env_lookups.cpp` (1265 LOC), `env_definitions.cpp` (139 LOC), `env.hpp` (807 LOC),*  
*`core.cpp` (partial: impl processing sections), `env_module_load_decls.cpp` (partial),*  
*`phase0i_behavior-fqn-keying/proposal.md`.*


---

## Section 4 — Body Checking and Inference Invariants

> **Full source audit**: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section4_body_inference.md`  
> **Source files**: `checker/expr.cpp` (652 LOC), `checker/expr_call.cpp` (802 LOC), `checker/expr_call_method.cpp` (1363 LOC), `checker/expr_call_method_types.cpp` (668 LOC), `checker/expr_ops.cpp`, `checker/types_checker.cpp`, `env_core.cpp`


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


---

## Section 5 — Cross-Cutting Invariants

> **Full source audit**: `.rulebook/tasks/phase12c_typechecker-invariants/specs/section5_cross_cutting.md`  
> **Source files**: `checker/expr_ops.cpp`, `checker/expr_special.cpp`, `checker/stmt.cpp`, `checker/control.cpp`, `checker/const_eval.cpp`, `checker/helpers.cpp`, `checker/resolve.cpp`, `checker/types_checker.cpp`, `builtins/register.cpp`, `builtins/types.cpp`, `builtins/io.cpp`, `builtins/mem.cpp`, `builtins/math.cpp`, `builtins/atomic.cpp`, `builtins/sync.cpp`, `builtins/async.cpp`, `env_core.cpp`, `env_definitions.cpp`, `env_scope.cpp`, `env_module_support.cpp`


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



---

## Section 6 — Self-Hosting Contract

This section enumerates the invariants a TML-written type checker MUST preserve to produce a `TypeEnv` that downstream C++ stages (HIR lowering, THIR lowering, MIR building, codegen) accept without modification.

Each contract item states a concrete requirement, cites source invariants, and notes latent bugs that must be replicated for compatibility until a coordinated fix lands.

---

### 6.1 Registration Contract

**R-01 — Builtins-before-user-code**
The type checker MUST initialize all builtin definitions before processing any user declaration. `init_builtins()` (or equivalent) is called first on every fresh TypeEnv. Init order MUST be: types -> io -> mem -> atomic -> sync -> math -> async. (`async` depends on `Poll[T]` which `types` registers.)
*Source*: CC-01, CC-02; `env_core.cpp:32-35`, `builtins/register.cpp:38-52`

**R-02 — Four-pass declaration order**
All declarations MUST be processed in exactly four sequential passes: (0) imports, (1) type declarations, (2) function signatures and impl registrations, (3) function bodies. No declaration from pass N+1 may begin before pass N is complete for the entire module.
*Source*: I-1.17; `checker/core.cpp:203-255`

**R-03 — Namespace blocks are self-contained**
A `namespace` block MUST run its own passes 1-2-3 inline during the outer pass 1. When outer pass 2 begins, all declarations inside namespaces (including bodies) are already fully registered.
*Source*: I-1.22; `checker/core.cpp:278-334`

**R-04 — Snapshot semantics for per-file state**
Each compilation unit starts with a snapshot: shares definition tables (`structs_`, `enums_`, `behaviors_`, `functions_`, `behavior_impls_`, `type_aliases_`, `classes_`, `interfaces_`, `class_interfaces_`, `builtins_`) but gets fresh local state (`current_scope_` = new root, `type_var_counter_` = 0, `substitutions_` = empty, `module_registry_` = empty, `imported_symbols_` = empty).
*Source*: I-1.13, I-1.14, CC-03; `env_core.cpp:102-122`

**R-05 — Reserved names enforced at registration**
Registering a struct, enum, or type alias in RESERVED_TYPE_NAMES (17 primitives + StringBuilder + Future) MUST emit T038 and drop the registration. Same for 23 RESERVED_BEHAVIOR_NAMES entries. The sets are fixed.
*Source*: I-1.23; `checker/core.cpp:46-113`

**R-06 — define_struct/enum/behavior is last-write-wins**
Calling `define_struct`, `define_enum`, or `define_behavior` twice with the same name silently overwrites the first. No deduplication guard may be added; existing tests rely on this for module loading interactions.
*Source*: CC-07; `env_definitions.cpp:27-37`

**R-07 — define_func appends, never replaces**
`define_func` MUST append to the overload vector `functions_[name]`. The first registered overload is what `lookup_func` returns. User-written methods are registered before behavior default methods, ensuring `lookup_func` returns the user-written one.
*Source*: CC-08, I-3.14; `env_definitions.cpp:39-61`

---

### 6.2 Module Contract

**M-01 — Import-before-registration**
All pass 0 (import) work MUST complete — loading referenced modules, populating `imported_symbols_` — before any pass 1 type registration for the current module.
*Source*: INV-13; `checker/core.cpp:203-261`

**M-02 — Module loaded at most once per TypeEnv**
`module_registry_->has_module(path)` is checked before loading. A path already registered returns immediately. A path in `loading_modules_` returns `true` (success) immediately without loading — the caller must not treat this as an error.
*Source*: INV-01, INV-02; `env_module_loading.cpp:179`, `env_module_load.cpp:309-315`

**M-03 — Re-export chain loading is transitive**
After registering a module, ALL `re_exports[i].source_path` modules and ALL `private_imports[i]` paths MUST be loaded with `silent=true`. This applies across all three load paths: source file, binary meta cache, and GlobalModuleCache.
*Source*: INV-15, INV-16; `env_module_load.cpp:481-503`

**M-04 — behavior_impls re-registered on cache hit**
When a module is loaded from GlobalModuleCache or binary meta cache, its `behavior_impls` map MUST be re-registered into the current TypeEnv before placing the module in the local ModuleRegistry.
*Source*: INV-04; `env_module_loading.cpp:256-260`, `352-356`

**M-05 — Single-path import: silent on missing module**
For `use foo::bar::Baz` (single path, not glob or group), a missing module MUST NOT emit T027. The last segment is assumed to be a symbol name. Glob and group imports DO emit T027.
*Source*: INV-21; `checker/core.cpp:554-558`

**M-06 — lookup_enum finds internal enums; lookup_struct does not**
`lookup_enum` searches all loaded modules unconditionally as a fallback, including `internal_enums`. `lookup_struct` does NOT have this fallback. This asymmetry MUST be preserved.
*Source*: INV-09, I-3.27; `env_lookups.cpp:55-87` vs `37-53`

**M-07 — abort_on_module_error_ temporarily disabled during import resolution**
When a UseDecl triggers a recursive module load inside `extract_module_declarations`, `abort_on_module_error_` MUST be temporarily set to `false` and restored. Module parse failures during import resolution are silently swallowed.
*Source*: Section 2 Finding 3; `env_module_load_decls.cpp:933-955`

---

### 6.3 Impl Contract

**IM-01 — Behavior name stored as short name (latent bug)**
`register_impl` MUST receive the short name (`segments.back()`) of the behavior, never the FQN. Both the checker and module loader use `.back()`. Until `phase0i_behavior-fqn-keying` is resolved, a TML port MUST replicate this short-name keying.
*Source*: I-3.4, I-3.33; `checker/core.cpp:1277`, `env_module_load_decls.cpp:525`

**IM-02 — behavior_impls_ is a vector; duplicates are harmless**
`behavior_impls_[T]` is `vector<string>`. `register_impl` appends without deduplication. The TML port MUST NOT add deduplication; both module loading and the checker call `register_impl` for the same pair.
*Source*: I-3.2; `env_lookups.cpp:460-462`

**IM-03 — @derive skips generic types (latent behaviour)**
`@derive(X)` MUST NOT register impls or derived methods for types with non-empty `generics`. The `decl.generics.empty()` guard MUST be replicated.
*Source*: I-3.7; `checker/decl_struct.cpp:173`

**IM-04 — Specialized impl double registration**
For specialized impls (e.g., `impl[T] Pin[Heap[T]]`), methods MUST be registered under BOTH the base key (`Pin::method`) and the specialized key (`Pin__Heap__T::method`). The `__` separator is used by codegen mangling.
*Source*: I-3.10, I-3.30; `checker/core.cpp:1256-1268`

**IM-05 — Default method propagation requires behavior to exist**
Default methods are propagated only when the BehaviorDef is found. The on-demand loader covers exactly 14 hardcoded behaviors. All others silently skip default propagation if the behavior is absent.
*Source*: I-3.1, I-3.5, I-3.6; `checker/core.cpp:1285-1327`

**IM-06 — Class methods live in ClassDef.methods, not functions_**
Class methods MUST NOT be placed in `functions_["ClassName::method"]`. They are in `ClassDef.methods`, accessed via `lookup_class(name)->methods`. Interface methods are in `InterfaceDef.methods`.
*Source*: I-3.11; `checker/core_oop.cpp:264-322`

**IM-07 — lookup_behavior searches GlobalModuleCache as final fallback**
Lookup sequence: (1) local `behaviors_`, (2) import table, (3) all loaded modules, (4) GlobalModuleCache. All four layers MUST be implemented or behavior-bounded generic code fails.
*Source*: I-3.28; `env_lookups.cpp:89-136`

---

### 6.4 Inference Contract

**IN-01 — Integer literals default to I64**
An unsuffixed integer literal with no expected-type context infers as `I64`. An expected integer type overrides the default. No second-pass coercion from I64 to I32 at call sites.
*Source*: I-4.02; `checker/expr.cpp:379`

**IN-02 — unify is one-sided, no structural decomposition**
`unify(a, b)` records a substitution only when one side is a TypeVar. If neither is a TypeVar, it is a no-op. Structural matching is done manually at call sites. No occurs-check.
*Source*: I-4.09, I-4.10; `env_core.cpp:57-63`

**IN-03 — resolve is shallow (not transitive)**
`resolve(T)` follows the top-level TypeVar chain only. Inner type arguments are NOT resolved. `T -> List[U]` + `U -> I32` resolves T to `List[U]`, not `List[I32]`. Callers needing fully-resolved composite types must call resolve on each component.
*Source*: I-4.11; `env_core.cpp:65-92`

**IN-04 — Closure inferred types stored back in AST via void***
After type-checking a closure, inferred param types and return type MUST be written to ClosureExpr AST node fields via `shared_ptr<void>` type erasure. HIR builder reads them back by static_pointer_cast.
*Source*: I-4.12, CC-61; `checker/types_checker.cpp:309-316`

**IN-05 — Enum constructor call returns bare NamedType (latent simplification)**
Calling `Just(42)` returns `NamedType{"Maybe", "", {}}` with empty type_args. The generic argument is NOT inferred. HIR builder must infer enum constructor type arguments independently.
*Source*: I-4.15; `checker/expr_call.cpp:391-393`

**IN-06 — Maybe[T].map(f) returns Maybe[T], not Maybe[U] (latent simplification)**
`Maybe[T].map(f)` returns the original `Maybe[T]`. The closure return type is not propagated. HIR builder and codegen must not assume the map return type reflects the closure return type.
*Source*: I-4.26; `checker/expr_call_method_types.cpp:67-69`

**IN-07 — Unknown method calls return Unit silently**
All 15 dispatch steps failing MUST return `make_unit()` with no diagnostic. Adding an error here would break programs that currently compile.
*Source*: I-4.23; `checker/expr_call_method.cpp:1360`

**IN-08 — Type variable IDs are file-local**
`type_var_counter_` resets to 0 in each snapshot. TypeVar IDs are not globally unique. TypeVar nodes MUST NEVER be persisted in shared definition tables.
*Source*: CC-09; `env_core.cpp:116`

---

### 6.5 Error Recovery Contract

**ER-01 — Error = append, not throw**
All type errors are appended to `errors_` with a fallback type returned. No exception or abort.
*Source*: CC-12; `checker/resolve.cpp:359-369`

**ER-02 — Binary mismatch returns left operand type**
On a binary operator type mismatch, the error is recorded but the expression type is the LEFT operand type.
*Source*: CC-13; `checker/expr_ops.cpp:57-65`

**ER-03 — if result is then_type on branch mismatch**
`check_if` returns `then_type` even when else_type differs. A TML port that computes a common supertype will break HIR builder assumptions.
*Source*: CC-14; `checker/control.cpp:38-52`

**ER-04 — when result is first non-Never arm**
Diverging arms are ignored; result is first non-diverging arm type. Mismatched non-diverging arms record an error but do not change the result type.
*Source*: CC-15; `checker/control.cpp:101-143`

**ER-05 — Unknown type name = silent NamedType placeholder**
`resolve_type_path` encountering an unknown name returns `NamedType{name, "", {}}` with no error. Forward references and typos pass silently.
*Source*: CC-17; `checker/resolve.cpp:354-357`

---

### 6.6 Global State Contract

**GS-01 — Builtin lookup uses last path segment**
`builtins_` is looked up by the LAST segment. `core::I32` resolves as `I32`. Module prefix discarded.
*Source*: CC-26; `checker/resolve.cpp:274-277`

**GS-02 — Type resolution priority: builtins > aliases > structs > classes > enums > behaviors**
`resolve_type_path` checks in exactly this order. User type aliases shadow nothing — builtins override them.
*Source*: CC-05, CC-06; `checker/resolve.cpp:274-299`

**GS-03 — Never is synthesized on demand, not in builtins_**
`Never` is produced by `make_never()`. `builtins_["Never"]` does not exist.
*Source*: CC-25; `builtins/types.cpp:49-65`

**GS-04 — GlobalModuleCache is process-wide oracle**
Type checking of file A can resolve behavior impls from file B compiled earlier in the same process. `lookup_behavior` and method resolution fallbacks MUST include a GlobalModuleCache scan as the last step.
*Source*: I-4.34; `checker/expr_call_method.cpp:767`, `env_lookups.cpp:121-133`

**GS-05 — types_compatible accepts integer widening and ImplBehaviorType without verification**
`types_compatible` MUST accept: any integer vs any integer (any width), any float vs any float, `Ptr[Unit]` vs any pointer, ClosureType vs compatible FuncType, and any NamedType as satisfying any ImplBehaviorType without checking actual behavior registration. The last rule is load-bearing.
*Source*: CC-16; `checker/helpers.cpp:91-228`

**GS-06 — Operators do not dispatch to behavior methods**
The type checker MUST NOT look up `impl Add`, `impl Sub`, etc. Arithmetic returns left operand type; comparisons return Bool; assignment returns Unit. Operator desugaring happens in the THIR lowerer.
*Source*: I-4.29; `checker/expr_ops.cpp:37-183`

**GS-07 — lowlevel block changes & and &mut return types**
Inside `lowlevel`, `&` returns `Ptr[T]`, `&mut` returns `*mut T`. `in_lowlevel_` MUST be saved and restored on entry/exit to support nesting.
*Source*: CC-10, CC-58; `checker/expr_special.cpp:131-153`

---

### 6.7 Compatibility Test Plan

| Test ID | Contract | Assertion |
|---|---|---|
| TP-01 | R-01 | `Maybe`, `I64`, `Bool` accessible before any user decl |
| TP-02 | R-02 | Function calling a later-defined function type-checks correctly |
| TP-03 | R-03 | Code outside namespace calls function defined inside it |
| TP-04 | R-04 | Two snapshots from same base have independent `substitutions_` |
| TP-05 | R-05 | `struct Bool {}` emits T038; builtin Bool unchanged |
| TP-06 | R-06 | Double-registering struct results in second definition winning |
| TP-07 | R-07 | First registered overload of a name is returned by `lookup_func` |
| TP-08 | M-01 | Types from imported module resolvable in current module struct fields |
| TP-09 | M-02 | Circular `use` chain compiles without loop or error |
| TP-10 | IM-01 | `impl core::io::Write for Foo` stores "Write" not the FQN |
| TP-11 | IM-03 | `@derive(Display)` on generic struct does not register derived methods |
| TP-12 | IN-01 | `let x = 42` infers I64 |
| TP-13 | IN-02 | `unify(List[I32], List[I32])` is no-op |
| TP-14 | IN-05 | `check_call("Just", [42])` returns NamedType{"Maybe","",[]} |
| TP-15 | IN-06 | `Just(42).map(do(x) x.to_string())` has type `Maybe[I32]` |
| TP-16 | IN-07 | `.nonexistent_method()` on any type returns Unit, no error |
| TP-17 | ER-02 | `true + 42` records error; expression type is Bool |
| TP-18 | ER-03 | `if cond { 42 } else { "str" }` records error; type is I64 |
| TP-19 | ER-05 | Using undefined type `Foo` produces NamedType placeholder, no error |
| TP-20 | GS-03 | `builtins_["Never"]` absent; `return` expression has type Never |
| TP-21 | GS-07 | `&x` inside `lowlevel {}` has type `Ptr[T]`, not `ref T` |
| TP-22 | GS-05 | `types_compatible(I8, I64)` returns true |

---


---

## Appendix A — Unified Invariant Index

All 176 invariants listed with their section, ID, and primary source location.

### Section 1 — Type Registration (25 invariants)

| ID | Brief | File:Line |
|---|---|---|
| I-1.01 | Every TypePtr from a factory has a unique non-zero id | `type.cpp:36-44` |
| I-1.02 | types_equal uses id as fast-path shortcut, not sole identity | `type.cpp:317` |
| I-1.03 | types_equal does NOT resolve TypeVars | `env_core.cpp:65-92` |
| I-1.04 | NamedType equality requires both name AND module_path to match | `type.cpp:329-336` |
| I-1.05 | substitute_type treats bare NamedType{name,"",{}} as a type param | `type.cpp:476-479` |
| I-1.06 | RefType equality ignores lifetime annotation | `type.cpp:339-342` |
| I-1.07 | FuncType equality: async flag matters | `type.cpp:355-366` |
| I-1.08 | ClosureType equality includes captures (name, type, mutability) | `type.cpp:367-386` |
| I-1.09 | GenericType equality compares name only, not bounds | `type.cpp:388-390` |
| I-1.10 | ConstGenericType equality: name + value_type, not resolved_value | `type.cpp:391-393` |
| I-1.11 | ClassType/InterfaceType equality: name + module_path + type_args | `type.cpp:415-434` |
| I-1.12 | TypeEnv constructor unconditionally calls init_builtins() | `env_core.cpp:32-35` |
| I-1.13 | TypeChecker creates TypeEnv via BuiltinsSnapshot::create_env() | `builtins_cache.cpp:17-25` |
| I-1.14 | snapshot() copies definition tables, resets inference/module state | `env_core.cpp:102-122` |
| I-1.15 | Four core enums registered in enums_ before any user code | `builtins/types.cpp:69-107` |
| I-1.16 | Four core behaviors registered in behaviors_ before any user code | `builtins/types.cpp:110-184` |
| I-1.17 | Four passes are strictly sequential, no interleaving | `checker/core.cpp:203-255` |
| I-1.18 | resolve_type during pass 1 does not validate that referenced types exist | `checker/core.cpp:209-229` |
| I-1.19 | Super-behavior names stored as strings; definition not required at registration | `checker/core.cpp:342-453` |
| I-1.20 | check_impl_decl attempts on-demand behavior load from binary cache (14 behaviors) | `checker/core.cpp:1283-1327` |
| I-1.21 | Specialized impl methods registered under BOTH base and specialized keys | `checker/core.cpp:1232-1268` |
| I-1.22 | Namespace blocks run their own 3-pass sequence during outer pass 1 | `checker/core.cpp:278-334` |
| I-1.23 | Reserved names emit T038 and are silently dropped | `checker/core.cpp:344-349, 457-462` |
| I-1.24 | types_match is weaker than types_equal (ignores module_path and type_args for NamedType) | `env_lookups.cpp:138-166` |
| I-1.25 | unify() has no occurs-check; cycle guard in resolve_impl is lazy | `env_core.cpp:57-63` |

### Section 2 — Module Resolution (23 invariants)

| ID | Brief | File:Line |
|---|---|---|
| INV-01 | Module registered at most once per TypeEnv | `env_module_loading.cpp:179` |
| INV-02 | loading_modules_ guard; circular detection returns true not error | `env_module_load.cpp:309-331` |
| INV-03 | GlobalModuleCache caches only core::*, std::*, test | `env_module_loading.cpp:184` |
| INV-04 | behavior_impls re-registered from cache before module is placed in registry | `env_module_loading.cpp:256-260, 352-356` |
| INV-05 | Binary meta cache accepted only on magic + version 8 + CRC32C match | `module_binary.hpp:46-56` |
| INV-06 | s_resolved_paths and s_not_found_paths are process-scoped, never invalidated | `env_module_loading.cpp:74-141` |
| INV-07 | Directory modules parse ALL .tml files in the directory | `env_module_load.cpp:340-392` |
| INV-08 | Sibling .tml files do NOT eagerly load external dependencies | `env_module_load_decls.cpp:926-930` |
| INV-09 | Private structs/enums in internal_structs/internal_enums; lookup_struct ignores internal | `env_module_load_decls.cpp:445-501` |
| INV-10 | Generic impl methods registered regardless of visibility | `env_module_load_decls.cpp:557-561` |
| INV-11 | Specialized impl: two keys (base + discriminated) in mod.functions | `env_module_load_decls.cpp:643-651` |
| INV-12 | Default behavior methods registered in pass 2 only, after behaviors populated | `env_module_load_decls.cpp:1073-1156` |
| INV-13 | UseDecl processed before all other declarations in check_module | `checker/core.cpp:203-261` |
| INV-14 | silent=false emits error on missing module; abort_on_module_error_ controls parse abort | `env_module_loading.cpp:502-507` |
| INV-15 | re_exports and private_imports loaded transitively after module registration | `env_module_load.cpp:481-503` |
| INV-16 | Re-export loading is symmetric across all three load paths | `env_module_loading.cpp:244-295, 343-388` |
| INV-17 | has_pure_tml_functions true when any function/method/class body or public const exists | `env_module_load_decls.cpp:1177-1224` |
| INV-18 | source_code stores preprocessed (post-#if) output, not raw source | `env_module_load_decls.cpp:1246-1250` |
| INV-19 | Relative imports prefixed with current module path | `env_module_load_decls.cpp:909-913` |
| INV-20 | process_use_decl implements two-level re-export fallback | `checker/core.cpp:490-541, 637-680` |
| INV-21 | Single-path import does NOT emit T027 on missing module | `checker/core.cpp:617-693` |
| INV-22 | s_lib_root resolved once per process; empty string on failure | `env_module_loading.cpp:83-113` |
| INV-23 | FuncSig::is_lowlevel set from parser::FuncDecl::is_unsafe | `env_module_load_decls.cpp:408, 639` |

### Section 3 — Impl Processing (33 invariants)

| ID | Brief | File:Line |
|---|---|---|
| I-3.1 | BehaviorDef must exist before default methods propagated | `checker/core.cpp:1285-1327` |
| I-3.2 | behavior_impls_ is a vector; duplicates are harmless | `env_lookups.cpp:460-462` |
| I-3.3 | Builtin behavior impls registered at TypeEnv construction | `builtins/types.cpp:193-248` |
| I-3.4 | Behavior name always extracted as segments.back() (short name only) | `checker/core.cpp:1277` |
| I-3.5 | Default method propagation in checker's register_impl_decl, not in register_impl | `checker/core.cpp:1329-1374` |
| I-3.6 | Only 14 hardcoded behaviors have on-demand default propagation | `checker/core.cpp:1296-1314` |
| I-3.7 | @derive skips generic types (decl.generics.empty() guard) | `checker/decl_struct.cpp:173` |
| I-3.8 | Struct full_name uses qualified_name() with '.' separator; enum uses plain short name | `checker/decl_struct.cpp:92, 736` |
| I-3.9 | Methods keyed as "TypeName::method_name" using short type name | `checker/core.cpp:1233` |
| I-3.10 | Specialized impl: double registration under base and specialized keys | `checker/core.cpp:1256-1268` |
| I-3.11 | Class methods in ClassDef.methods, NOT in functions_ | `checker/core_oop.cpp:264-322` |
| I-3.12 | lookup_func returns second[0] — first registered overload, ignoring arity | `env_lookups.cpp:175-177` |
| I-3.13 | lookup_func_overload does NOT search GlobalModuleCache | `env_lookups.cpp:292-333` |
| I-3.14 | First-registered method wins via lookup_func when inherent and behavior share a key | `env_lookups.cpp:175-177` |
| I-3.15 | type_implements returns bool; cannot distinguish direct vs inherited | `env_lookups.cpp:491-562` |
| I-3.16 | Send/Sync auto-derived structurally without register_impl | `env_lookups.cpp:515-558` |
| I-3.17 | Behavior methods must be pre-registered in functions_; no vtable walk | `env_lookups.cpp:168-289` |
| I-3.18 | No duplicate impl detection — two impl B for T blocks silently coexist | `env_definitions.cpp:39-41` |
| I-3.19 | Override return type comparison uses exact types_equal, not subtype | `checker/core_oop.cpp:723-728` |
| I-3.20 | Interface completeness checked by name only, not signature | `checker/core_oop.cpp:815` |
| I-3.21 | Abstract method check only fires for non-abstract classes with extends | `checker/core_oop.cpp:456-459` |
| I-3.22 | Extend blocks (inherent) do NOT add to behavior_impls_ | `checker/core.cpp:1272-1283` |
| I-3.23 | Private behavior impl and generic impl methods ARE registered; private inherent not | `env_module_load_decls.cpp:557-562` |
| I-3.24 | Struct field lookup is linear; field order matches declaration order | `checker/decl_struct.cpp:66-79` |
| I-3.25 | Enum variant payload is vector<TypePtr>; unit variants have empty payload | `checker/decl_struct.cpp:532-539` |
| I-3.26 | Direct recursive enum rejected with T085 before define_enum | `checker/decl_struct.cpp:589-598` |
| I-3.27 | lookup_enum searches ALL loaded modules including internal_enums | `env_lookups.cpp:55-87` |
| I-3.28 | lookup_behavior searches GlobalModuleCache as final fallback | `env_lookups.cpp:89-136` |
| I-3.29 | Generic impl methods stored with type_params set; no concrete substitution at registration | `checker/core.cpp:1233-1268` |
| I-3.30 | Specialized impl key uses __ separator (matches codegen mangling) | `checker/core.cpp:1260` |
| I-3.31 | impl_self_type_args populated only for specialized impls with type args | `checker/core.cpp:1226-1229` |
| I-3.32 | Where-clause type equalities in current_associated_types_ are ephemeral (per impl) | `checker/core.cpp:1196-1214` |
| I-3.33 | (BUG) behaviors_ keyed by short name; same-short-name behaviors from different modules collide | `env_definitions.cpp:36` |

### Section 4 — Body Checking and Inference (34 invariants)

| ID | Brief | File:Line |
|---|---|---|
| I-4.01 | Expected-type propagation only for LiteralExpr, UnaryExpr (neg), CallExpr, ArrayExpr, TupleExpr, ClosureExpr | `checker/expr.cpp:215-296` |
| I-4.02 | Unsuffixed integer literal defaults to I64 | `checker/expr.cpp:379` |
| I-4.03 | Overflow checked for suffixed literals and expected-type integers | `checker/expr.cpp:308-322` |
| I-4.04 | Negative literal to unsigned type = T050 | `checker/expr.cpp:229-233` |
| I-4.05 | null has type Ptr[Unit] | `checker/expr.cpp:405` |
| I-4.06 | Variable reads tracked in read_vars_ regardless of lookup success | `checker/expr.cpp:413` |
| I-4.07 | resolve_imported_symbol called twice for same symbol (struct/enum check then constant check) | `checker/expr.cpp:457, 506` |
| I-4.08 | Char constant maps to U32 internally | `checker/expr.cpp:541` |
| I-4.09 | unify is one-sided; no structural decomposition | `env_core.cpp:57-63` |
| I-4.10 | unify has no occurs-check; cycle guard is lazy | `env_core.cpp:57-63` |
| I-4.11 | resolve is shallow (top-level TypeVar only) | `env_core.cpp:65-92` |
| I-4.12 | Closure inferred types stored back in AST via shared_ptr<void> | `checker/types_checker.cpp:310-314` |
| I-4.13 | Closure param types inferred via unify at call sites | `checker/expr.cpp:699-719` |
| I-4.14 | Third write to inferred types after unification (can overwrite) | `checker/expr.cpp:282-289` |
| I-4.15 | Enum constructor call returns bare NamedType with empty type_args | `checker/expr_call.cpp:391-393` |
| I-4.16 | Generic function arguments type-checked twice (overload check + substitution) | `checker/expr_call.cpp:270, 286` |
| I-4.17 | Parameterized where-clause checks base behavior only | `checker/expr_call.cpp:346-350` |
| I-4.18 | extract_type_params overwrites existing substitutions on conflict | `checker/expr_call.cpp:46` |
| I-4.19 | TypeVar stored unresolved in substitutions map | `checker/expr_call.cpp:46` |
| I-4.20 | Optional chaining (?.) flattens Maybe[Maybe[V]] to Maybe[V] | `checker/expr_call_method.cpp:229` |
| I-4.21 | where-clause bound substitutes type param name, not resolved type | `checker/expr_call_method.cpp:937-940` |
| I-4.22 | Dynamic primitive method lookup: no generic substitution applied | `checker/expr_call_method.cpp:1094` |
| I-4.23 | Unknown method calls return Unit silently (no diagnostic) | `checker/expr_call_method.cpp:1360` |
| I-4.24 | No monomorphization cache at type-check time; substitution computed per call site | `checker/expr_call_method.cpp:655-679` |
| I-4.25 | impl_self_type_args drives specialized dispatch (RefType in args = specialized pattern) | `checker/expr_call_method.cpp:557-619` |
| I-4.26 | Maybe[T].map(f) returns Maybe[T], not Maybe[U] | `checker/expr_call_method_types.cpp:67-69` |
| I-4.27 | and/or operands not verified to be Bool | `checker/expr_ops.cpp:136-138` |
| I-4.28 | Deref of non-pointer/non-ref returns type unchanged, no error | `checker/expr_ops.cpp:200+` |
| I-4.29 | Operator overloading NOT checked in type checker; happens in THIR lowerer | `checker/expr_ops.cpp:37-183` |
| I-4.30 | when arm type mismatch: result is first non-Never arm type (see CC-15) | `checker/control.cpp:101-143` |
| I-4.31 | ! on non-Outcome/Maybe: no error; returns original type | `checker/types_checker.cpp:344` |
| I-4.32 | break and continue have type Never | `checker/expr.cpp:174, 177` |
| I-4.33 | First-match wins in multi-module method lookup (unordered map iteration) | `checker/expr_call_method.cpp:751-778` |
| I-4.34 | GlobalModuleCache is process-wide singleton; cross-unit type resolution | `checker/expr_call_method.cpp:767` |

### Section 5 — Cross-Cutting Invariants (61 invariants)

See Section 5 summary table (CC-01 through CC-61) in the body of this document.

| Range | Category |
|---|---|
| CC-01 to CC-04 | Phase ordering dependencies |
| CC-05 to CC-06 | Type resolution priority |
| CC-07 to CC-09 | Global state safety |
| CC-10 to CC-11 | Per-function checker state |
| CC-12 to CC-17 | Error recovery |
| CC-18 to CC-24 | Scope chain rules |
| CC-25 to CC-29 | Builtin type registration |
| CC-30 to CC-34 | Operator checking |
| CC-35 to CC-39 | Statement and pattern checking |
| CC-40 to CC-46 | Control flow typing |
| CC-47 to CC-50 | Constant evaluation |
| CC-51 to CC-53 | Type resolution and inference |
| CC-54 | "Did you mean?" suggestions |
| CC-55 | Return coverage analysis |
| CC-56 to CC-57 | Import conflict handling |
| CC-58 | lowlevel block semantics |
| CC-59 | await expression typing |
| CC-60 to CC-61 | Closure capture analysis |

**Total invariants: 25 + 23 + 33 + 34 + 61 = 176**

---

## Appendix B — Latent Bugs and Surprising Findings

These are behaviours discovered during the audit that are incorrect (bugs) or surprising but load-bearing (must be replicated). A self-hosted TML type checker must replicate ALL of these until a coordinated fix is landed.

**B-01 — FQN collision in behaviors_ (I-3.33, phase0i)**
`behaviors_["Write"]` is overwritten when both `core::io::Write` and `core::fmt::Write` are loaded. The last-loaded behavior wins. This causes wrong default methods and LLVM GEP errors when both behaviors are used. Fix: migrate all maps to FQN keying simultaneously across type checker and codegen. Tracked in `phase0i_behavior-fqn-keying`.
*Source*: `env_definitions.cpp:36`, `env_lookups.cpp:460-462`

**B-02 — ClosureType collapse at codegen boundary (I-4 Finding 6)**
The type checker correctly preserves `ClosureType`. When codegen re-parses mangled names (e.g., `RepeatWith__Fn`), it reconstructs `NamedType{"Fn","",{}}` instead of the original ClosureType. The bug is in `generic_instantiate.cpp:273-276`, not in the type checker. A self-hosted type checker is NOT responsible for this bug.
*Source*: `compiler/src/codegen/llvm/core/generic_instantiate.cpp:273-276`

**B-03 — Enum constructors lose type arguments (I-4.15)**
`Just(42)` returns `NamedType{"Maybe","",{}}` with empty `type_args`. This is intentional simplification — the type checker does not infer the generic argument of enum constructors at call sites. The HIR builder works around this by inferring the type argument separately.
*Source*: `checker/expr_call.cpp:391-393`

**B-04 — Maybe[T].map(f) loses type argument (I-4.26)**
`map`, `and_then`, `or_else`, `filter` on `Maybe[T]` all return `Maybe[T]` regardless of the closure's return type. Chained `.map()` operations silently discard the transformation's type. HIR builder must infer the actual type from the closure's signature.
*Source*: `checker/expr_call_method_types.cpp:67-69`

**B-05 — types_compatible accepts ImplBehaviorType without verification (CC-16, Finding 3)**
Any `NamedType` passes `types_compatible` against an `ImplBehaviorType` without verifying actual behavior registration. This is load-bearing for body checking — real conformance verification is Phase 3's job. A TML port that adds verification here will reject programs that currently compile.
*Source*: `checker/helpers.cpp:91-228`

**B-06 — Fallback type for unresolved types is I32, not an error (Section 2, Finding 7)**
`resolve_simple_type` in module loading returns `make_primitive(PrimitiveKind::I32)` for any type expression it cannot resolve. The debug message is unreachable (dead code). Any type that maps to nothing in `resolve_simple_type` silently becomes `I32` in method signatures stored in `Module::functions`.
*Source*: `env_module_load_decls.cpp:344-349`

**B-07 — Base key collision for multiple specialized impls (Section 2, Finding 2)**
Multiple `impl[T] Pin[ref T]` and `impl[T] Pin[Heap[T]]` both write to `mod.functions["Pin::method"]`. The last write wins. Only the discriminated keys (`Pin[ref]::method`, `Pin[Heap]::method`) preserve both impls. Callers using the base key may silently dispatch to the wrong specialization.
*Source*: `env_module_load_decls.cpp:643-650`

**B-08 — @derive silently skips all generic types (I-3.7)**
A generic struct with `@derive(Display, Hash, Eq)` gets no derived methods registered and no error. This is the current design — generic types wait for monomorphization. But there is no error or warning to help developers catch the case where they wrote `@derive` on a generic type expecting it to work.
*Source*: `checker/decl_struct.cpp:173`

**B-09 — Duplicate ParsedModuleFile definition in two translation units (Section 2, Finding 1)**
`ParsedModuleFile` and several helper functions are defined as static in both `env_module_load.cpp` and `env_module_load_decls.cpp`. A change to one copy that is not mirrored in the other introduces silent divergence. This is fragile but not currently a bug.
*Source*: `env_module_load.cpp:46-235`, `env_module_load_decls.cpp:13-204`

**B-10 — Duplicate resolve_imported_symbol call for constants (I-4.07)**
`check_ident` calls `resolve_imported_symbol` at line 457 (for struct/enum lookup) and again at line 506 (for constant lookup). If the first call succeeds but the result is a struct, the second call hits the constants path. No functional bug, but each call records side effects and both fire.
*Source*: `checker/expr.cpp:457, 506`

**B-11 — let-else else block not verified to diverge (CC-37)**
The `else` block of `let X = e else { block }` is not verified to have type `Never`. The checker accepts `let Just(x) = e else { println("ok") }` with no error. HIR lowering is expected to enforce this.
*Source*: `checker/stmt.cpp:137-177`

---

## Appendix C — Known Gaps

The following areas were not covered by this audit and represent gaps in the invariant documentation:

**Gap 1 — HIR builder assumptions about TypeEnv**
The HIR builder (`hir_builder.cpp`, `hir_builder_expr.cpp`) makes specific assumptions about what TypeEnv guarantees. These assumptions are not documented here. A complete self-hosting contract requires auditing the HIR builder's TypeEnv consumption patterns.

**Gap 2 — THIR lowerer type-checker interactions**
The THIR lowerer (`thir_lower.cpp`) resolves operator desugaring and method calls using TypeEnv. The exact TypeEnv fields it reads (beyond what body checking produces) are not audited here.

**Gap 3 — Borrow checker TypeEnv usage**
The borrow checker (`borrow/checker.cpp`) reads TypeEnv fields to determine type ownership and reference validity. Its exact requirements from TypeEnv are not covered by this audit.

**Gap 4 — Codegen TypeEnv re-use**
The legacy LLVM codegen (`codegen/llvm/`) re-runs parts of the type checking (notably module loading and type resolution) during codegen. The exact requirements from TypeEnv for the MIR codegen path are documented in part in Section 3 (method key conventions) but not exhaustively.

**Gap 5 — when arm type mismatch exact behavior**
I-4.30 notes that the exact behavior of `check_when` with mismatched arm types is based on reading Section 5 (CC-15) rather than direct verification against `control.cpp`. The claim should be verified with a targeted test.

**Gap 6 — @derive for specific behaviors (Eq, Ord, Hash, etc.)**
The derived method signatures (parameter types, return types) for each supported `@derive` target are documented in `decl_struct.cpp` but are not fully enumerated in this invariant doc. A self-hosted `@derive` must produce the exact same method signatures.

---

## Appendix D — Terminology Glossary

| Term | Definition |
|---|---|
| `behavior` | TML trait/interface mechanism. Declared with `behavior Name { ... }`. |
| `behavior impl` | An `impl Behavior for Type { ... }` block. Registers into `behavior_impls_`. |
| `extend block` | An `impl Type { ... }` block with no `trait_type`. Registers methods into `functions_`. Does NOT register into `behavior_impls_`. |
| `TypeVar` | An unresolved inference variable `?N`. Created by `fresh_type_var()`. Valid only within one compilation unit's body-checking pass. |
| `TypeEnv` | The central type environment holding all definition tables and per-file state. |
| `snapshot` | A `TypeEnv` created by `TypeEnv::snapshot()`. Shares definition tables, fresh inference state. |
| `BuiltinsSnapshot` | A singleton that creates one TypeEnv, populates builtins, and clones it on demand. |
| `GlobalModuleCache` | Process-scoped singleton holding `Module` values for `core::*`, `std::*`, and `test` modules. |
| `ModuleRegistry` | Per-TypeEnv map from module path to `Module`. |
| `FQN` | Fully-qualified name, e.g., `core::io::Write`. |
| `short name` | Unqualified last path segment, e.g., `Write`. |
| `FuncSig` | A function signature stored in `functions_`. Holds name, params, return type, type_params, is_async. |
| `StructDef` | A struct definition stored in `structs_`. Holds name, fields, generics. |
| `EnumDef` | An enum definition stored in `enums_`. Holds name, variants (name + payload types). |
| `BehaviorDef` | A behavior (trait) definition stored in `behaviors_`. Holds name, methods, super_behaviors. |
| `types_equal` | Structural equality function in `type.cpp`. Does NOT resolve TypeVars. |
| `types_match` | Weaker equality in `env_lookups.cpp`. Ignores module_path and type_args for NamedType. |
| `types_compatible` | Compatibility check in `helpers.cpp`. Accepts integer widening, ImplBehaviorType without verification. |
| `substitute_type` | Replaces GenericType and bare NamedType entries in a type tree per a substitution map. |
| `resolve` | Follows TypeVar substitution chain to ground a type variable (shallow — top level only). |
| `unify` | One-sided substitution assignment. No occurs-check, no structural decomposition. |
| `Never` | The bottom type. Produced by `make_never()`. `break`, `continue`, `return`, `panic`. Not in `builtins_`. |
| `lowlevel` | A block where `in_lowlevel_` is true. `&` produces `Ptr[T]`, `&mut` produces `*mut T`. |
| `@derive` | A decorator that auto-generates behavior impls for non-generic types. |
| `check_module` | The top-level type-checking function. Runs 4 passes over module declarations. |
| `register_impl` | Records `type_name -> behavior_name` in `behavior_impls_`. Appends (no dedup). |
| `define_func` | Appends a FuncSig to `functions_[name]`. |
| `extract_type_params` | Structural matching to infer type variable bindings from argument types. |
| `build_receiver_subs` | Builds substitution map from receiver type's type args for method dispatch. |

---

*End of TML Type Checker Invariants — Consolidated Reference*

*Audit date: 2026-04-06. Source files: 38 type checker C++ files analyzed.*
*Total invariants: 176 (25 + 23 + 33 + 34 + 61). Section 6 contract items: 37 (R-01..07, M-01..07, IM-01..07, IN-01..08, ER-01..05, GS-01..07).*
