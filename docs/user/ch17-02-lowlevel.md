# Lowlevel Blocks and Intrinsics

TML enforces memory safety by tracking ownership, lifetimes, and references
at compile time. Some operations — raw pointer arithmetic, manual allocation,
reinterpreting bytes — fall outside what the type system can verify. TML
places those operations inside `lowlevel` blocks to make them visible and
auditable.

`lowlevel` is TML's equivalent of Rust's `unsafe`. The keyword does not
disable the compiler; it creates a clearly delimited region where certain
additional capabilities become available that require programmer discipline
to use correctly.

## The lowlevel Keyword

```tml
lowlevel {
    // Raw pointer operations, memory intrinsics, and other
    // operations requiring explicit programmer attention go here.
    let ptr: *Unit = mem_alloc(64)
    ptr_write[I32](ptr, 0, 42)
    let v: I32 = ptr_read[I32](ptr, 0)
    mem_free(ptr)
}
```

Code outside a `lowlevel` block cannot invoke memory intrinsics directly.
This makes it simple to audit a codebase: searching for `lowlevel` finds all
locations where raw memory operations occur.

A `lowlevel` block is an expression and can appear anywhere an expression is
valid, including inside functions:

```tml
func read_as_i32(ptr: *Unit, byte_offset: I64) -> I32 {
    return lowlevel {
        ptr_read[I32](ptr, byte_offset)
    }
}
```

## Memory Intrinsics

The following intrinsics are available inside `lowlevel` blocks. They are
built into the compiler and do not require any import.

### mem_alloc

```tml
func mem_alloc(size: I64) -> *Unit
```

Allocates `size` bytes on the heap and returns a pointer to the start of
the allocation. The contents are uninitialized. Returns a non-null pointer
on success; panics on allocation failure.

```tml
let buf: *Unit = mem_alloc(256)
```

### mem_free

```tml
func mem_free(ptr: *Unit)
```

Releases memory previously allocated by `mem_alloc`. Every call to
`mem_alloc` must eventually be matched by exactly one call to `mem_free`.
Calling `mem_free` twice on the same pointer (double-free), or calling it
on a pointer not from `mem_alloc`, is undefined behavior.

```tml
mem_free(buf)
```

### ptr_read[T]

```tml
func ptr_read[T](ptr: *Unit, byte_offset: I64) -> T
```

Reads a `T` value from memory at `ptr + byte_offset`. The memory at that
address must hold a valid, fully-initialized value of type `T`, and the
read must not extend past the end of the allocation.

```tml
let value: I32 = ptr_read[I32](ptr, 0)
let second: I32 = ptr_read[I32](ptr, 4)  // 4 bytes past the start
```

### ptr_write[T]

```tml
func ptr_write[T](ptr: *Unit, byte_offset: I64, value: T)
```

Writes `value` of type `T` to memory at `ptr + byte_offset`. The write
must not extend past the end of the allocation.

```tml
ptr_write[I32](ptr, 0, 42)
ptr_write[I32](ptr, 4, 99)
```

### ptr_offset

```tml
func ptr_offset(ptr: *Unit, byte_count: I64) -> *Unit
```

Returns a new pointer advanced by `byte_count` bytes. This does not
dereference or modify any memory — it is pure address arithmetic.

```tml
let p0: *Unit = mem_alloc(16)
let p4: *Unit = ptr_offset(p0, 4)   // Points 4 bytes into the allocation
let p8: *Unit = ptr_offset(p0, 8)   // Points 8 bytes into the allocation
```

### copy_nonoverlapping

```tml
func copy_nonoverlapping(src: *Unit, dst: *Unit, byte_count: I64)
```

Copies `byte_count` bytes from `src` to `dst`. The two regions must not
overlap. This is equivalent to C's `memcpy`. For overlapping regions,
the behavior is undefined — use a manual loop instead.

```tml
let src: *Unit = mem_alloc(32)
let dst: *Unit = mem_alloc(32)

// Initialize src...
ptr_write[I32](src, 0, 1)
ptr_write[I32](src, 4, 2)

copy_nonoverlapping(src, dst, 32)

mem_free(src)
mem_free(dst)
```

### size_of[T]

```tml
func size_of[T]() -> I64
```

Returns the size in bytes of type `T`. The result is a compile-time
constant, though it is returned as `I64` for use in arithmetic.

```tml
let i32_size: I64 = size_of[I32]()    // 4
let f64_size: I64 = size_of[F64]()    // 8
let bool_size: I64 = size_of[Bool]()  // 1
```

