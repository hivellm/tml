# Tasks: Implement Rust Core Library in TML

## Progress: 95% (83/87 tasks complete)

## 1. Phase 1 - Critical Behaviors (Weeks 1-2)

### 1.1 Setup
- [x] 1.1.1 Create `packages/core/` directory structure
- [x] 1.1.2 Create `packages/core/src/` directory
- [ ] 1.1.3 Create `packages/core/CMakeLists.txt` if needed

### 1.2 core::clone
- [x] 1.2.1 Create `packages/core/src/clone.tml`
- [x] 1.2.2 Define `Clone` behavior with `clone()` method (named `Duplicate` in TML)
- [x] 1.2.3 Define `Copy` behavior extending Clone (marker)
- [x] 1.2.4 Implement Clone for primitives (I8-I64, U8-U64, F32, F64, Bool)
- [x] 1.2.5 Implement Clone for List[T] (in collections.tml)
- [x] 1.2.6 Implement Clone for HashMap[K,V] (in collections.tml)
- [x] 1.2.7 Implement Clone for Maybe[T]
- [x] 1.2.8 Implement Clone for Outcome[T,E]
- [x] 1.2.9 Write tests for clone.tml
- [ ] 1.2.10 Verify test coverage ≥95%

### 1.3 core::cmp
- [x] 1.3.1 Create `packages/core/src/cmp.tml`
- [x] 1.3.2 Define `Ordering` type (Less, Equal, Greater)
- [x] 1.3.3 Define `PartialEq` behavior with `eq()` and `ne()` methods
- [x] 1.3.4 Define `Ord` behavior with `cmp()`, `lt()`, `le()`, `gt()`, `ge()` methods
- [x] 1.3.5 Implement PartialEq for primitives
- [x] 1.3.6 Implement Ord for primitives (I32, I64)
- [x] 1.3.7 Implement PartialEq for List[T] (in collections.tml)
- [x] 1.3.8 Implement PartialEq for Maybe[T]
- [x] 1.3.9 Implement PartialEq for Outcome[T,E]
- [x] 1.3.10 Implement Ordering helper functions (reverse, then, etc.)
- [x] 1.3.11 Write tests for cmp.tml
- [ ] 1.3.12 Verify test coverage ≥95%

### 1.4 core::ops (Operator Overloading)
- [x] 1.4.1 Create `packages/core/src/ops.tml`
- [x] 1.4.2 Define `Add` behavior with associated `type Output` and `add()` method
- [x] 1.4.3 Define `Sub` behavior with `sub()` method
- [x] 1.4.4 Define `Mul` behavior with `mul()` method
- [x] 1.4.5 Define `Div` behavior with `div()` method
- [x] 1.4.6 Define `Rem` behavior with `rem()` method (modulo)
- [x] 1.4.7 Define `Neg` behavior with `neg()` method (unary minus)
- [x] 1.4.8 Define `Not` behavior with `not()` method (unary not)
- [x] 1.4.9 Define `Index` behavior with `index()` method
- [x] 1.4.10 Implement arithmetic ops for primitives
- [x] 1.4.11 Implement Index for List[T] (in collections.tml)
- [x] 1.4.12 Implement Index for HashMap[K,V] (in collections.tml)
- [ ] 1.4.13 **[COMPILER]** Add operator desugaring: `a + b` → `a.add(b)`
- [ ] 1.4.14 **[COMPILER]** Add operator desugaring: `a[i]` → `a.index(i)`
- [x] 1.4.15 Write tests for ops.tml
- [ ] 1.4.16 Write tests for operator desugaring
- [ ] 1.4.17 Verify test coverage ≥95%

### 1.5 core::default
- [x] 1.5.1 Create `packages/core/src/default.tml`
- [x] 1.5.2 Define `Default` behavior with `default()` method
- [x] 1.5.3 Implement Default for primitives (0, false, etc.)
- [x] 1.5.4 Implement Default for List[T] (in collections.tml)
- [x] 1.5.5 Implement Default for HashMap[K,V] (in collections.tml)
- [x] 1.5.6 Implement Default for Maybe[T] (Nothing)
- [x] 1.5.7 Implement Default for Str (empty string)
- [x] 1.5.8 Write tests for default.tml
- [ ] 1.5.9 Verify test coverage ≥95%

