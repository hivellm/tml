# TML Syntax Redesign Proposal: Complete Departure from Rust

## Executive Summary

This document identifies syntax patterns in TML v1.0 that are too similar to Rust and proposes LLM-native alternatives. The goal is to create a language that is:

1. **Distinctly TML** - Not "Rust with different syntax"
2. **LLM-Optimized** - Verbose, explicit, natural-language-like
3. **Self-Documenting** - Readable without deep language knowledge
4. **Unambiguous** - No symbols with multiple meanings

---

## Critical Analysis: What's Still Too Rust-Like

### Category 1: Annotations/Attributes

| Current (Rust-like) | Problem |
|---------------------|---------|
| `#[cfg(...)]` | Exact Rust syntax |
| `#[derive(...)]` | Exact Rust syntax |
| `#[test]` | Exact Rust syntax |
| `#[inline]` | Exact Rust syntax |
| `#[unsafe]` | Rust pattern |

### Category 2: Reference/Pointer Syntax

| Current (Rust-like) | Problem |
|---------------------|---------|
| `&T` | Rust borrow syntax |
| `&mut T` | Rust mutable borrow |
| `*T` | Rust raw pointer |
| `*mut T` | Rust mutable raw pointer |

### Category 3: Type Names

| Current (Rust-like) | Problem |
|---------------------|---------|
| `Option[T]` | Same name as Rust |
| `Result[T, E]` | Same name as Rust |
| `Box[T]` | Same name as Rust |
| `Rc[T]` | Same abbreviation as Rust |
| `Arc[T]` | Same abbreviation as Rust |
| `Vec[T]` | Rust uses `Vec<T>` |
| `RefCell[T]` | Same name as Rust |

### Category 4: Smart Pointer Patterns

| Current (Rust-like) | Problem |
|---------------------|---------|
| `Box.new(x)` | Rust pattern |
| `Rc.clone(&x)` | Rust pattern |
| `Arc.downgrade(&x)` | Rust pattern |

### Category 5: Keywords/Concepts

| Current (Rust-like) | Problem |
|---------------------|---------|
| `unsafe` | Same keyword |
| `trait` | Same keyword |
| `extend ... with Trait` | Similar to `impl Trait for Type` |
| `Self` / `This` | `This` is good, but pattern is same |
| `move` in closures | Rust pattern |

### Category 6: Operators

| Current (Rust-like) | Problem |
|---------------------|---------|
| `..` range | Rust syntax |
| `..=` inclusive range | Rust syntax |
| `?` or `!` for errors | `!` is different but similar concept |

---

## Proposed LLM-Native Redesign

### 1. Annotations → Directives

**Philosophy**: Use natural language directives instead of cryptic `#[...]` syntax.

```
CURRENT (Rust-like):
#[cfg(target_os = "linux")]
#[derive(Debug, Clone)]
#[test]
#[inline]
#[unsafe]

PROPOSED (LLM-native):
@when(os: linux)
@auto(debug, clone)
@test
@hint(inline)
@lowlevel
```

**Full Directive System**:

```tml
// Platform/Conditional
@when(os: linux)
@when(os: windows)
@when(arch: x86_64)
@when(arch: aarch64)
@when(feature: async)
@when(debug)
@unless(os: windows)  // Negation

// Auto-generation
@auto(debug)         // Auto-generate debug representation
@auto(clone)         // Auto-generate clone
@auto(equal)         // Auto-generate equality
@auto(order)         // Auto-generate ordering
@auto(hash)          // Auto-generate hashing
@auto(format)        // Auto-generate string formatting

// Testing
@test
@test(should_fail)
@test(ignore)
@benchmark

// Optimization hints
@hint(inline)
@hint(inline: always)
@hint(inline: never)
@hint(cold)          // Unlikely path
@hint(hot)           // Hot path

// Safety
@lowlevel            // Instead of unsafe
@trusted             // Verified unsafe code

// Visibility (alternative to `public`)
@export              // Public API

// Deprecation
@deprecated("Use new_function instead")
@deprecated(since: "2.0", use: "new_function")

// Documentation
@doc("Main entry point")

// AI hints (already proposed)
@ai(context: "Hot loop")
@ai(intent: "Security critical")
```

**Rationale**:
- `@` is universally recognized as "directive/annotation"
- Natural language inside parentheses
- No nested brackets like `#[cfg(target_os = "...")]`
- LLMs can read and generate these naturally

---

### 2. References → Explicit Keywords

**Philosophy**: Replace symbols with words for borrowing.