### align_of[T]

```tml
func align_of[T]() -> I64
```

Returns the alignment requirement in bytes of type `T`. Allocations used
to store `T` must begin at an address that is a multiple of this value.

```tml
let i32_align: I64 = align_of[I32]()  // 4
let f64_align: I64 = align_of[F64]()  // 8
```

### Summary Table

| Intrinsic | Signature | Description |
|-----------|-----------|-------------|
| `mem_alloc` | `(I64) -> *Unit` | Allocate bytes on the heap |
| `mem_free` | `(*Unit)` | Free a heap allocation |
| `ptr_read[T]` | `(*Unit, I64) -> T` | Read T from pointer + offset |
| `ptr_write[T]` | `(*Unit, I64, T)` | Write T to pointer + offset |
| `ptr_offset` | `(*Unit, I64) -> *Unit` | Advance pointer by bytes |
| `copy_nonoverlapping` | `(*Unit, *Unit, I64)` | Copy bytes (no overlap) |
| `size_of[T]` | `() -> I64` | Size of T in bytes |
| `align_of[T]` | `() -> I64` | Alignment of T in bytes |

## Practical Patterns

### Typed Buffer

Allocate space for a fixed array of typed values:

```tml
func alloc_array[T](count: I64) -> *Unit {
    return lowlevel {
        let elem_size: I64 = size_of[T]()
        mem_alloc(count * elem_size)
    }
}

func array_get[T](ptr: *Unit, index: I64) -> T {
    return lowlevel {
        let offset: I64 = index * size_of[T]()
        ptr_read[T](ptr, offset)
    }
}

func array_set[T](ptr: *Unit, index: I64, value: T) {
    lowlevel {
        let offset: I64 = index * size_of[T]()
        ptr_write[T](ptr, offset, value)
    }
}

func main() {
    let arr: *Unit = alloc_array[I32](5)

    loop i in 0 to 5 {
        array_set[I32](arr, i, i * 10)
    }

    loop i in 0 to 5 {
        println(array_get[I32](arr, i))  // 0, 10, 20, 30, 40
    }

    lowlevel { mem_free(arr) }
}
```

### Stack-Based Arena Allocator

Allocate many small values from a single large block, then free them all at
once. This avoids per-object allocation overhead and is common in parsers
and compilers.

```tml
type Arena {
    base:   *Unit,
    offset: I64,
    size:   I64,
}

impl Arena {
    func new(size: I64) -> Arena {
        return Arena {
            base:   lowlevel { mem_alloc(size) },
            offset: 0,
            size:   size,
        }
    }

    func alloc(mut this, byte_count: I64) -> *Unit {
        if this.offset + byte_count > this.size {
            panic("Arena out of memory")
        }
        let ptr: *Unit = lowlevel {
            ptr_offset(this.base, this.offset)
        }
        this.offset = this.offset + byte_count
        return ptr
    }

    func reset(mut this) {
        // Return all memory to the pool in O(1).
        this.offset = 0
    }

    func destroy(mut this) {
        lowlevel { mem_free(this.base) }
    }
}

func main() {
    var arena: Arena = Arena::new(4096)

    let a: *Unit = arena.alloc(size_of[I32]())
    let b: *Unit = arena.alloc(size_of[I32]())

    lowlevel {
        ptr_write[I32](a, 0, 1)
        ptr_write[I32](b, 0, 2)
        println(ptr_read[I32](a, 0) + ptr_read[I32](b, 0))  // 3
    }

    arena.destroy()
}
```

### Binary Data Parsing

Read structured binary data from a byte buffer, such as a network packet
or a file format:

```tml
type PacketHeader {
    magic:   U32,
    version: U16,
    length:  U16,
}

func parse_header(buf: *Unit) -> PacketHeader {
    return lowlevel {
        PacketHeader {
            magic:   ptr_read[U32](buf, 0),
            version: ptr_read[U16](buf, 4),
            length:  ptr_read[U16](buf, 6),
        }
    }
}

func main() {
    // Simulate a received packet.
    let buf: *Unit = mem_alloc(8)
    lowlevel {
        ptr_write[U32](buf, 0, 0xDEADBEEF)
        ptr_write[U16](buf, 4, 1)
        ptr_write[U16](buf, 6, 128)
    }

    let header: PacketHeader = parse_header(buf)
    println("magic: ",   header.magic)
    println("version: ", header.version)
    println("length: ",  header.length)

    lowlevel { mem_free(buf) }
}
```