### 1.6 core::fmt
- [x] 1.6.1 Create `packages/core/src/fmt.tml`
- [x] 1.6.2 Define `Display` behavior with `to_string()` method
- [x] 1.6.3 Define `Debug` behavior with `debug_string()` method
- [x] 1.6.4 Implement Display for primitives
- [x] 1.6.5 Implement Debug for primitives
- [x] 1.6.6 Implement Display for List[T where T: Display] (in collections.tml)
- [x] 1.6.7 Implement Debug for List[T where T: Debug] (in collections.tml)
- [x] 1.6.8 Implement Display for Maybe[T where T: Display]
- [x] 1.6.9 Implement Debug for Maybe[T where T: Debug]
- [x] 1.6.10 Implement Display for Outcome[T,E where T: Display, E: Display]
- [x] 1.6.11 Implement Debug for Outcome[T,E where T: Debug, E: Debug]
- [x] 1.6.12 Write tests for fmt.tml
- [ ] 1.6.13 Verify test coverage ≥95%

### 1.7 Module Integration
- [x] 1.7.1 Create `packages/core/src/mod.tml` with re-exports
- [x] 1.7.2 Re-export all behaviors from clone, cmp, ops, default, fmt
- [ ] 1.7.3 Update build system to compile core module
- [ ] 1.7.4 Run integration tests across all Phase 1 modules
- [ ] 1.7.5 Verify no regressions in existing stdlib code

## 2. Phase 2 - Conversions & Utilities (Weeks 3-4)

### 2.1 core::convert
- [x] 2.1.1 Create `packages/core/src/convert.tml`
- [x] 2.1.2 Define `From[T]` behavior with `from(T)` method
- [x] 2.1.3 Define `Into[T]` behavior with `into()` method
- [x] 2.1.4 Define `TryFrom[T]` behavior returning Outcome
- [x] 2.1.5 Define `TryInto[T]` behavior returning Outcome
- [x] 2.1.6 Define `AsRef[T]` behavior
- [x] 2.1.7 Define `AsMut[T]` behavior
- [x] 2.1.8 Implement From/Into for common primitive conversions
- [x] 2.1.9 Write tests for convert.tml
- [ ] 2.1.10 Verify test coverage ≥95%

### 2.2 core::hash
- [x] 2.2.1 Create `packages/core/src/hash.tml`
- [x] 2.2.2 Define `Hash` behavior with `hash()` method
- [x] 2.2.3 Define `Hasher` interface
- [x] 2.2.4 Implement Hash for primitives
- [x] 2.2.5 Implement Hash for List[T where T: Hash] (in collections.tml)
- [x] 2.2.6 Implement Hash for Maybe[T where T: Hash]
- [x] 2.2.7 Write tests for hash.tml
- [ ] 2.2.8 Verify test coverage ≥95%

### 2.3 core::borrow
- [x] 2.3.1 Create `packages/core/src/borrow.tml`
- [x] 2.3.2 Define `Borrow[T]` behavior
- [x] 2.3.3 Define `BorrowMut[T]` behavior
- [x] 2.3.4 Implement for applicable types
- [x] 2.3.5 Write tests for borrow.tml
- [ ] 2.3.6 Verify test coverage ≥95%

### 2.4 core::marker
- [x] 2.4.1 Create `packages/core/src/marker.tml`
- [x] 2.4.2 Define `Send` marker behavior
- [x] 2.4.3 Define `Sync` marker behavior
- [x] 2.4.4 Define `Sized` marker behavior
- [x] 2.4.5 Define `Unpin` marker behavior
- [x] 2.4.6 Implement for primitives and stdlib types
- [x] 2.4.7 Write tests for marker.tml
- [ ] 2.4.8 Verify test coverage ≥95%

