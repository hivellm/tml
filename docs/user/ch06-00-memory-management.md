# Memory Management

TML provides explicit memory management through builtin intrinsics and smart pointer types. This chapter covers manual allocation, typed pointer operations, and smart pointers for automatic memory management.

## Memory Intrinsics

TML provides these builtin memory intrinsics for low-level memory operations:

| Function | Signature | Description |
|----------|-----------|-------------|
| `mem_alloc(size)` | `(I64) -> *Unit` | Allocate `size` bytes, returns a pointer |
| `mem_free(ptr)` | `(*Unit) -> Unit` | Free previously allocated memory |
| `ptr_read[T](ptr)` | `(*Unit) -> T` | Read a typed value from pointer |
| `ptr_write[T](ptr, value)` | `(*Unit, T) -> Unit` | Write a typed value to pointer |
| `ptr_offset(ptr, n)` | `(*Unit, I64) -> *Unit` | Move pointer by `n` bytes |
| `copy_nonoverlapping(src, dst, n)` | `(*Unit, *Unit, I64) -> Unit` | Copy `n` bytes (no overlap) |

> **Legacy aliases:** `alloc`, `dealloc`, `read_i32`, `write_i32` still work but the modern names above are preferred.

## Basic Allocation

```tml
func main() {
    // Allocate space for one I32 (4 bytes)
    let ptr = mem_alloc(4)

    // Write a value
    ptr_write[I32](ptr, 42)

    // Read it back
    let value = ptr_read[I32](ptr)
    println("{value}")  // 42

    // Free the memory
    mem_free(ptr)
}
```

## Working with Arrays

Allocate space for multiple values:

```tml
func main() {
    // Allocate space for 5 I32 values (20 bytes)
    let arr = mem_alloc(20)

    // Write values using ptr_offset
    ptr_write[I32](arr, 10)
    ptr_write[I32](ptr_offset(arr, 4), 20)
    ptr_write[I32](ptr_offset(arr, 8), 30)
    ptr_write[I32](ptr_offset(arr, 12), 40)
    ptr_write[I32](ptr_offset(arr, 16), 50)

    // Calculate sum
    let mut sum: I32 = 0
    loop i in 0 to 5 {
        sum = sum + ptr_read[I32](ptr_offset(arr, i * 4))
    }
    println("Sum: {sum}")  // Sum: 150

    mem_free(arr)
}
```

> **Tip:** For most use cases, prefer `List[T]` from `std::collections` which handles allocation and resizing automatically.

## Smart Pointers

TML provides smart pointer types for automatic memory management, similar to Rust's `Box`, `Rc`, and `Arc`:

### Heap[T] — Owned Heap Allocation

`Heap[T]` places a single value on the heap with unique ownership (like Rust's `Box[T]`):

```tml
use std::alloc::Heap

let boxed = Heap::new(42)
let value = boxed.deref()   // Access the inner value
// Memory is freed when boxed goes out of scope (Drop behavior)
```

### Shared[T] — Reference Counted

`Shared[T]` allows shared ownership with reference counting (like Rust's `Rc[T]`). Not thread-safe:

```tml
use std::sync::Shared

let a = Shared::new(42)
let b = a.duplicate()       // Increment refcount
let c = a.duplicate()       // Increment refcount

// All three point to the same allocation
// Memory freed when last reference is dropped
```

### Sync[T] — Atomic Reference Counted

`Sync[T]` is the thread-safe version of `Shared[T]` (like Rust's `Arc[T]`). Uses atomic operations for refcounting:

```tml
use std::sync::Sync

let arc = Sync::new(42)
let arc2 = arc.duplicate()  // Atomic increment

// Safe to send to other threads
// Memory freed when last reference is dropped
```

## The Drop Behavior

Types that implement the `Drop` behavior are automatically cleaned up when they go out of scope. All smart pointers and standard library collections implement `Drop`:

```tml
func example() {
    let list = List[I32].new(16)
    list.push(1)
    list.push(2)

    let map = HashMap[Str, I32]::new(16)
    map.set("a", 1)

    // Both list and map are automatically freed here
    // when they go out of scope
}
```

### @derive(Drop)

You can derive `Drop` for your own types. For types that don't have a custom `Drop`, you can implement it manually:

```tml
type MyResource {
    data: *Unit,
    size: I64,
}

impl Drop for MyResource {
    func drop(mut this) {
        mem_free(this.data)
    }
}
```

## Pointer Type Alias

The `core::ptr` module provides `Ptr` as a convenient alias for `*Unit`:

```tml
use core::ptr::Ptr

let mem: Ptr = mem_alloc(64)
// ... use memory ...
mem_free(mem)
```

## Memory Safety Guidelines

1. **Always free allocated memory**: Every `mem_alloc` should have a matching `mem_free`, or use smart pointers
2. **Don't use freed memory**: After `mem_free`, the pointer is invalid
3. **Don't free twice**: Only call `mem_free` once per allocation
4. **Respect boundaries**: Don't read/write past allocated size
5. **Prefer smart pointers**: Use `Heap[T]`, `Shared[T]`, or `Sync[T]` for automatic cleanup
6. **Prefer collections**: Use `List[T]`, `HashMap[K,V]`, etc. instead of manual array management

```tml
func main() {
    let ptr = mem_alloc(4)  // Only 4 bytes!

    ptr_write[I32](ptr, 10)           // OK
    // ptr_write[I32](ptr_offset(ptr, 4), 20)  // BAD: out of bounds!

    mem_free(ptr)
    // let x = ptr_read[I32](ptr)  // BAD: use after free!
}
```

## When to Use Manual Memory

Use manual memory management for:

1. **Implementing data structures**: Building custom collections in pure TML
2. **FFI interop**: Working with C libraries that expect raw pointers
3. **Performance-critical code**: When you need precise control over allocation patterns
4. **Custom allocators**: Implementing specialized allocation strategies (arena, pool, etc.)

For most application code, prefer smart pointers and standard library collections which handle memory automatically.
