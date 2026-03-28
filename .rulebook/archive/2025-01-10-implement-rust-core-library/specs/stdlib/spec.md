# Specification: Rust Core Library Implementation

## Overview

This specification defines the implementation of Rust-equivalent core behaviors and types in TML's standard library. The goal is to provide 90% of Rust's ergonomics through critical behaviors (traits) that enable operator overloading, cloning, comparison, formatting, and conversions.

## Architecture

```
packages/core/
├── src/
│   ├── mod.tml          # Root module with re-exports
│   ├── clone.tml        # Clone, Copy behaviors
│   ├── cmp.tml          # PartialEq, Ord, Ordering
│   ├── ops.tml          # Add, Sub, Mul, Div, Index, Neg, Not
│   ├── default.tml      # Default behavior
│   ├── fmt.tml          # Display, Debug behaviors
│   ├── convert.tml      # From, Into, TryFrom, TryInto
│   ├── hash.tml         # Hash behavior
│   ├── borrow.tml       # Borrow, BorrowMut
│   ├── marker.tml       # Send, Sync, Sized
│   ├── cell.tml         # Cell[T], RefCell[T]
│   ├── str.tml          # String utilities
│   ├── slice.tml        # Slice utilities
│   ├── ptr.tml          # Pointer utilities
│   └── error.tml        # Error behavior
```

## Core Behaviors

### 1. Clone (core::clone)

**Purpose**: Explicit duplication of values

**Definition**:
```tml
pub behavior Clone {
    func clone(this) -> This
}

pub behavior Copy extends Clone {
    // Marker behavior - automatic bitwise copy
}
```

**Implementations**:
- All primitives (I8-I64, U8-U64, F32, F64, Bool) implement Copy
- List[T where T: Clone] implements Clone (deep copy)
- HashMap[K, V where K: Clone, V: Clone] implements Clone
- Maybe[T where T: Clone] implements Clone
- Outcome[T, E where T: Clone, E: Clone] implements Clone

**Example**:
```tml
let list1 = [1, 2, 3]
let list2 = list1.clone()  // Deep copy
```

### 2. Comparison (core::cmp)

**Purpose**: Enable equality and ordering comparisons

**Definition**:
```tml
pub type Ordering {
    Less,
    Equal,
    Greater
}

pub behavior PartialEq {
    func eq(this, other: This) -> Bool

    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

pub behavior Ord extends PartialEq {
    func cmp(this, other: This) -> Ordering

    func lt(this, other: This) -> Bool {
        when this.cmp(other) {
            Less => return true,
            _ => return false
        }
    }

    func le(this, other: This) -> Bool {
        when this.cmp(other) {
            Less => return true,
            Equal => return true,
            Greater => return false
        }
    }

    func gt(this, other: This) -> Bool {
        when this.cmp(other) {
            Greater => return true,
            _ => return false
        }
    }

    func ge(this, other: This) -> Bool {
        when this.cmp(other) {
            Greater => return true,
            Equal => return true,
            Less => return false
        }
    }
}
```

**Implementations**:
- All primitives implement Ord
- List[T where T: Ord] implements Ord (lexicographic)
- Maybe[T where T: Ord] implements Ord (Nothing < Just)
- Ordering implements Ord

**Example**:
```tml
if a.eq(b) { ... }
if a.lt(b) { ... }

let ordering = a.cmp(b)
when ordering {
    Less => println("a < b"),
    Equal => println("a == b"),
    Greater => println("a > b")
}
```

### 3. Operator Overloading (core::ops)

**Purpose**: Enable natural syntax with operators

**Definition**:
```tml
pub behavior Add {
    type Output
    func add(this, rhs: This) -> This::Output
}

pub behavior Sub {
    type Output
    func sub(this, rhs: This) -> This::Output
}

pub behavior Mul {
    type Output
    func mul(this, rhs: This) -> This::Output
}

pub behavior Div {
    type Output
    func div(this, rhs: This) -> This::Output
}

pub behavior Rem {
    type Output
    func rem(this, rhs: This) -> This::Output
}

pub behavior Neg {
    type Output
    func neg(this) -> This::Output
}

pub behavior Not {
    type Output
    func not(this) -> This::Output
}

pub behavior Index {
    type Output
    func index(this, idx: I64) -> This::Output
}
```

