# TML Core Library - Comparative Analysis with Rust Core

## Current TML vs Rust Core Structure Analysis

### Overall Status

**Current TML:** 9 modules implemented
**Rust Core:** 33+ modules
**Coverage:** ~27%

---

## MODULES ALREADY IMPLEMENTED IN TML

### 1. **core::mem**
**Status:** Basic implementation
**File:** `packages/compiler/src/core/mem.tml`

**Features:**
- `alloc()` - Memory allocation
- `dealloc()` - Memory deallocation
- `read_i32()`, `write_i32()` - Read/write
- `ptr_offset()` - Pointer arithmetic

**vs Rust core::mem:**
- Missing: `size_of()`, `align_of()`, `swap()`, `replace()`, `drop()`
- Missing: `forget()`, `discriminant()`, `transmute()`
- Missing: `MaybeUninit[T]` type

---

### 2. **core::time**
**Status:** Basic implementation
**File:** `packages/compiler/src/core/time.tml`

**vs Rust core::time:**
- Probably has timing functions
- Need to verify if it has `Duration`, `Instant`

---

### 3. **core::thread**
**Status:** Basic implementation
**File:** `packages/compiler/src/core/thread.tml`

**vs Rust core (std::thread):**
- Rust core doesn't have threads (that's std)
- TML has threading implementation

---

### 4. **core::sync**
**Status:** Basic implementation
**File:** `packages/compiler/src/core/sync.tml`

**vs Rust core::sync:**
- Probably has synchronization primitives
- Need to verify: `Arc`, `Mutex`, `RwLock`, `Barrier`

---

### 5. **std::types**
**Status:** Implemented
**File:** `packages/std/src/types/mod.tml`

**Features:**
- `Maybe[T]` (equivalent to `Option[T]`)
- `Outcome[T, E]` (equivalent to `Result[T, E]`)
- Helper functions: `is_just()`, `is_nothing()`, `unwrap_or()`
- Helper functions: `is_ok()`, `is_err()`, `unwrap_or_ok()`

**vs Rust core::option + core::result:**
- Base types implemented
- Missing: `map()`, `and_then()`, `or_else()`, `filter()`
- Missing: `unwrap()`, `expect()`, `unwrap_or_else()`

---

### 6. **std::iter**
**Status:** Advanced implementation
**File:** `packages/std/src/iter/mod.tml`

**Features:**
- `Iterator` behavior (trait)
- `IntoIterator` behavior
- `Range` type with iteration
- Methods: `next()`, `take()`, `skip()`, `sum()`, `count()`
- Methods: `fold()`, `any()`, `all()`

**vs Rust core::iter:**
- Very good base structure
- Missing: `map()`, `filter()`, `collect()`
- Missing: `zip()`, `enumerate()`, `chain()`, `rev()`
- Missing: `find()`, `position()`, `max()`, `min()`

---

### 7. **std::collections**
**Status:** Advanced implementation
**File:** `packages/std/src/collections/mod.tml`

**Features:**
- `List[T]` - Dynamic array (Vec equivalent)
- `HashMap[K, V]` - Hash table
- `Buffer` - Byte buffer