### Raw C Pointer Interop

When a C function returns a pointer that must be read or written by TML,
use `lowlevel` to perform the access:

```tml
@extern("c")
func malloc(size: U64) -> *Unit

@extern("c")
func free(ptr: *Unit)

func main() {
    // Allocate memory through C's allocator.
    let c_buf: *Unit = malloc(16)

    lowlevel {
        ptr_write[I64](c_buf, 0, 12345678)
        let v: I64 = ptr_read[I64](c_buf, 0)
        println(v)  // 12345678
    }

    // Return memory to C's allocator (not TML's mem_free).
    free(c_buf)
}
```

> **Important:** Memory allocated by C's `malloc` must be freed with C's
> `free`, not with TML's `mem_free`. They may use different heap
> implementations. Mixing them is undefined behavior.

## Inline lowlevel Expressions

`lowlevel` blocks can be used as expressions, which allows writing
utility functions with a clean interface:

```tml
func byte_swap_i32(v: I32) -> I32 {
    return lowlevel {
        let u: U32 = v as U32
        let b0: U32 = (u >> 24) and 0xFF
        let b1: U32 = (u >> 8)  and 0xFF00
        let b2: U32 = (u << 8)  and 0xFF0000
        let b3: U32 = (u << 24) and 0xFF000000
        (b0 or b1 or b2 or b3) as I32
    }
}
```

## The RAII Pattern for lowlevel Resources

Wrap raw pointer allocations in a struct that frees on drop to prevent
resource leaks:

```tml
type RawBuffer {
    ptr:  *Unit,
    size: I64,
}

impl RawBuffer {
    func new(size: I64) -> RawBuffer {
        return RawBuffer {
            ptr:  lowlevel { mem_alloc(size) },
            size: size,
        }
    }

    func as_ptr(this) -> *Unit {
        return this.ptr
    }

    // Called automatically when the value goes out of scope.
    func destroy(mut this) {
        lowlevel { mem_free(this.ptr) }
    }
}

func main() {
    let buf: RawBuffer = RawBuffer::new(1024)

    lowlevel {
        ptr_write[I32](buf.as_ptr(), 0, 99)
        println(ptr_read[I32](buf.as_ptr(), 0))  // 99
    }

    // buf goes out of scope here; destroy() is called automatically.
}
```

This pattern — owning a raw pointer in a struct with a `destroy` method —
is the recommended way to use `lowlevel` allocations safely. The RAII
wrapper restores the safety invariant at the boundary of each scope.

## When to Use lowlevel

`lowlevel` is appropriate for:

- **Implementing collections** — building `List[T]`, hash maps, or ring
  buffers using raw memory when the standard library does not have what you
  need.

- **FFI buffer management** — allocating and writing data into buffers that
  will be passed to C functions expecting raw pointers.

- **Custom allocators** — arena allocators, pool allocators, and other
  allocation strategies with special performance requirements.

- **Binary format parsing** — reading structured binary data from byte
  buffers where layout must match an external specification exactly.

- **Performance-critical inner loops** — in rare cases where the type
  system's overhead (extra copies, bounds checks) is measurable and
  significant.

Prefer safe TML code for everything else. The standard library's `List[T]`,
`HashMap[K, V]`, and `Str` types handle the vast majority of use cases
without requiring `lowlevel`.

## Safety Rules

The compiler cannot verify `lowlevel` code. You are responsible for these
invariants:

1. **Do not read uninitialized memory.** `mem_alloc` does not zero memory.
   Write a value before reading it.

2. **Do not access out-of-bounds memory.** Reading or writing past the end
   of an allocation is undefined behavior, not a runtime error.

3. **Do not use freed memory.** After `mem_free(ptr)`, the pointer is
   invalid. Reads and writes through it are undefined behavior.

4. **Do not free twice.** Calling `mem_free` on the same pointer more than
   once corrupts the heap.

5. **Do not mix allocators.** Memory from `mem_alloc` must be freed with
   `mem_free`. Memory from C's `malloc` must be freed with C's `free`.

6. **Ensure proper alignment.** `ptr_read[T]` and `ptr_write[T]` require
   that `ptr + byte_offset` is aligned to `align_of[T]()`. Misaligned
   access is undefined behavior on many platforms.

Wrapping every `lowlevel` allocation in a RAII struct (as shown above)
makes it much harder to accidentally violate rules 3 and 4.