```
CURRENT (Rust-like):
func process(data: &String) -> &String
func modify(data: &mut String)
let ptr: *const I32

PROPOSED (LLM-native):
func process(data: ref String) -> ref String
func modify(data: mut ref String)
let ptr: ptr I32
```

**Complete Reference Syntax**:

```tml
// Immutable reference (borrow)
ref T              // Instead of &T
ref String         // Reference to String

// Mutable reference
mut ref T          // Instead of &mut T
mut ref String     // Mutable reference to String

// Raw pointers (low-level only)
ptr T              // Instead of *const T
mut ptr T          // Instead of *mut T

// Example function signatures
func length(s: ref String) -> U64
func append(s: mut ref String, suffix: String)
func raw_access(p: ptr U8, len: U64) -> Bytes
```

**Rationale**:
- `ref` is a clear English word meaning "reference"
- `mut ref` reads naturally: "mutable reference"
- `ptr` is explicit about being a pointer
- No ambiguity with `&` (bitwise AND) or `*` (multiply/dereference)

---

### 3. Type Names → Descriptive Names

**Philosophy**: Use full, descriptive names instead of Rust abbreviations.

```
CURRENT (Rust-like):          PROPOSED (LLM-native):
Option[T]                     Maybe[T]
Result[T, E]                  Outcome[T, E] or Try[T, E]
Box[T]                        Heap[T] or Owned[T]
Rc[T]                         Shared[T]
Arc[T]                        Atomic[T] or ThreadSafe[T]
RefCell[T]                    MutCell[T]
Vec[T]                        List[T]  (already done!)
Cell[T]                       ValueCell[T]
Weak[T]                       WeakRef[T]
Pin[T]                        Pinned[T]
```

**Type Renaming Table**:

| Rust/Current | TML Proposed | Rationale |
|--------------|--------------|-----------|
| `Option[T]` | `Maybe[T]` | Haskell-inspired, clearer meaning |
| `Some(x)` | `Just(x)` | Pairs with Maybe |
| `None` | `Nothing` | Pairs with Maybe |
| `Result[T, E]` | `Outcome[T, E]` | Describes what it is |
| `Ok(x)` | `Success(x)` | Clear meaning |
| `Err(e)` | `Failure(e)` | Clear meaning |
| `Box[T]` | `Heap[T]` | Describes where it lives |
| `Rc[T]` | `Shared[T]` | Describes behavior |
| `Arc[T]` | `Sync[T]` | Thread-safe shared |
| `RefCell[T]` | `RuntimeChecked[T]` | Describes what it does |
| `Cell[T]` | `Mutable[T]` | Interior mutability |
| `Weak[T]` | `WeakRef[T]` | Clear it's a reference |
| `Pin[T]` | `Pinned[T]` | Past tense, it's pinned |

**Examples**:

```tml
// Old (Rust-like)
func find_user(id: U64) -> Option[User]
func parse_config(path: String) -> Result[Config, ParseError]
let cached: Rc[Data] = Rc.new(data)

// New (LLM-native)
func find_user(id: U64) -> Maybe[User]
func parse_config(path: String) -> Outcome[Config, ParseError]
let cached: Shared[Data] = Shared.create(data)
```

**Pattern Matching Updates**:

```tml
// Old
when result {
    Some(user) -> use(user),
    None -> handle_missing(),
}

when outcome {
    Ok(config) -> load(config),
    Err(e) -> log_error(e),
}

// New
when result {
    Just(user) -> use(user),
    Nothing -> handle_missing(),
}

when outcome {
    Success(config) -> load(config),
    Failure(e) -> log_error(e),
}
```

---

### 4. Smart Pointers → Memory Wrappers

**Philosophy**: Rename and restructure smart pointer APIs.

```tml
// Old (Rust-like)
let boxed = Box.new(value)
let shared = Rc.new(data)
let atomic = Arc.new(data)
let weak = Rc.downgrade(&shared)

// New (LLM-native)
let heaped = Heap.allocate(value)
let shared = Shared.create(data)
let synced = Sync.create(data)
let weak = shared.weak_reference()
```

**API Redesign**:

```tml
// Heap[T] (was Box[T])
Heap.allocate(value)      // Create
heaped.value              // Access inner
heaped.into_inner()       // Consume and get value

// Shared[T] (was Rc[T])
Shared.create(value)      // Create with count 1
shared.clone()            // Increment count
shared.count()            // Get reference count
shared.weak_reference()   // Create weak ref

// Sync[T] (was Arc[T])
Sync.create(value)        // Thread-safe shared
synced.clone()            // Atomic increment
synced.count()            // Atomic count
synced.weak_reference()   // Create weak ref

// WeakRef[T] (was Weak[T])
weak.upgrade()            // Try to get strong ref
weak.is_valid()           // Check if still alive
```