**Compiler Desugaring**:
- `a + b` → `a.add(b)`
- `a - b` → `a.sub(b)`
- `a * b` → `a.mul(b)`
- `a / b` → `a.div(b)`
- `a % b` → `a.rem(b)`
- `-a` → `a.neg()`
- `not a` → `a.not()`
- `a[i]` → `a.index(i)`

**Implementations**:
- All numeric primitives implement Add, Sub, Mul, Div, Rem, Neg
- Bool implements Not
- List[T] implements Index (returns T)
- HashMap[K, V] implements Index (returns V)

**Example**:
```tml
// Before (verbose):
let sum = a.add(b)
let item = list.get(i)

// After (with ops):
let sum = a + b
let item = list[i]
```

### 4. Default Values (core::default)

**Purpose**: Provide zero/empty values for types

**Definition**:
```tml
pub behavior Default {
    func default() -> This
}
```

**Implementations**:
- I8-I64, U8-U64 → 0
- F32, F64 → 0.0
- Bool → false
- List[T] → empty list
- HashMap[K, V] → empty map
- Maybe[T] → Nothing
- Str → empty string

**Example**:
```tml
let x: I32 = Default::default()  // 0
let list: List[I32] = Default::default()  // []
```

### 5. Formatting (core::fmt)

**Purpose**: Convert types to strings for display and debugging

**Definition**:
```tml
pub behavior Display {
    func fmt(this) -> Str
}

pub behavior Debug {
    func debug_fmt(this) -> Str
}
```

**Implementations**:
- All primitives implement Display and Debug
- List[T where T: Display] implements Display as "[1, 2, 3]"
- List[T where T: Debug] implements Debug
- Maybe[T where T: Display] implements Display as "Just(x)" or "Nothing"
- Outcome[T, E where T: Display, E: Display] implements Display

**Example**:
```tml
println(list.fmt())  // Uses Display
println(list.debug_fmt())  // Uses Debug
```

### 6. Conversions (core::convert)

**Purpose**: Enable type conversions

**Definition**:
```tml
pub behavior From[T] {
    func from(value: T) -> This
}

pub behavior Into[T] {
    func into(this) -> T
}

pub behavior TryFrom[T] {
    type Error
    func try_from(value: T) -> Outcome[This, This::Error]
}

pub behavior TryInto[T] {
    type Error
    func try_into(this) -> Outcome[T, This::Error]
}

pub behavior AsRef[T] {
    func as_ref(this) -> ref T
}

pub behavior AsMut[T] {
    func as_mut(this) -> mut ref T
}
```

**Example**:
```tml
let x: I64 = I64::from(42i32)
let result: Outcome[I32, ParseError] = I32::try_from("42")
```

### 7. Hashing (core::hash)

**Purpose**: Enable custom types in HashMap

**Definition**:
```tml
pub behavior Hash {
    func hash(this, hasher: mut ref Hasher)
}

pub type Hasher {
    // Hasher state
}

impl Hasher {
    pub func write_i32(this, value: I32)
    pub func write_i64(this, value: I64)
    pub func finish(this) -> I64
}
```

**Implementations**:
- All primitives implement Hash
- List[T where T: Hash] implements Hash
- Maybe[T where T: Hash] implements Hash

### 8. Borrowing (core::borrow)

**Purpose**: Generic borrowing patterns

**Definition**:
```tml
pub behavior Borrow[T] {
    func borrow(this) -> ref T
}

pub behavior BorrowMut[T] {
    func borrow_mut(this) -> mut ref T
}
```

### 9. Markers (core::marker)

**Purpose**: Marker behaviors for compiler guarantees

