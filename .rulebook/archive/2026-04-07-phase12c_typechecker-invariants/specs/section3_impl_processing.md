# Section 3 — Impl Processing

**Document**: TML Type Checker Invariants — Section 3  
**Scope**: `compiler/src/types/checker/core_oop.cpp` (1067 LOC),
           `compiler/src/types/checker/decl_struct.cpp` (1207 LOC),
           `compiler/src/types/env_lookups.cpp` (1265 LOC)  
**Purpose**: Reference specification for the self-hosting port. Documents what the
code actually does, including known bugs. Fixes go in their own tasks.

---

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