---

### 5. Unsafe → LowLevel

**Philosophy**: "Unsafe" has negative connotations. Use "lowlevel" to indicate system-level code.

```tml
// Old (Rust-like)
#[unsafe]
func raw_memory_access(ptr: *const U8) -> U8 {
    return *ptr
}

unsafe {
    // dangerous code
}

// New (LLM-native)
@lowlevel
func raw_memory_access(ptr: ptr U8) -> U8 {
    return ptr.read()
}

lowlevel {
    // system-level code
}
```

**LowLevel Operations**:

```tml
// Pointer operations (inside @lowlevel)
ptr.read()                // Read value at pointer
ptr.write(value)          // Write value to pointer
ptr.offset(n)             // Pointer arithmetic
ptr.as_ref()              // Convert to reference
ptr.is_null()             // Check for null

// Memory operations
memory.allocate(size)     // Raw allocation
memory.deallocate(ptr)    // Raw deallocation
memory.copy(src, dst, n)  // Raw copy

// FFI
@foreign("C")
func external_function(x: I32) -> I32
```

---

### 6. Ranges → Explicit Syntax

**Philosophy**: Replace `..` with clearer syntax.

```tml
// Old (Rust-like)
0..10        // Exclusive range
0..=10       // Inclusive range
..5          // Up to (exclusive)
5..          // From onwards

// New (LLM-native)
0 to 10           // Exclusive (0, 1, 2, ... 9)
0 through 10      // Inclusive (0, 1, 2, ... 10)
upto 5            // 0, 1, 2, 3, 4
from 5            // 5, 6, 7, ...

// Or with keywords in expressions
range(0, 10)              // Exclusive
range(0, 10, inclusive)   // Inclusive
```

**Examples**:

```tml
// Old
loop i in 0..items.len() {
    process(items[i])
}

let slice = data[5..10]
let rest = data[5..]

// New
loop i in 0 to items.len() {
    process(items[i])
}

let slice = data[5 to 10]
let rest = data[from 5]
```

---

### 7. Traits → Contracts or Behaviors

**Philosophy**: "Trait" is Rust terminology. Use more intuitive terms.

```tml
// Option A: "behavior" (describes what it defines)
behavior Printable {
    func to_text(this) -> String
}

extend User with Printable {
    func to_text(this) -> String {
        return this.name
    }
}

// Option B: "contract" (already used for pre/post)
contract Comparable {
    func compare(this, other: This) -> Ordering
}

// Option C: "interface" (familiar from other languages)
interface Serializable {
    func serialize(this) -> Bytes
    func deserialize(data: Bytes) -> This
}
```

**Recommendation**: Use `behavior` as it describes what the construct does - it defines a set of behaviors that types can implement.

```tml
// Complete example
behavior Equatable {
    func equals(this, other: ref This) -> Bool

    func not_equals(this, other: ref This) -> Bool {
        return not this.equals(other)
    }
}

behavior Ordered requires Equatable {
    func compare(this, other: ref This) -> Ordering

    func less_than(this, other: ref This) -> Bool {
        return this.compare(other) == Ordering.Less
    }
}

extend Point with Equatable {
    func equals(this, other: ref This) -> Bool {
        return this.x == other.x and this.y == other.y
    }
}
```

---

### 8. Move Semantics → Transfer

**Philosophy**: "Move" is Rust terminology. Use "transfer" for ownership transfer.

```tml
// Old (Rust-like)
let consumer = move do() {
    print(data)
}

// New (LLM-native)
let consumer = transfer do() {
    print(data)
}

// Or more explicit
let consumer = do() capturing(data) {
    print(data)
}
```

---

### 9. Clone → Copy or Duplicate

**Philosophy**: Differentiate between automatic copies and explicit duplications.

```tml
// Automatic (for simple types)
let a: I32 = 42
let b = a       // Automatic copy, a still valid

// Explicit duplication (for complex types)
let s1 = String.from("hello")
let s2 = s1.duplicate()    // Instead of .clone()

// Or
let s2 = copy s1           // Keyword-based
```

---

### 10. Full Example Comparison

**Old (Rust-like TML)**:

