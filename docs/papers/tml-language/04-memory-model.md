# 4. Memory Model

## 4.1 Overview

TML adopts a compile-time ownership model derived from Rust's ownership system. Rather than relying on a garbage collector (as Go and Java do) or requiring manual memory management (as C and C++ do), TML's compiler statically proves memory correctness through a borrow checker at compile time, with zero runtime overhead for the safety guarantees themselves.

The core invariant is simple: every value in a TML program has exactly one owner at any given point in time. When the owning variable goes out of scope, the value is dropped automatically via RAII (Resource Acquisition Is Initialization), with no garbage collection pause, no explicit free call, and no possibility of a double-free.

What distinguishes TML from Rust is the surface syntax: TML uses English keywords (`ref`, `mut ref`, `lowlevel`) instead of symbols (`&`, `&mut`, `unsafe`), and TML infers all lifetimes without requiring explicit annotation.

---

## 4.2 Ownership System

### 4.2.1 The Single-Owner Rule

TML enforces three rules at compile time:

1. **Every value has exactly one owner.** A value is bound to exactly one variable at any point.
2. **When the owner goes out of scope, the value is dropped.** Destructors run deterministically at scope exit.
3. **Ownership may be transferred (moved).** After a move, the source variable is permanently invalidated.

```
let a = List.new()    // 'a' owns the list
let b = a             // ownership moved to 'b'
// a.push(1)          // COMPILE ERROR: 'a' has been moved
b.push(1)             // OK: 'b' owns the list
```

### 4.2.2 Move Semantics

By default, assignment and function argument passing move values:

```
func consume(list: List[I32]) { ... }

let items = List.of(1, 2, 3)
consume(items)          // 'items' is moved into the function
// items.len()          // COMPILE ERROR: 'items' has been moved
```

Types that implement the `Copy` behavior (small, stack-allocated types like integers and booleans) are implicitly copied instead of moved. Types that implement `Duplicate` can be explicitly duplicated:

```
let a = List.of(1, 2, 3)
let b = a.duplicate()   // deep copy; both 'a' and 'b' are valid
```

---

## 4.3 References

TML's reference system is semantically identical to Rust's but uses keyword syntax:

| TML | Rust | Meaning |
|-----|------|---------|
| `ref T` | `&T` | Shared (immutable) reference |
| `mut ref T` | `&mut T` | Exclusive (mutable) reference |

### 4.3.1 Borrowing Rules

The borrow checker enforces two rules:

1. **Multiple shared references OR one mutable reference** — never both simultaneously.
2. **References must not outlive the referent** — no dangling pointers.

```
func length(s: ref Str) -> I64 {   // borrows 's' immutably
    return s.len()
}

func append(s: mut ref List[I32], value: I32) {  // borrows 's' mutably
    s.push(value)
}

let items = List.of(1, 2, 3)
let len = length(ref items)     // shared borrow — OK
append(mut ref items, 4)        // mutable borrow — OK (no other borrows active)
```

### 4.3.2 Keyword Syntax Rationale

The choice of `ref` and `mut ref` over `&` and `&mut` is a readability decision:

- `ref List[I32]` reads as "reference to List of I32" — natural English.
- `&Vec<i32>` reads as "ampersand Vec angle-bracket i32 angle-bracket" — symbol soup.

For LLMs, the keyword syntax has an additional advantage: the word "ref" activates semantic associations with "reference" from natural language training data, while `&` requires language-specific knowledge of whether it means "reference" (Rust), "address-of" (C), "bitwise AND" (most languages), or "string concatenation" (some languages).

---

## 4.4 No Explicit Lifetimes

The most significant departure from Rust's memory model is TML's complete absence of explicit lifetime annotations. In Rust, complex reference relationships require the programmer to annotate lifetimes:

```rust
// Rust: explicit lifetimes required
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
```

In TML, the equivalent function requires no lifetime annotation:

```
func longest(x: ref Str, y: ref Str) -> ref Str {
    if x.len() > y.len() then x else y
}
```

### 4.4.1 How This Works

TML's borrow checker implements Non-Lexical Lifetimes (NLL) analysis with extended inference:

