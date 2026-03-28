# Proposal: Implement Rust Core Library in TML

## Why

TML currently has 27% coverage of Rust's core library (9/33+ modules). While basic types like `Maybe[T]` and `Outcome[T,E]` exist, critical behaviors (traits) are missing that prevent ergonomic, idiomatic code. Without behaviors like `Clone`, `Ord`, and operator overloading (`Add`, `Sub`, `Index`), LLMs cannot generate code that feels natural and type-safe.

**Key pain points:**
- No `Clone` behavior → can't duplicate non-Copy values
- No `Ord`/`PartialEq` → can't sort collections or compare custom types
- No operator overloading → must write `list.get(i)` instead of `list[i]`, `a.add(b)` instead of `a + b`
- No `Default` behavior → can't create zero-values generically
- No `Display`/`Debug` → can't print custom types idiomatically

**Impact**: With just 5 critical behaviors implemented, TML would achieve **90% of Rust's ergonomics** for LLM code generation.

## What Changes

### Phase 1: Critical Behaviors (Weeks 1-2)

#### 1. **core::clone** (`packages/core/src/clone.tml`)
- `Clone` behavior - explicit duplication
- `Copy` behavior - implicit copying (marker)
- Enables: Working with collections, passing values without moves

#### 2. **core::cmp** (`packages/core/src/cmp.tml`)
- `Ordering` type (Less, Equal, Greater)
- `PartialEq` behavior - equality comparison
- `Ord` behavior - full ordering
- Enables: Sorting, searching, custom comparisons

#### 3. **core::ops** (`packages/core/src/ops.tml`)
- `Add`, `Sub`, `Mul`, `Div` behaviors - arithmetic operators
- `Index` behavior - `collection[i]` syntax
- `Neg`, `Not` behaviors - unary operators
- Enables: Natural syntax, operator overloading

#### 4. **core::default** (`packages/core/src/default.tml`)
- `Default` behavior - zero/empty values
- Enables: Generic initialization, builder patterns

#### 5. **core::fmt** (`packages/core/src/fmt.tml`)
- `Display` behavior - user-facing formatting
- `Debug` behavior - developer formatting
- Enables: Custom printing, debugging

### Phase 2: Conversions & Utilities (Weeks 3-4)

#### 6. **core::convert** (`packages/core/src/convert.tml`)
- `From`, `Into` behaviors - infallible conversions
- `TryFrom`, `TryInto` behaviors - fallible conversions
- Enables: Type conversions, ergonomic APIs

#### 7. **core::hash** (`packages/core/src/hash.tml`)
- `Hash` behavior - custom hashing
- `Hasher` interface
- Enables: HashMap with custom types

#### 8. **core::borrow** (`packages/core/src/borrow.tml`)
- `Borrow`, `BorrowMut` behaviors
- Enables: Generic borrowing patterns

#### 9. **core::marker** (`packages/core/src/marker.tml`)
- `Send`, `Sync` marker behaviors
- Enables: Thread-safety guarantees

#### 10. **Expand core::mem**
- Add `size_of()`, `align_of()`, `swap()`, `replace()`

### Phase 3: Advanced Features (Weeks 5-6)

#### 11. **core::cell** (`packages/core/src/cell.tml`)
- `Cell[T]`, `RefCell[T]` - interior mutability

#### 12. **core::str** (`packages/core/src/str.tml`)
- String slice utilities, pattern matching

#### 13. **core::slice** (`packages/core/src/slice.tml`)
- Slice manipulation, sorting, searching

#### 14. **core::ptr** (`packages/core/src/ptr.tml`)
- Pointer utilities, `NonNull[T]`

#### 15. **core::error** (`packages/core/src/error.tml`)
- `Error` behavior for error types

### Phase 4: Iterator Enhancements (Week 7)

#### 16. **Expand std::iter**
- Add `map()`, `filter()`, `collect()`
- Add `zip()`, `enumerate()`, `chain()`, `rev()`
- Add `find()`, `position()`, `max()`, `min()`

### Phase 5: Types Enhancements (Week 8)

#### 17. **Expand std::types**
- Add `map()`, `and_then()`, `or_else()`, `filter()` to `Maybe[T]`
- Add `map()`, `map_err()`, `and_then()`, `or_else()` to `Outcome[T,E]`
- Add `unwrap()`, `expect()`, `unwrap_or_else()`

### Design Principles

Based on Rust core library design:
- Behaviors (traits) are minimal and composable
- Default implementations where sensible (e.g., `ne()` from `eq()`)
- Associated types for flexibility (e.g., `type Output` in `Add`)
- Generic bounds enable type-safe abstractions
- Zero-cost abstractions - behaviors compile to static dispatch

## Impact

- **Affected specs**:
  - `docs/05-SEMANTICS.md` (behaviors system)
  - `docs/04-TYPES.md` (operator desugaring)
  - `packages/std/src/types/mod.tml` (Maybe/Outcome enhancements)
  - `packages/std/src/iter/mod.tml` (iterator combinators)

- **Affected code**:
  - New `packages/core/src/` directory (15+ new files)
  - New `packages/core/src/mod.tml` (root module with re-exports)
  - Updates to existing stdlib types to implement behaviors

- **Breaking change**: NO (pure additions, existing code unaffected)

- **User benefit**:
  - 90% of Rust's ergonomics with Phase 1 alone
  - Natural syntax: `a + b`, `list[i]`, `x.clone()`
  - Type-safe generic programming with behavior bounds
  - Better debugging with Display/Debug
  - Collections can be sorted, searched, hashed with custom types

- **Dependencies**:
  - Requires behaviors (traits) system (already implemented)
  - Requires generic types (already implemented)
  - Requires operator desugaring in compiler (NEW - needs implementation)

## Success Criteria

### Phase 1 (Critical Behaviors)
1. `Clone` and `Copy` behaviors work, can clone Lists and custom types
2. `Ord` and `PartialEq` enable sorting and comparison
3. Operator overloading works: `a + b` desugars to `a.add(b)`, `list[i]` to `list.index(i)`
4. `Default` provides zero-values for primitives and collections
5. `Display` and `Debug` enable custom printing
6. Test coverage ≥95% for all Phase 1 modules
7. All primitives (I8-I64, F32, F64, Bool) implement all behaviors
8. `List[T]`, `HashMap[K,V]` implement all applicable behaviors

### Phase 2 (Conversions & Utilities)
1. `From`/`Into` enable ergonomic conversions
2. `Hash` behavior allows custom types in HashMap
3. `Borrow` enables generic borrowing patterns
4. Expanded `core::mem` has `swap()`, `replace()`, `size_of()`

### Phase 3 (Advanced)
1. Interior mutability patterns work with `Cell`/`RefCell`
2. String and slice utilities are complete
3. Error handling has `Error` behavior

### Phases 4-5 (Enhancements)
1. Iterators have full combinator suite (`map`, `filter`, `collect`)
2. `Maybe` and `Outcome` have full monadic API
3. All enhancements have tests and documentation

### Overall
- Test coverage ≥95% across all modules
- Documentation for every public behavior and function
- Performance: zero-cost abstractions (verify with benchmarks)
- Compatibility: Existing code compiles without changes