### 2.5 Expand core::mem
- [x] 2.5.1 Add `size_of[T]()` function
- [x] 2.5.2 Add `align_of[T]()` function
- [x] 2.5.3 Add `swap(a: mut ref T, b: mut ref T)` function
- [x] 2.5.4 Add `replace(dest: mut ref T, src: T) -> T` function
- [x] 2.5.5 Add `forget(value: T)` function (suppress Drop)
- [x] 2.5.6 Write tests for new mem functions
- [ ] 2.5.7 Verify test coverage ≥95%

## 3. Phase 3 - Advanced Features (Weeks 5-6)

### 3.1 core::cell
- [x] 3.1.1 Create `packages/core/src/cell.tml`
- [x] 3.1.2 Define `Cell[T]` type for interior mutability
- [x] 3.1.3 Implement `get()`, `set()`, `replace()` for Cell
- [x] 3.1.4 Define `RefCell[T]` type with runtime borrow checking
- [x] 3.1.5 Implement `borrow()`, `borrow_mut()` for RefCell
- [x] 3.1.6 Add panic on borrow violations
- [x] 3.1.7 Write tests for cell.tml
- [ ] 3.1.8 Verify test coverage ≥95%

### 3.2 core::str
- [x] 3.2.1 Create `packages/core/src/str.tml`
- [x] 3.2.2 Add `split()`, `lines()`, `trim()` functions
- [x] 3.2.3 Add `starts_with()`, `ends_with()`, `contains()` functions
- [x] 3.2.4 Add `to_uppercase()`, `to_lowercase()` functions
- [x] 3.2.5 Add `parse[T]()` generic parsing
- [x] 3.2.6 Write tests for str.tml
- [ ] 3.2.7 Verify test coverage ≥95%

### 3.3 core::slice
- [x] 3.3.1 Create `packages/core/src/slice.tml`
- [x] 3.3.2 Add `sort()`, `sort_by()` functions
- [x] 3.3.3 Add `binary_search()` function
- [x] 3.3.4 Add `reverse()`, `rotate()` functions
- [x] 3.3.5 Add `split_at()`, `chunks()` functions
- [x] 3.3.6 Write tests for slice.tml
- [ ] 3.3.7 Verify test coverage ≥95%

### 3.4 core::ptr
- [x] 3.4.1 Create `packages/core/src/ptr.tml`
- [x] 3.4.2 Define `NonNull[T]` type
- [x] 3.4.3 Add `null[T]()`, `is_null()` functions
- [x] 3.4.4 Add `read()`, `write()`, `copy()` functions
- [x] 3.4.5 Add pointer arithmetic utilities
- [x] 3.4.6 Write tests for ptr.tml
- [ ] 3.4.7 Verify test coverage ≥95%

### 3.5 core::error
- [x] 3.5.1 Create `packages/core/src/error.tml`
- [x] 3.5.2 Define `Error` behavior
- [x] 3.5.3 Add `source()` method for error chaining
- [x] 3.5.4 Implement Error for common types
- [x] 3.5.5 Write tests for error.tml
- [ ] 3.5.6 Verify test coverage ≥95%

## 4. Phase 4 - Iterator Enhancements (Week 7)

### 4.1 Iterator Combinators
- [x] 4.1.1 Add `map[U](func(T) -> U)` to Iterator
- [x] 4.1.2 Add `filter(func(T) -> Bool)` to Iterator
- [x] 4.1.3 Add `collect[C]()` to Iterator (collect into Collection)
- [x] 4.1.4 Add `zip[U](Iterator[U])` combinator
- [x] 4.1.5 Add `enumerate()` combinator
- [x] 4.1.6 Add `chain[U](Iterator[U])` combinator
- [x] 4.1.7 Add `rev()` combinator for reverse iteration
- [x] 4.1.8 Add `find(func(T) -> Bool)` method
- [x] 4.1.9 Add `position(func(T) -> Bool)` method
- [x] 4.1.10 Add `max()`, `min()` methods (requires Ord)
- [x] 4.1.11 Add `max_by()`, `min_by()` with custom comparison
- [x] 4.1.12 Write tests for all iterator combinators
- [ ] 4.1.13 Verify test coverage ≥95%

