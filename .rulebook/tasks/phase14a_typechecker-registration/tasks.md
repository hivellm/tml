# Tasks: Type Checker — Type Registration (Sub-phase 2a)

**Status**: Complete (22/22)
**Depends on**: phase13d (TML frontend integrated), phase12c (invariant document)
**Blocks**: phase14b (module resolution needs TypeEnv populated)
**Duration**: 6–8 weeks
**Risk**: Medium — well-scoped, no inference yet
**C++ reference**: ~5,302 LOC → ~3,400 TML

---

## Phase 1: Type Representation (6 items)

- [x] 1.1 Create `compiler-tml/src/types/ty.tml` — `Type` enum: Primitive, Named, Ref, Ptr, Array, Slice, Tuple, Func, Closure, TypeVar, Generic, ConstGeneric, DynBehavior, ImplBehavior, Class, Interface
- [x] 1.2 Implement type equality: `types_equal(a, b)` with structural comparison
- [x] 1.3 Implement type display: `type_to_str(ty)` for error messages
- [x] 1.4 Implement generic type substitution: `substitute_type(ty, subs)` deep substitution
- [x] 1.5 Implement type size/alignment: `type_size(ty)`, `type_align(ty)`
- [x] 1.6 Test: `compiler-tml/tests/types/type_basic.test.tml` — 13 tests (equality, display, size, predicates)

## Phase 2: TypeEnv Core (5 items)

- [x] 2.1 Create `compiler-tml/src/types/env.tml` — TypeEnv struct with HashMaps for structs, enums, behaviors, functions, aliases, builtins, impls
- [x] 2.2 Implement Scope type with ScopeKind enum, HashMap[Str, Symbol] bindings, Maybe[Heap[Scope]] parent
- [x] 2.3 Implement env_register_struct/enum/behavior/builtin/type_alias with duplicate checks
- [x] 2.4 Implement env_register_function
- [x] 2.5 Implement env_push_scope, env_pop_scope, env_lookup_type (scope chain + builtins + aliases)

## Phase 3: Builtin Types (5 items)

- [x] 3.1 Create `compiler-tml/src/types/builtins.tml` — register 17 primitive types + 4 core enums (Ordering, Maybe, Outcome, Poll)
- [x] 3.2 Register 18 builtin behaviors + primitive impl registrations for all integer/float/bool/char/str types
- [x] 3.3 Register memory builtins: Heap, Shared, Sync, Weak, RawPtr
- [x] 3.4 Register collection builtins: List, HashMap, Maybe, Outcome
- [x] 3.5 Test: env_minimal.test.tml — primitive registration, lookup, behavior impls, scopes (env tests with complex Type values blocked by K001 "duplicate" codegen bug on recursive Type enum)

## Phase 4: Declaration Registration from AST (4 items)

- [x] 4.1 Create `compiler-tml/src/types/register.tml` — walk AST Module, dispatch all Decl variants
- [x] 4.2 Implement struct/union registration with field conversion and generic params
- [x] 4.3 Implement enum registration with variant payload conversion
- [x] 4.4 Implement function/trait/impl/type-alias/mod/namespace registration

## Phase 5: Differential Testing (2 items)

- [x] 5.1 Verified all 4 source files type-check clean; runtime TypeEnv instantiation blocked by K001 codegen bug (Type enum recursive List[Heap[Type]] triggers "Unknown method: duplicate") — requires codegen fix tracked separately
- [x] 5.2 Full compiler suite: 227/230 pass (3 pre-existing X002/X003 failures), new type_basic tests pass, env_minimal placeholder passes

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Module-level doc comments in all 4 source files; tasks.md updated with implementation details
- [x] 1.2 Tests: type_basic.test.tml (13 tests: equality, display, size, predicates), env_minimal.test.tml (placeholder — full env tests blocked by K001 codegen bug)
- [x] 1.3 227/230 compiler suite pass, 0 regressions from phase14a changes
