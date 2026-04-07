# Tasks: Type Checker — Type Registration (Sub-phase 2a)

**Status**: Planned (0/22)
**Depends on**: phase13d (TML frontend integrated), phase12c (invariant document)
**Blocks**: phase14b (module resolution needs TypeEnv populated)
**Duration**: 6–8 weeks
**Risk**: Medium — well-scoped, no inference yet
**C++ reference**: ~5,302 LOC → ~3,400 TML

---

## Phase 1: Type Representation (6 items)

- [ ] 1.1 Create `compiler-tml/src/types/type.tml` — `Type` enum: Primitive, Struct, Enum, Func, Generic, Ref, Ptr, Array, Tuple, Never, Unit, Unknown
- [ ] 1.2 Implement type equality: `Type.equals(other: Type) -> Bool` with structural comparison
- [ ] 1.3 Implement type display: `Type.to_str() -> Str` for error messages
- [ ] 1.4 Implement generic type substitution: `Type.substitute(params: HashMap[Str, Type]) -> Type`
- [ ] 1.5 Implement type size/alignment calculation: `Type.size() -> I64`, `Type.align() -> I64`
- [ ] 1.6 Test: round-trip all Type variants through serialize/deserialize

## Phase 2: TypeEnv Core (5 items)

- [ ] 2.1 Create `compiler-tml/src/types/env.tml` — `TypeEnv` struct: types (HashMap), functions (HashMap), behaviors (HashMap), scopes (List[Scope])
- [ ] 2.2 Implement `Scope` type: parent scope ref, local bindings, scope kind (Module/Function/Block)
- [ ] 2.3 Implement `TypeEnv.register_type(name: Str, ty: Type)` — add to types map, error on duplicate
- [ ] 2.4 Implement `TypeEnv.register_function(name: Str, sig: FuncSig)` — add to functions map
- [ ] 2.5 Implement `TypeEnv.push_scope()`, `pop_scope()`, `lookup(name: Str) -> Maybe[Type]` with scope chain

## Phase 3: Builtin Types (5 items)

- [ ] 3.1 Create `compiler-tml/src/types/builtins.tml` — register all primitive types: I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, Bool, Str, Unit, Never
- [ ] 3.2 Register builtin behaviors: Add, Sub, Mul, Div, Eq, Ord, Hash, Display, Debug, Clone, Copy, Drop, Iterator
- [ ] 3.3 Register memory builtins: Heap[T], Shared[T], Sync[T], Weak[T], RawPtr
- [ ] 3.4 Register collection builtins: List[T], HashMap[K,V], Maybe[T], Outcome[T,E]
- [ ] 3.5 Test: TypeEnv after builtin registration has all expected types and behaviors

## Phase 4: Declaration Registration from AST (4 items)

- [ ] 4.1 Create `compiler-tml/src/types/register.tml` — walk AST Module, register all declarations
- [ ] 4.2 Implement struct registration: extract fields, generic params, where clauses → register Type::Struct
- [ ] 4.3 Implement enum registration: extract variants with payload types → register Type::Enum
- [ ] 4.4 Implement function registration: extract params, return type, generic params → register FuncSig

## Phase 5: Differential Testing (2 items)

- [ ] 5.1 Register types from 20 stdlib modules → serialize TypeEnv → compare with C++ TypeEnv output
- [ ] 5.2 Register types from full test suite → verify zero diffs against C++ registration phase

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