```tml
#[cfg(target_os = "linux")]
module platform {
    caps: [io.file]

    #[derive(Debug, Clone)]
    type FileHandle {
        fd: I32,
        path: String,
    }

    trait Readable {
        func read(this, buf: &mut [U8]) -> Result[U64, IoError];
    }

    extend FileHandle with Readable {
        func read(this, buf: &mut [U8]) -> Result[U64, IoError] {
            unsafe {
                let result = libc_read(this.fd, buf.as_ptr(), buf.len())
                if result < 0 {
                    return Err(IoError.from_errno())
                }
                return Ok(result as U64)
            }
        }
    }

    func open(path: &String) -> Result[FileHandle, IoError] {
        let fd = unsafe { libc_open(path.as_ptr()) }!
        if fd < 0 {
            return Err(IoError.from_errno())
        }
        return Ok(FileHandle { fd: fd, path: path.clone() })
    }
}
```

**New (LLM-native TML)**:

```tml
@when(os: linux)
module platform {
    caps: [io.file]

    @auto(debug, duplicate)
    type FileHandle {
        fd: I32,
        path: String,
    }

    behavior Readable {
        func read(this, buf: mut ref Bytes) -> Outcome[U64, IoError]
    }

    extend FileHandle with Readable {
        func read(this, buf: mut ref Bytes) -> Outcome[U64, IoError] {
            lowlevel {
                let result = libc_read(this.fd, buf.as_ptr(), buf.len())
                if result < 0 then {
                    return Failure(IoError.from_errno())
                }
                return Success(result as U64)
            }
        }
    }

    func open(path: ref String) -> Outcome[FileHandle, IoError] {
        let fd = lowlevel { libc_open(path.as_ptr()) }!
        if fd < 0 then {
            return Failure(IoError.from_errno())
        }
        return Success(FileHandle { fd: fd, path: path.duplicate() })
    }
}
```

---

## Summary of Changes

### Keywords Changed

| Rust/Current | TML New | Category |
|--------------|---------|----------|
| `#[...]` | `@...` | Directives |
| `#[cfg(...)]` | `@when(...)` | Platform |
| `#[derive(...)]` | `@auto(...)` | Generation |
| `#[unsafe]` | `@lowlevel` | Safety |
| `unsafe { }` | `lowlevel { }` | Safety |
| `trait` | `behavior` | Type system |
| `&T` | `ref T` | References |
| `&mut T` | `mut ref T` | References |
| `*T` | `ptr T` | Pointers |
| `..` | `to` | Ranges |
| `..=` | `through` | Ranges |
| `move` | `transfer` | Closures |
| `.clone()` | `.duplicate()` | Copying |

### Types Renamed

| Rust/Current | TML New |
|--------------|---------|
| `Option[T]` | `Maybe[T]` |
| `Some(x)` | `Just(x)` |
| `None` | `Nothing` |
| `Result[T, E]` | `Outcome[T, E]` |
| `Ok(x)` | `Success(x)` |
| `Err(e)` | `Failure(e)` |
| `Box[T]` | `Heap[T]` |
| `Rc[T]` | `Shared[T]` |
| `Arc[T]` | `Sync[T]` |
| `RefCell[T]` | `RuntimeChecked[T]` |
| `Cell[T]` | `Mutable[T]` |
| `Weak[T]` | `WeakRef[T]` |

---

## Implementation Priority

### Phase 1: Critical (Breaking Changes)
1. `#[...]` → `@...` directives
2. `&T` → `ref T` references
3. `trait` → `behavior`
4. `unsafe` → `lowlevel`

### Phase 2: Important (Type System)
5. `Option` → `Maybe`
6. `Result` → `Outcome`
7. Smart pointer renames

### Phase 3: Polish
8. Range syntax
9. Move → transfer
10. Clone → duplicate

---

## Rationale Summary

1. **No `#[...]`**: This is uniquely Rust. `@` is more universal (Java, Python decorators, etc.)

2. **No `&T`**: This conflicts with bitwise AND and is Rust-specific. `ref` is clearer.

3. **No `trait`**: This is Rust terminology. `behavior` describes what it defines.

4. **No `unsafe`**: Negative connotation. `lowlevel` is neutral and descriptive.

5. **No Rust type names**: `Maybe`, `Outcome`, `Heap`, `Shared` are self-documenting.

6. **No `..` ranges**: Symbolic and Rust-specific. `to`/`through` are natural language.

These changes make TML a genuinely new language optimized for LLM comprehension, not "Rust with square brackets for generics."