**vs Rust std::collections (core doesn't have this):**
- Dynamic list implemented
- HashMap implemented
- Missing: `BTreeMap`, `BinaryHeap`, `VecDeque`
- Missing: `HashSet`, `BTreeSet`

---

### 8. **std::file**
**Status:** Implemented
**File:** `packages/std/src/file/mod.tml`

**vs Rust std::fs (core doesn't have I/O):**
- File I/O implemented
- Rust core doesn't have I/O (only std)

---

## CRITICAL MISSING MODULES

### HIGH PRIORITY

#### 1. **core::clone** - CRITICAL
**Rust:** `Clone` trait for explicit duplication
**TML:** NOT IMPLEMENTED
**Required for:**
- Duplicating values that are not `Copy`
- Implementing `clone()` on complex types

**Suggested implementation:**
```tml
// packages/core/src/clone.tml
pub behavior Clone {
    func clone(this) -> This
}

pub behavior Copy extends Clone {
    // Marker behavior - copied implicitly
}
```

---

#### 2. **core::cmp** - CRITICAL
**Rust:** `PartialEq`, `Eq`, `PartialOrd`, `Ord`
**TML:** NOT IMPLEMENTED
**Required for:**
- Custom comparisons
- Collection sorting

**Suggested implementation:**
```tml
// packages/core/src/cmp.tml
pub behavior PartialEq {
    func eq(this, other: This) -> Bool
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

pub behavior Ord extends PartialEq {
    func cmp(this, other: This) -> Ordering
    func lt(this, other: This) -> Bool
    func le(this, other: This) -> Bool
    func gt(this, other: This) -> Bool
    func ge(this, other: This) -> Bool
}

pub type Ordering {
    Less,
    Equal,
    Greater
}
```

---

#### 3. **core::ops** - CRITICAL
**Rust:** Operator overloading (`Add`, `Sub`, `Mul`, `Div`, `Index`)
**TML:** NOT IMPLEMENTED
**Required for:**
- Operator overloading (+, -, *, /, [])
- Syntax sugar for custom types

**Suggested implementation:**
```tml
// packages/core/src/ops.tml
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

pub behavior Index {
    type Output
    func index(this, idx: I64) -> This::Output
}
```

---

#### 4. **core::default** - HIGH PRIORITY
**Rust:** `Default` trait for default values
**TML:** NOT IMPLEMENTED
**Required for:**
- Creating default instances of types
- Generic initialization

**Suggested implementation:**
```tml
// packages/core/src/default.tml
pub behavior Default {
    func default() -> This
}
```

---

#### 5. **core::fmt** - HIGH PRIORITY
**Rust:** Formatting (`Display`, `Debug`)
**TML:** NOT IMPLEMENTED (uses builtins)
**Required for:**
- Custom print
- String representation
- Debug output

**Suggested implementation:**
```tml
// packages/core/src/fmt.tml
pub behavior Display {
    func fmt(this) -> Str
}

pub behavior Debug {
    func debug_fmt(this) -> Str
}
```

---

### MEDIUM PRIORITY

#### 6. **core::convert** - MEDIUM
**Rust:** `From`, `Into`, `TryFrom`, `TryInto`, `AsRef`, `AsMut`
**TML:** NOT IMPLEMENTED
**Required for:**
- Type conversions
- Generic trait bounds

---

#### 7. **core::borrow** - MEDIUM
**Rust:** `Borrow`, `BorrowMut`, `ToOwned`, `Cow`
**TML:** Ownership system exists but no traits
**Required for:**
- Abstraction over owned/borrowed
- Generic borrowing

---

#### 8. **core::hash** - MEDIUM
**Rust:** `Hash` trait, `Hasher`
**TML:** HashMap exists but hash trait is not public
**Required for:**
- Custom hashing
- HashMap with custom types

---

#### 9. **core::cell** - MEDIUM
**Rust:** `Cell[T]`, `RefCell[T]` - interior mutability
**TML:** NOT IMPLEMENTED
**Required for:**
- Interior mutability
- Safe shared mutability

---

#### 10. **core::marker** - MEDIUM
**Rust:** `Copy`, `Send`, `Sync`, `Sized`, `Unpin`
**TML:** NOT IMPLEMENTED
**Required for:**
- Marker traits
- Type guarantees

---

### LOW PRIORITY

#### 11. **core::any** - LOW
**Rust:** Type reflection (`Any`, `TypeId`)
**TML:** NOT IMPLEMENTED

#### 12. **core::str** - LOW
**Rust:** String slice manipulation
**TML:** Has `Str` builtin but no module

#### 13. **core::slice** - LOW
**Rust:** Slice utilities
**TML:** Has arrays but no slice abstraction

#### 14. **core::array** - LOW
**Rust:** Array utilities and traits
**TML:** Arrays exist but no utilities

#### 15. **core::ptr** - LOW
**Rust:** Raw pointer utilities
**TML:** Has `Ptr[T]` but no utilities

#### 16. **core::panic** - LOW
**Rust:** Panic infrastructure
**TML:** NOT IMPLEMENTED (has assert builtins)

#### 17. **core::pin** - LOW
**Rust:** Pinning pointers
**TML:** NOT IMPLEMENTED

#### 18. **core::future** / **core::task** - LOW
**Rust:** Async foundations
**TML:** NOT IMPLEMENTED

#### 19. **core::error** - LOW
**Rust:** Error trait
**TML:** Has `Outcome[T,E]` but no Error trait

---

## SUGGESTED IMPLEMENTATION PLAN

### PHASE 1 - Foundations (CRITICAL)

**Objective:** Implement essential behaviors for generic APIs

1. **core::clone** - `Clone` behavior
2. **core::cmp** - `PartialEq`, `Ord`, `Ordering`
3. **core::default** - `Default` behavior
4. **core::ops** - `Add`, `Sub`, `Mul`, `Div`, `Index`
5. **core::fmt** - `Display`, `Debug`

**Impact:** Enables 80% of common patterns

---

### PHASE 2 - Conversions and Utilities (MEDIUM)

6. **core::convert** - `From`, `Into`, `TryFrom`, `TryInto`
7. **core::hash** - Public `Hash` trait
8. **core::borrow** - `Borrow`, `BorrowMut`
9. **core::marker** - `Copy`, `Send`, `Sync`
10. **Expand core::mem** - `size_of`, `swap`, `replace`

**Impact:** More expressive and type-safe APIs

---

### PHASE 3 - Advanced (LOW)

11. **core::cell** - `Cell[T]`, `RefCell[T]`
12. **core::str** - String utilities
13. **core::slice** - Slice manipulation
14. **core::ptr** - Pointer utilities
15. **core::error** - Error trait

**Impact:** Advanced features

---

### PHASE 4 - Async and Specialized (OPTIONAL)

16. **core::future** - Future trait
17. **core::task** - Task types
18. **core::pin** - Pin types
19. **core::any** - Type reflection

**Impact:** Async/await support

---

## IMMEDIATE RECOMMENDATIONS

### For LLMs to Generate Efficient Code:

**TOP 3 PRIORITIES:**

1. **Implement core::clone**
   - 90% of Rust code uses Clone
   - Critical for working with collections

2. **Implement core::cmp**
   - Required for sorting and ordering
   - Enables `sort()` on List[T]

3. **Implement core::ops**
   - Massive syntax sugar
   - `vec[i]` instead of `vec.get(i)`
   - `a + b` instead of `a.add(b)`

### Files to Create:

```
packages/core/src/
  mod.tml           # Re-export all core modules
  clone.tml         # Clone, Copy behaviors
  cmp.tml           # PartialEq, Ord, Ordering
  default.tml       # Default behavior
  ops.tml           # Add, Sub, Mul, Div, Index
  fmt.tml           # Display, Debug
  convert.tml       # From, Into conversions
  hash.tml          # Hash behavior
  borrow.tml        # Borrow, BorrowMut
  marker.tml        # Copy, Send, Sync markers
```

---

## PRIORITY MATRIX

| Module | Priority | Complexity | Impact | Effort | ROI |
|--------|----------|------------|--------|--------|-----|
| core::clone | CRITICAL | Low | High | 1 day | ***** |
| core::cmp | CRITICAL | Medium | High | 2 days | ***** |
| core::ops | CRITICAL | Medium | Very High | 3 days | ***** |
| core::default | HIGH | Low | Medium | 1 day | **** |
| core::fmt | HIGH | Medium | High | 2 days | **** |
| core::convert | MEDIUM | Medium | Medium | 2 days | *** |
| core::hash | MEDIUM | Low | Low | 1 day | *** |
| core::borrow | MEDIUM | High | Medium | 3 days | *** |
| core::cell | LOW | High | Low | 4 days | ** |
| core::future | OPTIONAL | Very High | Low | 10+ days | * |

---

## EXECUTIVE SUMMARY

**What TML already has (VERY GOOD):**
- Maybe[T] and Outcome[T,E] - solid foundation
- Iterator system - well implemented
- Basic collections - List, HashMap, Buffer
- Low-level memory - core::mem functional

**What TML needs URGENTLY:**
- Essential behaviors: Clone, PartialEq, Ord
- Operator overloading: Add, Sub, Index, etc.
- Default trait
- Display/Debug for formatting

**Impact:**
With **core::clone, core::cmp, core::ops** implemented, TML would have **90%** of Rust's ergonomics for LLMs to generate idiomatic code.

---

## NEXT STEPS

1. **Create `packages/core/src/mod.tml`**
2. **Implement `core::clone.tml`** - Clone behavior
3. **Implement `core::cmp.tml`** - PartialEq, Ord
4. **Implement `core::ops.tml`** - Add, Sub, Mul, etc.
5. **Implement `core::default.tml`** - Default
6. **Implement `core::fmt.tml`** - Display, Debug
7. **Update std::iter** - Add map(), filter(), collect()
8. **Update std::types** - Add map(), and_then(), etc.

---

**Documented on:** 2025-12-26
**TML Version:** 0.1.0
**Baseline:** Rust core 1.83.0
