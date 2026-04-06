# Proposal: Type Checker — Type Registration (Sub-phase 2a)

## Problem

The TML type checker's first phase populates `TypeEnv` with all type declarations before any
inference or checking begins. This phase is currently implemented in C++ across `type.cpp` (965
LOC), `env_core.cpp`, `env_definitions.cpp`, `env_scope.cpp`, `builtins_cache.cpp`, and ten
`builtins/*.cpp` files (~5,302 LOC total). Porting this phase to TML is the entry point for
self-hosting the type checker and unblocks all subsequent checking sub-phases (14b–14d).

## Solution

Port the type representation and environment initialization code to four TML modules under
`compiler-tml/src/types/`:

- `type.tml` — `Type` as a TML enum replacing the C++ class hierarchy in `type.hpp`
- `env.tml` — `TypeEnv` struct replacing `TypeEnv` fields and methods from `env.hpp`
- `builtins.tml` — builtin type/behavior registration replacing `builtins/*.cpp`
- `register.tml` — AST declaration walker replacing `env_definitions.cpp` and checker `core.cpp`
  phase-1 logic

## Key Design Decisions

**Type as enum, not class hierarchy.** The C++ code uses a `Type` base class with ~15 derived
classes. TML enums with payload variants express the same structure more concisely and enable
exhaustive `when` matching throughout the checker, eliminating virtual dispatch and downcasts.

**HashMap-backed TypeEnv.** `TypeEnv` holds three `HashMap[Str, ...]` maps (types, functions,
behaviors). Lookup is O(1) average. The C++ implementation uses `std::unordered_map` with the
same semantics, so the port is direct.

**Scope chain via List[Scope].** Each `Scope` holds a `Maybe[ref Scope]` parent pointer and a
local `HashMap[Str, Type]`. `lookup` walks the chain from innermost scope to module root. This
matches the C++ `env_scope.cpp` algorithm exactly.

**Builtin registration at env construction.** `TypeEnv.new()` calls `register_builtins(self)`
immediately, matching `env_core.cpp`'s constructor behavior. All 14 primitives, 13 behaviors,
5 memory types, and 4 collection types are registered before any user code is processed.

**Generic substitution in-tree.** `Type.substitute(params)` performs deep substitution on
`Type::Generic` variants, enabling monomorphization without a separate pass. This replaces the
C++ `type.cpp` `substitute` method.

## Files Changed

| File | Purpose |
|------|---------|
| `compiler-tml/src/types/type.tml` | Type enum, equality, display, substitution, size/align |
| `compiler-tml/src/types/env.tml` | TypeEnv struct, Scope, register/lookup operations |
| `compiler-tml/src/types/builtins.tml` | All builtin type and behavior registration |
| `compiler-tml/src/types/register.tml` | AST declaration walker — structs, enums, functions |

## Success Criteria

Differential testing: serialize the TML `TypeEnv` after registration and compare field-by-field
with C++ `TypeEnv` output on the same input. Zero differences on all 20 stdlib modules and the
full test suite constitutes a passing port. The C++ registration phase remains the reference
until phase14d completes.

## Dependencies

- **Requires**: phase13d (TML frontend parsed and AST available in TML), phase12c (type system
  invariant document for correctness reference)
- **Blocks**: phase14b (module resolution reads the populated TypeEnv to resolve imports)
- **Duration**: 6–8 weeks
- **Risk**: Medium — type representation and builtin registration are well-scoped with no
  constraint solving or inference logic in scope