**Definition**:
```tml
pub behavior Send {
    // Marker: safe to send across threads
}

pub behavior Sync {
    // Marker: safe to share references across threads
}

pub behavior Sized {
    // Marker: type has known size at compile time
}

pub behavior Unpin {
    // Marker: type can be moved after pinning
}
```

**Auto-implementation**: Most types automatically implement these based on their fields.

### 10. Interior Mutability (core::cell)

**Purpose**: Mutate through shared references

**Definition**:
```tml
pub type Cell[T] {
    value: T
}

impl[T] Cell[T] {
    pub func new(value: T) -> Cell[T]
    pub func get(this) -> T
    pub func set(this, value: T)
    pub func replace(this, value: T) -> T
}

pub type RefCell[T] {
    value: T
    borrow_count: I32
}

impl[T] RefCell[T] {
    pub func new(value: T) -> RefCell[T]
    pub func borrow(this) -> Ref[T]
    pub func borrow_mut(this) -> RefMut[T]
}
```

**Runtime checks**: RefCell panics on borrow violations.

## Iterator Enhancements

### New Methods for Iterator Behavior

```tml
pub behavior Iterator {
    type Item
    func next(this) -> Maybe[This::Item]

    // Existing:
    func take(this, n: I64) -> TakeIterator[This]
    func skip(this, n: I64) -> SkipIterator[This]
    func fold[B](this, init: B, func(B, This::Item) -> B) -> B
    func sum(this) -> This::Item
    func count(this) -> I64
    func any(this, func(This::Item) -> Bool) -> Bool
    func all(this, func(This::Item) -> Bool) -> Bool

    // NEW:
    func map[U](this, func(This::Item) -> U) -> MapIterator[This, U]
    func filter(this, func(This::Item) -> Bool) -> FilterIterator[This]
    func collect[C](this) -> C
    func zip[U](this, other: Iterator[U]) -> ZipIterator[This, U]
    func enumerate(this) -> EnumerateIterator[This]
    func chain[U](this, other: Iterator[U]) -> ChainIterator[This, U]
    func rev(this) -> RevIterator[This]
    func find(this, func(This::Item) -> Bool) -> Maybe[This::Item]
    func position(this, func(This::Item) -> Bool) -> Maybe[I64]
    func max(this) -> Maybe[This::Item] where This::Item: Ord
    func min(this) -> Maybe[This::Item] where This::Item: Ord
}
```

**Example**:
```tml
let nums = [1, 2, 3, 4, 5]
let result = nums.iter()
    .filter(do(x) x % 2 == 0)
    .map(do(x) x * 2)
    .collect[List[I32]]()
// result = [4, 8]
```

## Maybe[T] and Outcome[T, E] Enhancements

### Maybe[T] New Methods

```tml
impl[T] Maybe[T] {
    // Existing:
    pub func is_just(this) -> Bool
    pub func is_nothing(this) -> Bool
    pub func unwrap_or(this, default: T) -> T

    // NEW:
    pub func map[U](this, func(T) -> U) -> Maybe[U]
    pub func and_then[U](this, func(T) -> Maybe[U]) -> Maybe[U]
    pub func or_else(this, func() -> Maybe[T]) -> Maybe[T]
    pub func filter(this, func(T) -> Bool) -> Maybe[T]
    pub func unwrap(this) -> T  // Panics on Nothing
    pub func expect(this, msg: Str) -> T  // Panics with message
    pub func unwrap_or_else(this, func() -> T) -> T
    pub func ok_or[E](this, err: E) -> Outcome[T, E]
    pub func ok_or_else[E](this, func() -> E) -> Outcome[T, E]
}
```

### Outcome[T, E] New Methods