1. **Lifetime elision rules** (similar to Rust's) handle common patterns automatically.
2. **Region inference** determines lifetime relationships from the control flow graph.
3. **Constraint solving** proves that all references are valid without requiring programmer annotations.

### 4.4.2 Trade-offs

| Aspect | TML (no lifetimes) | Rust (explicit lifetimes) |
|--------|-------------------|--------------------------|
| Simplicity | Dramatically simpler syntax | Lifetime annotations add noise |
| Expressiveness | Cannot express some complex patterns | Full control over lifetime relationships |
| Learning curve | Much lower — no lifetime syntax to learn | Lifetimes are the steepest part of Rust's curve |
| Self-referential structs | Limited — requires Pin | Possible with explicit lifetimes |
| LLM accuracy | Higher — no lifetime errors possible | LLMs frequently generate incorrect lifetimes |

The trade-off is intentional: TML sacrifices a small amount of expressiveness (self-referential types, complex lifetime relationships) in exchange for dramatically simpler syntax. The vast majority of real-world code does not require explicit lifetime annotations even in Rust (lifetime elision handles most cases), so TML's approach covers the common case while simplifying the language significantly.

---

## 4.5 Smart Pointers

TML provides three smart pointer types with self-documenting names:

### 4.5.1 Heap[T] (Rust: Box<T>)

Single-ownership heap allocation. The value is deallocated when the `Heap` goes out of scope.

```
let value = Heap.new(42)         // allocates I32 on the heap
let large = Heap.new(Matrix.identity(1000))  // large value on heap
```

The name `Heap` describes WHERE the value lives. Rust's `Box` is a metaphor that must be learned.

### 4.5.2 Shared[T] (Rust: Rc<T>)

Reference-counted shared ownership. Multiple `Shared` pointers can point to the same value. The value is deallocated when the last `Shared` is dropped.

```
let a = Shared.new(Config { name: "default" })
let b = a.duplicate()    // both 'a' and 'b' point to same data
// value freed when both 'a' and 'b' go out of scope
```

The name `Shared` describes HOW ownership works. Rust's `Rc` (Reference Counted) is an abbreviation of the implementation mechanism.

### 4.5.3 Sync[T] (Rust: Arc<T>)

Atomically reference-counted shared ownership. Thread-safe version of `Shared`.

```
let config = Sync.new(AppConfig.load())
// Can be sent to multiple threads safely
spawn(do() { config.read() })
```

The name `Sync` describes the SAFETY PROPERTY. Rust's `Arc` (Atomically Reference Counted) describes the implementation mechanism.

---

## 4.6 lowlevel Blocks

TML uses `lowlevel` instead of `unsafe` for blocks that bypass the borrow checker:

```
lowlevel {
    let raw = mem_alloc(size)
    ptr_write(raw, value)
    let result = ptr_read[I32](raw)
    mem_free(raw)
}
```

### 4.6.1 Available Intrinsics

| Intrinsic | Purpose |
|-----------|---------|
| `mem_alloc(size)` | Allocate raw memory |
| `mem_free(ptr)` | Free allocated memory |
| `ptr_read[T](ptr)` | Read a value from a raw pointer |
| `ptr_write(ptr, value)` | Write a value to a raw pointer |
| `ptr_offset(ptr, offset)` | Pointer arithmetic |
| `copy_nonoverlapping(src, dst, count)` | Bulk memory copy |

### 4.6.2 Naming Philosophy

The word "unsafe" carries moral connotations of irresponsibility. This framing discourages use even when low-level operations are the correct tool — for example, when implementing a high-performance data structure. "Lowlevel" is descriptively accurate: the code operates at a lower level of abstraction, bypassing the type system's safety guarantees. It is not inherently wrong; it simply requires more care.

This naming choice reduces the psychological barrier to using low-level operations when they are genuinely needed, while still clearly marking the code as requiring additional review.

---

## 4.7 Interior Mutability

TML provides the same interior mutability primitives as Rust:

| TML | Rust | Purpose |
|-----|------|---------|
| `Cell[T]` | `Cell<T>` | Copy-based interior mutability |
| `RefCell[T]` | `RefCell<T>` | Runtime-checked borrowing |
| `OnceCell[T]` | `OnceCell<T>` | Write-once lazy initialization |
| `LazyCell[T]` | `LazyCell<T>` | Lazy computation with caching |
| `UnsafeCell[T]` | `UnsafeCell<T>` | Foundation for all interior mutability |

---

## 4.8 Concurrency Safety

TML enforces thread safety through marker behaviors and synchronization primitives:

- **`Send` behavior**: Types that can be transferred between threads.
- **`SyncSafe` behavior**: Types that can be shared between threads via references.
- **`Mutex[T]`**: Mutual exclusion with data protection (the data is inside the mutex).
- **`RwLock[T]`**: Reader-writer lock.
- **`Sync[T]`**: Atomic reference counting for thread-safe shared ownership.
- **Atomic types**: `AtomicI32`, `AtomicI64`, `AtomicBool`, `AtomicPtr` for lock-free programming.
- **MPSC channels**: Multi-producer, single-consumer message passing.

The model is identical to Rust's: the type system prevents data races at compile time. `Mutex[T]` wraps the protected data, ensuring that access is only possible while holding the lock.

---

## 4.9 Comparison Matrix

| Aspect | TML | Rust | C++ | Go | Swift | Zig |
|--------|-----|------|-----|----|-------|-----|
| Memory model | Ownership | Ownership | RAII + manual | GC | ARC | Manual |
| Compile-time safety | Yes (borrow checker) | Yes (borrow checker) | Partial (RAII only) | No (GC handles) | Partial (ARC) | No |
| Runtime overhead | Zero | Zero | Zero (RAII) | GC pauses | ARC counting | Zero |
| Reference syntax | `ref T` / `mut ref T` | `&T` / `&mut T` | `T&` / `const T&` | Pointers | Implicit | `*T` |
| Lifetime annotations | Never (inferred) | Sometimes explicit | N/A | N/A | N/A | N/A |
| Unsafe syntax | `lowlevel {}` | `unsafe {}` | Always unsafe | `unsafe` | Implicit | `@import("std")` |
| Smart pointers | `Heap`/`Shared`/`Sync` | `Box`/`Rc`/`Arc` | `unique_ptr`/`shared_ptr` | N/A (GC) | N/A (ARC) | Manual |
| Data race prevention | Compile-time | Compile-time | None built-in | Runtime (race detector) | Runtime (Sendable) | None built-in |

TML's memory model is the closest to Rust's of any language. The differences are purely syntactic (keywords vs symbols, inferred vs explicit lifetimes) rather than semantic. Both languages provide the same safety guarantees with the same zero-cost abstraction principle.