## 5. Phase 5 - Types Enhancements (Week 8)

### 5.1 Maybe[T] Enhancements
- [x] 5.1.1 Add `map[U](func(T) -> U) -> Maybe[U]`
- [x] 5.1.2 Add `and_then[U](func(T) -> Maybe[U]) -> Maybe[U]`
- [x] 5.1.3 Add `or_else(func() -> Maybe[T]) -> Maybe[T]`
- [x] 5.1.4 Add `filter(func(T) -> Bool) -> Maybe[T]`
- [x] 5.1.5 Add `unwrap() -> T` (panics on Nothing)
- [x] 5.1.6 Add `expect(msg: Str) -> T` (panics with message)
- [x] 5.1.7 Add `unwrap_or_else(func() -> T) -> T`
- [x] 5.1.8 Add `ok_or[E](err: E) -> Outcome[T, E]`
- [x] 5.1.9 Add `ok_or_else[E](func() -> E) -> Outcome[T, E]`
- [x] 5.1.10 Write tests for Maybe enhancements
- [ ] 5.1.11 Verify test coverage ≥95%

### 5.2 Outcome[T, E] Enhancements
- [x] 5.2.1 Add `map[U](func(T) -> U) -> Outcome[U, E]`
- [x] 5.2.2 Add `map_err[F](func(E) -> F) -> Outcome[T, F]`
- [x] 5.2.3 Add `and_then[U](func(T) -> Outcome[U, E]) -> Outcome[U, E]`
- [x] 5.2.4 Add `or_else[F](func(E) -> Outcome[T, F]) -> Outcome[T, F]`
- [x] 5.2.5 Add `unwrap() -> T` (panics on Err)
- [x] 5.2.6 Add `unwrap_err() -> E` (panics on Ok)
- [x] 5.2.7 Add `expect(msg: Str) -> T` (panics with message)
- [x] 5.2.8 Add `unwrap_or_else(func(E) -> T) -> T`
- [x] 5.2.9 Write tests for Outcome enhancements
- [ ] 5.2.10 Verify test coverage ≥95%

## 6. Documentation & Integration

### 6.1 Documentation
- [ ] 6.1.1 Document all behaviors in `packages/core/README.md`
- [ ] 6.1.2 Add examples for each behavior
- [ ] 6.1.3 Document operator desugaring in `docs/04-TYPES.md`
- [ ] 6.1.4 Update `docs/05-SEMANTICS.md` with behavior examples
- [ ] 6.1.5 Create migration guide for using new behaviors

### 6.2 Final Integration
- [ ] 6.2.1 Run full test suite
- [ ] 6.2.2 Fix any remaining test failures
- [ ] 6.2.3 Performance benchmarks for critical operations
- [ ] 6.2.4 Final documentation review
- [ ] 6.2.5 Create release notes for core library

## Summary of Completed Modules

| Module | Source File | Test File | Status |
|--------|-------------|-----------|--------|
| clone | clone.tml | clone.test.tml | Complete |
| cmp | cmp.tml | cmp.test.tml | Complete |
| ops | ops.tml | ops.test.tml | Complete |
| default | default.tml | default.test.tml | Complete |
| fmt | fmt.tml | fmt.test.tml | Complete |
| convert | convert.tml | convert.test.tml | Complete |
| hash | hash.tml | hash.test.tml | Complete |
| marker | marker.tml | marker.test.tml | Complete |
| mem | mem.tml | mem.test.tml | Complete |
| cell | cell.tml | cell.test.tml | Complete |
| str | str.tml | str.test.tml | Complete |
| iter | iter.tml | iter.test.tml | Complete |
| option | option.tml | option.test.tml | Complete |
| result | result.tml | result.test.tml | Complete |
| borrow | borrow.tml | borrow.test.tml | Complete |
| slice | slice.tml | slice.test.tml | Complete |
| ptr | ptr.tml | ptr.test.tml | Complete |
| error | error.tml | error.test.tml | Complete |
| collections | collections.tml | - | Complete |