```tml
impl[T, E] Outcome[T, E] {
    // Existing:
    pub func is_ok(this) -> Bool
    pub func is_err(this) -> Bool
    pub func unwrap_or(this, default: T) -> T

    // NEW:
    pub func map[U](this, func(T) -> U) -> Outcome[U, E]
    pub func map_err[F](this, func(E) -> F) -> Outcome[T, F]
    pub func and_then[U](this, func(T) -> Outcome[U, E]) -> Outcome[U, E]
    pub func or_else[F](this, func(E) -> Outcome[T, F]) -> Outcome[T, F]
    pub func unwrap(this) -> T  // Panics on Err
    pub func unwrap_err(this) -> E  // Panics on Ok
    pub func expect(this, msg: Str) -> T
    pub func unwrap_or_else(this, func(E) -> T) -> T
}
```

## Compiler Changes Required

### 1. Operator Desugaring

**Before type checking**, the parser/AST must desugar operators to method calls:

| Syntax | Desugars To | Requires Behavior |
|--------|-------------|-------------------|
| `a + b` | `a.add(b)` | Add |
| `a - b` | `a.sub(b)` | Sub |
| `a * b` | `a.mul(b)` | Mul |
| `a / b` | `a.div(b)` | Div |
| `a % b` | `a.rem(b)` | Rem |
| `-a` | `a.neg()` | Neg |
| `not a` | `a.not()` | Not |
| `a[i]` | `a.index(i)` | Index |

**Implementation location**: `packages/compiler/src/parser/parser_expr.cpp`

**Logic**:
1. Parse binary/unary operators as usual
2. Create AST node for operator
3. During AST lowering, transform to method call AST node
4. Type checker verifies behavior implementation

### 2. Associated Types

Behaviors can have associated types (e.g., `type Output` in `Add`).

**Implementation**: Already supported via behavior type members in type checker.

**Verification**: Ensure `This::Output` syntax works in return types.

### 3. Behavior Bounds

Generic functions/types can constrain type parameters with behaviors:

```tml
func sort[T: Ord](list: mut ref List[T]) {
    // T must implement Ord
}
```

**Implementation**: Already supported via behavior bounds in type checker.

## Testing Strategy

### Unit Tests

Each module has a test file:
- `packages/core/tests/clone.test.tml`
- `packages/core/tests/cmp.test.tml`
- `packages/core/tests/ops.test.tml`
- etc.

**Coverage target**: ≥95% for each module.

### Integration Tests

Test interactions between behaviors:
- Clone + Ord: Cloning ordered collections
- Display + Iterator: Formatting iterators
- Index + Clone: Cloning indexed values

### Performance Tests

Verify zero-cost abstractions:
- Compare assembly of `a + b` (with Add behavior) vs manual addition
- Verify no overhead for operator desugaring
- Benchmark iterator combinators vs manual loops

### Regression Tests

Ensure existing stdlib code continues to work:
- All existing tests in `packages/std/tests/` pass
- No performance regressions

## Migration Path

### Phase 1 (Weeks 1-2)
Focus on critical behaviors that unlock 90% of ergonomics. Update primitives and core types first.

### Phase 2 (Weeks 3-4)
Add conversion and utility behaviors. Expand core::mem.

### Phase 3 (Weeks 5-6)
Advanced features like interior mutability, string/slice utilities.

### Phases 4-5 (Weeks 7-8)
Enhancements to iterators and Maybe/Outcome for full monadic API.

### Backwards Compatibility

**Breaking changes**: NONE

All additions are pure additions. Existing code compiles without modifications.

**Optional migration**: Code can adopt new behaviors incrementally (e.g., use `list[i]` instead of `list.get(i)` when ready).

## Success Metrics

1. **Functionality**: All behaviors work as specified
2. **Coverage**: ≥95% test coverage across all modules
3. **Performance**: Zero-cost abstractions verified via assembly inspection
4. **Ergonomics**: Natural syntax matches Rust idioms
5. **Compatibility**: No regressions in existing code
6. **Documentation**: Every public item has doc comments and examples

## Dependencies

- **Behaviors system**: Already implemented
- **Generic types**: Already implemented
- **Operator desugaring**: NEW - requires compiler changes
- **Associated types**: Already implemented
- **Behavior bounds**: Already implemented
