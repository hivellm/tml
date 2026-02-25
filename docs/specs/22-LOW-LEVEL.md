# TML v1.0 — Low-Level Primitives

## 1. Overview

TML provides low-level primitives for systems programming. These operations are essential for:
- Memory management
- Hardware interaction
- Performance-critical code
- Operating system development
- Self-hosting the compiler

## 2. Raw Pointers

### 2.1 Pointer Types

```tml
// Immutable raw pointer
*const T

// Mutable raw pointer
*mut T

// Void pointers (opaque)
*const Void
*mut Void

// Function pointers
*func(A, B) -> C
```

### 2.2 Pointer Creation

```tml
func pointer_examples() {
    var x: I32 = 42
    var arr: [I32; 5] = [1, 2, 3, 4, 5]

    // From references (safe)
    let p: *const I32 = ptr_of(ref x)
    let pm: *mut I32 = ptr_of_mut(mut ref x)

    // From array
    let arr_ptr: *const I32 = arr.as_ptr()
    let arr_ptr_mut: *mut I32 = arr.as_mut_ptr()

    // Null pointer
    let null_ptr: *const I32 = null

    // From address (lowlevel)
    lowlevel {
        let addr_ptr: *mut I32 = 0x1000 as *mut I32
    }
}
```

### 2.3 Pointer Operations

```tml
extend *const T {
    /// Check if null
    pub func is_null(this) -> Bool

    /// Offset by count elements
    pub func offset(this, count: I64) -> *const T

    /// Add positive offset
    pub func add(this, count: U64) -> *const T

    /// Subtract offset
    pub func sub(this, count: U64) -> *const T

    /// Distance between pointers (in elements)
    pub func offset_from(this, origin: *const T) -> I64

    /// Read value (lowlevel)
    public lowlevel func read(this) -> T

    /// Read volatile (no optimization)
    public lowlevel func read_volatile(this) -> T

    /// Read unaligned
    public lowlevel func read_unaligned(this) -> T

    /// Cast to different pointer type
    pub func cast[U](this) -> *const U

    /// Convert to usize
    pub func addr(this) -> U64
}

extend *mut T {
    // All *const T methods plus:

    /// Write value (lowlevel)
    public lowlevel func write(this, value: T)

    /// Write volatile
    public lowlevel func write_volatile(this, value: T)

    /// Write unaligned
    public lowlevel func write_unaligned(this, value: T)

    /// Write bytes (memset)
    public lowlevel func write_bytes(this, value: U8, count: U64)

    /// Copy from source (memcpy, non-overlapping)
    public lowlevel func copy_from_nonoverlapping(this, src: *const T, count: U64)

    /// Copy from source (memmove, may overlap)
    public lowlevel func copy_from(this, src: *const T, count: U64)

    /// Swap values at two pointers
    public lowlevel func swap(this, other: *mut T)

    /// Replace value, returning old
    public lowlevel func replace(this, value: T) -> T

    /// Drop value in place
    public lowlevel func drop_in_place(this)
}
```

### 2.4 Pointer Arithmetic

```tml
lowlevel func array_sum(ptr: *const I32, len: U64) -> I32 {
    var sum: I32 = 0
    var current: ptr U8 = ptr

    loop i in 0 to len {
        sum += current.read()
        current = current.add(1)
    }

    return sum
}

lowlevel func reverse_array(ptr: *mut I32, len: U64) {
    var left: ptr U8 = ptr
    var right: ptr U8 = ptr.add(len - 1)

    loop while left.addr() < right.addr() {
        left.swap(right)
        left = left.add(1)
        right = right.sub(1)
    }
}
```

## 3. Bit Operations

### 3.1 Bitwise Operators

```tml
// All integer types support these operators

a & b       // Bitwise AND
a | b       // Bitwise OR
a ^ b       // Bitwise XOR
~a          // Bitwise NOT (complement)
a << n      // Left shift
a >> n      // Right shift (arithmetic for signed, logical for unsigned)
a >>> n     // Logical right shift (always zero-fill)
```

### 3.2 Bit Manipulation Methods

```tml
extend U64 {
    /// Count leading zeros
    pub func leading_zeros(this) -> U32

    /// Count trailing zeros
    pub func trailing_zeros(this) -> U32

    /// Count ones (population count)
    pub func count_ones(this) -> U32

    /// Count zeros
    pub func count_zeros(this) -> U32

    /// Rotate left
    pub func rotate_left(this, n: U32) -> U64

    /// Rotate right
    pub func rotate_right(this, n: U32) -> U64

    /// Reverse bits
    pub func reverse_bits(this) -> U64

    /// Swap bytes (endianness)
    pub func swap_bytes(this) -> U64

    /// Convert to big endian
    pub func to_be(this) -> U64

    /// Convert to little endian
    pub func to_le(this) -> U64

    /// Convert from big endian
    pub func from_be(value: U64) -> U64

    /// Convert from little endian
    pub func from_le(value: U64) -> U64

    /// Check if power of two
    pub func is_power_of_two(this) -> Bool

    /// Next power of two
    pub func next_power_of_two(this) -> U64

    /// Get bit at position
    pub func bit(this, pos: U32) -> Bool {
        return (this >> pos) & 1 != 0
    }

    /// Set bit at position
    pub func set_bit(this, pos: U32) -> U64 {
        return this | (1 << pos)
    }

    /// Clear bit at position
    pub func clear_bit(this, pos: U32) -> U64 {
        return this & ~(1 << pos)
    }

    /// Toggle bit at position
    pub func toggle_bit(this, pos: U32) -> U64 {
        return this ^ (1 << pos)
    }

    /// Extract bit field
    pub func extract_bits(this, start: U32, len: U32) -> U64 {
        return (this >> start) & ((1 << len) - 1)
    }

    /// Insert bit field
    pub func insert_bits(this, value: U64, start: U32, len: U32) -> U64 {
        let mask: U64 = ((1 << len) - 1) << start
        return (this & ~mask) | ((value << start) & mask)
    }
}

// Same methods for I8, I16, I32, I64, I128, U8, U16, U32, U128
```

### 3.3 Byte Arrays

```tml
extend U32 {
    /// Convert to bytes (native endian)
    pub func to_ne_bytes(this) -> [U8; 4]

    /// Convert to bytes (big endian)
    pub func to_be_bytes(this) -> [U8; 4]

    /// Convert to bytes (little endian)
    pub func to_le_bytes(this) -> [U8; 4]

    /// From bytes (native endian)
    pub func from_ne_bytes(bytes: [U8; 4]) -> U32

    /// From bytes (big endian)
    pub func from_be_bytes(bytes: [U8; 4]) -> U32

    /// From bytes (little endian)
    pub func from_le_bytes(bytes: [U8; 4]) -> U32
}

// Similar for all integer types
```

### 3.4 Bit Casting

```tml
/// Reinterpret bits without conversion
public lowlevel func transmute[T, U](value: T) -> U
    where size_of[T]() == size_of[U]()

// Examples
func bit_cast_examples() {
    // Float to int bits
    let f: F32 = 3.14
    let bits: U32 = lowlevel { transmute(f) }

    // Int bits to float
    let back: F32 = lowlevel { transmute(bits) }

    // Pointer to integer
    let ptr: *const I32 = ptr_of(ref x)
    let addr: U64 = ptr.addr()  // Safe alternative
}
```

## 4. Memory Operations

### 4.1 Size and Alignment

```tml
/// Size of type in bytes
pub func size_of[T]() -> U64

/// Alignment of type in bytes
pub func align_of[T]() -> U64

/// Size of value
pub func size_of_val[T](val: ref T) -> U64

/// Alignment of value
pub func align_of_val[T](val: ref T) -> U64

/// Minimum alignment (usually 1)
public const MIN_ALIGN: U64 = 1

/// Maximum alignment
public const MAX_ALIGN: U64 = 16  // Platform-dependent
```

### 4.2 Memory Layout

```tml
pub type Layout {
    size: U64,
    align: U64,
}

extend Layout {
    /// Create layout for type
    pub func new[T]() -> This {
        return This { size: size_of[T](), align: align_of[T]() }
    }

    /// Create with explicit size and alignment
    pub func from_size_align(size: U64, align: U64) -> Outcome[This, LayoutError]

    /// Create for array of T
    pub func array[T](n: U64) -> Outcome[This, LayoutError]

    /// Extend layout for next field
    pub func extend(this, next: Layout) -> Outcome[(This, U64), LayoutError]

    /// Pad to alignment
    pub func pad_to_align(this) -> This

    /// Repeat layout n times
    pub func repeat(this, n: U64) -> Outcome[(This, U64), LayoutError]
}
```

### 4.3 Raw Memory Functions

```tml
module mem

/// Copy bytes (non-overlapping)
public lowlevel func copy[T](dst: *mut T, src: *const T, count: U64)

/// Copy bytes (may overlap)
public lowlevel func copy_overlapping[T](dst: *mut T, src: *const T, count: U64)

/// Set bytes to value
public lowlevel func set[T](dst: *mut T, value: U8, count: U64)

/// Compare bytes
public lowlevel func compare[T](a: *const T, b: *const T, count: U64) -> I32

/// Zero memory
public lowlevel func zero[T](dst: *mut T, count: U64)

/// Swap memory regions
public lowlevel func swap[T](a: *mut T, b: *mut T, count: U64)

/// Replace value, returning old
pub func replace[T](dest: mut ref T, value: T) -> T

/// Take value, leaving default
pub func take[T: Default](dest: mut ref T) -> T

/// Swap two values
pub func swap_refs[T](a: mut ref T, b: mut ref T)

/// Forget value (don't run destructor)
pub func forget[T](value: T)

/// Drop value explicitly
pub func drop[T](value: T)

/// Check if types have same layout
pub func same_layout[T, U]() -> Bool

/// Align address up
pub func align_up(addr: U64, align: U64) -> U64

/// Align address down
pub func align_down(addr: U64, align: U64) -> U64

/// Check if address is aligned
pub func is_aligned(addr: U64, align: U64) -> Bool
```

### 4.4 Uninitialized Memory

```tml
/// Marker for uninitialized memory
pub type MaybeUninit[T] {
    value: T,  // May be uninitialized
}

extend MaybeUninit[T] {
    /// Create uninitialized
    pub func uninit() -> This

    /// Create zeroed (all bytes zero)
    pub func zeroed() -> This

    /// Create from initialized value
    pub func new(value: T) -> This

    /// Write value
    pub func write(this, value: T) -> mut ref T

    /// Assume initialized (lowlevel)
    public lowlevel func assume_init(this) -> T

    /// Get pointer to value
    pub func as_ptr(this) -> *const T

    /// Get mutable pointer to value
    pub func as_mut_ptr(this) -> *mut T
}

/// Create uninitialized array
pub func uninit_array[T, const N: U64]() -> [MaybeUninit[T]; N]
```

## 5. Atomic Operations

### 5.1 Atomic Types

```tml
pub type AtomicBool { inner: U8 }
pub type AtomicI8 { inner: I8 }
pub type AtomicI16 { inner: I16 }
pub type AtomicI32 { inner: I32 }
pub type AtomicI64 { inner: I64 }
pub type AtomicU8 { inner: U8 }
pub type AtomicU16 { inner: U16 }
pub type AtomicU32 { inner: U32 }
pub type AtomicU64 { inner: U64 }
pub type AtomicPtr[T] { inner: *mut T }
```

### 5.2 Memory Ordering

```tml
pub type MemoryOrdering =
    | Relaxed    // No synchronization
    | Acquire    // Acquire semantics (loads)
    | Release    // Release semantics (stores)
    | AcqRel     // Both acquire and release
    | SeqCst     // Sequential consistency
```

### 5.3 Atomic Operations

```tml
extend AtomicI64 {
    /// Create new atomic
    pub func new(value: I64) -> This

    /// Load value
    pub func load(this, order: MemoryOrdering) -> I64

    /// Store value
    pub func store(this, value: I64, order: MemoryOrdering)

    /// Swap values
    pub func swap(this, value: I64, order: MemoryOrdering) -> I64

    /// Compare and swap
    pub func compare_exchange(
        this,
        current: I64,
        new: I64,
        success: MemoryOrdering,
        failure: MemoryOrdering
    ) -> Outcome[I64, I64]

    /// Weak compare and swap (may fail spuriously)
    pub func compare_exchange_weak(
        this,
        current: I64,
        new: I64,
        success: MemoryOrdering,
        failure: MemoryOrdering
    ) -> Outcome[I64, I64]

    /// Fetch and add
    pub func fetch_add(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch and subtract
    pub func fetch_sub(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch and AND
    pub func fetch_and(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch and OR
    pub func fetch_or(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch and XOR
    pub func fetch_xor(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch and NAND
    pub func fetch_nand(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch max
    pub func fetch_max(this, value: I64, order: MemoryOrdering) -> I64

    /// Fetch min
    pub func fetch_min(this, value: I64, order: MemoryOrdering) -> I64

    /// Get mutable reference (when exclusively owned)
    pub func get_mut(this) -> mut ref I64
}
```

### 5.4 Fence Operations

```tml
/// Memory fence
pub func fence(order: MemoryOrdering)

/// Compiler fence (no CPU barrier)
pub func compiler_fence(order: MemoryOrdering)

/// Spin-loop hint
pub func spin_loop_hint()
```

## 6. Thread Primitives

### 6.1 Thread Creation

```tml
module thread

pub type Thread {
    handle: RawThreadHandle,
}

pub type JoinHandle[T] {
    thread: Thread,
    result: *mut Maybe[T],
}

/// Spawn new thread
pub func spawn[F, T](f: F) -> JoinHandle[T]
where F: Callable[(), T] + Sendable,
      T: Sendable
effects: [io.thread]

/// Spawn with builder
pub func spawn_builder[F, T](builder: Builder, f: F) -> Outcome[JoinHandle[T], SpawnError]
where F: Callable[(), T] + Sendable,
      T: Sendable
effects: [io.thread]

extend JoinHandle[T] {
    /// Wait for thread to finish
    pub func join(this) -> Outcome[T, JoinError]
    effects: [io.thread]

    /// Check if thread finished
    pub func is_finished(this) -> Bool

    /// Get thread reference
    pub func thread(this) -> ref Thread
}

extend Thread {
    /// Get thread ID
    pub func id(this) -> ThreadId

    /// Get thread name
    pub func name(this) -> Maybe[ref str]

    /// Unpark thread
    pub func unpark(this)
}

/// Current thread operations
pub func current() -> Thread
pub func yield_now()
pub func park()
pub func park_timeout(dur: Duration)
pub func sleep(dur: Duration) effects: [io.time]
```

### 6.2 Thread Builder

```tml
pub type Builder {
    name: Maybe[String],
    stack_size: Maybe[U64],
}

extend Builder {
    pub func new() -> This
    pub func name(this, name: String) -> This
    pub func stack_size(this, size: U64) -> This
    pub func spawn[F, T](this, f: F) -> Outcome[JoinHandle[T], SpawnError]
    where F: Callable[(), T] + Sendable,
          T: Sendable
}
```

### 6.3 Thread-Local Storage

```tml
/// Thread-local variable
@thread_local
var COUNTER: I32 = 0

/// Thread-local with lazy initialization
pub type ThreadLocal[T] {
    init: func() -> T,
}

extend ThreadLocal[T] {
    pub func new(init: func() -> T) -> This

    /// Get or initialize value
    pub func get(this) -> ref T

    /// Get mutable reference
    pub func get_mut(this) -> mut ref T

    /// Try get (Nothing if not initialized)
    pub func try_get(this) -> Maybe[ref T]
}
```

### 6.4 Synchronization Primitives

```tml
module sync

/// Mutex
pub type Mutex[T] {
    locked: AtomicBool,
    data: LowlevelCell[T],
}

extend Mutex[T] {
    pub func new(value: T) -> This
    pub func lock(this) -> MutexGuard[T]
    pub func try_lock(this) -> Maybe[MutexGuard[T]]
    pub func is_locked(this) -> Bool
    pub func get_mut(this) -> mut ref T  // When exclusively owned
    pub func into_inner(this) -> T
}

/// Read-write lock
pub type RwLock[T] {
    state: AtomicU64,
    data: LowlevelCell[T],
}

extend RwLock[T] {
    pub func new(value: T) -> This
    pub func read(this) -> RwLockReadGuard[T]
    pub func try_read(this) -> Maybe[RwLockReadGuard[T]]
    pub func write(this) -> RwLockWriteGuard[T]
    pub func try_write(this) -> Maybe[RwLockWriteGuard[T]]
}

/// Condition variable
pub type Condvar {
    waiters: AtomicU64,
}

extend Condvar {
    pub func new() -> This
    pub func wait[T](this, guard: MutexGuard[T]) -> MutexGuard[T]
    pub func wait_timeout[T](this, guard: MutexGuard[T], dur: Duration) -> (MutexGuard[T], Bool)
    pub func notify_one(this)
    pub func notify_all(this)
}

/// Once cell (lazy initialization)
pub type Once {
    state: AtomicU8,
}

extend Once {
    pub func new() -> This
    pub func call_once(this, f: func())
    pub func is_completed(this) -> Bool
}

/// Barrier
pub type Barrier {
    count: U64,
    state: AtomicU64,
}

extend Barrier {
    pub func new(count: U64) -> This
    pub func wait(this) -> BarrierWaitResult
}

/// Semaphore
pub type Semaphore {
    permits: AtomicI64,
}

extend Semaphore {
    pub func new(permits: I64) -> This
    pub func acquire(this)
    pub func try_acquire(this) -> Bool
    pub func release(this)
}
```

## 7. Lowlevel Cell

```tml
/// Interior mutability primitive
pub type LowlevelCell[T] {
    value: T,
}

extend LowlevelCell[T] {
    pub func new(value: T) -> This

    /// Get raw pointer to inner value
    pub func get(this) -> *mut T

    /// Get mutable reference (when exclusively owned)
    pub func get_mut(this) -> mut ref T

    /// Get inner value
    pub func into_inner(this) -> T
}

/// Cell (single-threaded interior mutability)
pub type Cell[T] {
    value: LowlevelCell[T],
}

extend Cell[T: Copy] {
    pub func new(value: T) -> This
    pub func get(this) -> T
    pub func set(this, value: T)
    pub func swap(this, other: ref Cell[T])
    pub func replace(this, value: T) -> T
    pub func take(this) -> T where T: Default
}

/// RefCell (runtime borrow checking)
pub type RefCell[T] {
    borrow_state: Cell[I32],
    value: LowlevelCell[T],
}

extend RefCell[T] {
    pub func new(value: T) -> This
    pub func borrow(this) -> Ref[T]
    pub func try_borrow(this) -> Outcome[Ref[T], BorrowError]
    pub func borrow_mut(this) -> RefMut[T]
    pub func try_borrow_mut(this) -> Outcome[RefMut[T], BorrowMutError]
}
```

## 8. Inline Assembly

### 8.1 Basic Syntax

```tml
lowlevel func read_timestamp() -> U64 {
    var low: U32 = 0
    var high: U32 = 0

    asm! {
        "rdtsc",
        out("eax") low,
        out("edx") high,
        options(nostack, nomem, preserves_flags)
    }

    return (high as U64) << 32 | (low as U64)
}
```

### 8.2 Operand Types

```tml
asm! {
    "instruction",

    // Input operands
    in(reg) value,           // Input in register
    in("eax") value,         // Input in specific register

    // Output operands
    out(reg) var,            // Output to variable
    out("eax") var,          // Output from specific register
    lateout(reg) var,        // Output after all inputs consumed

    // Input/output
    inout(reg) var,          // Same register for input and output
    inlateout(reg) var,      // Input, then late output

    // Clobbers
    out("memory") _,         // Clobbers memory
    clobber_abi("C"),        // Clobbers all C ABI registers

    // Options
    options(
        pure,                // No side effects
        nomem,               // No memory access
        readonly,            // Only reads memory
        nostack,             // Doesn't use stack
        preserves_flags,     // Doesn't modify flags
        noreturn,            // Doesn't return
        att_syntax           // Use AT&T syntax
    )
}
```

### 8.3 Platform-Specific Examples

```tml
@when(target_arch = "x86_64")
lowlevel func cpuid(leaf: U32) -> (U32, U32, U32, U32) {
    var eax: U32 = 0
    var ebx: U32 = 0
    var ecx: U32 = 0
    var edx: U32 = 0

    asm! {
        "cpuid",
        inout("eax") leaf => eax,
        out("ebx") ebx,
        out("ecx") ecx,
        out("edx") edx,
    }

    return (eax, ebx, ecx, edx)
}

@when(target_arch = "aarch64")
lowlevel func read_cycle_counter() -> U64 {
    var count: U64 = 0

    asm! {
        "mrs {}, cntvct_el0",
        out(reg) count,
        options(nostack, nomem)
    }

    return count
}
```

## 9. SIMD Intrinsics

### 9.1 Vector Types

```tml
@when(target_feature = "sse2")
module simd.x86

// 128-bit vectors
pub type I8x16 = [I8; 16]
pub type I16x8 = [I16; 8]
pub type I32x4 = [I32; 4]
pub type I64x2 = [I64; 2]
pub type F32x4 = [F32; 4]
pub type F64x2 = [F64; 2]

// 256-bit vectors (AVX)
@when(target_feature = "avx")
pub type I8x32 = [I8; 32]
pub type F32x8 = [F32; 8]
pub type F64x4 = [F64; 4]

// 512-bit vectors (AVX-512)
@when(target_feature = "avx512f")
pub type F32x16 = [F32; 16]
pub type F64x8 = [F64; 8]
```

### 9.2 SIMD Operations

```tml
@when(target_feature = "sse2")
module simd.x86.sse2

/// Add packed 32-bit integers
pub func add_i32x4(a: I32x4, b: I32x4) -> I32x4

/// Subtract packed 32-bit integers
pub func sub_i32x4(a: I32x4, b: I32x4) -> I32x4

/// Multiply packed 32-bit integers (low 32 bits)
pub func mul_i32x4(a: I32x4, b: I32x4) -> I32x4

/// Add packed floats
pub func add_f32x4(a: F32x4, b: F32x4) -> F32x4

/// Multiply packed floats
pub func mul_f32x4(a: F32x4, b: F32x4) -> F32x4

/// Compare equal (returns mask)
pub func cmpeq_i32x4(a: I32x4, b: I32x4) -> I32x4

/// Shuffle elements
pub func shuffle_f32x4[const MASK: U8](a: F32x4, b: F32x4) -> F32x4

/// Load aligned
public lowlevel func load_i32x4(ptr: *const I32) -> I32x4

/// Load unaligned
public lowlevel func loadu_i32x4(ptr: *const I32) -> I32x4

/// Store aligned
public lowlevel func store_i32x4(ptr: *mut I32, v: I32x4)

/// Store unaligned
public lowlevel func storeu_i32x4(ptr: *mut I32, v: I32x4)
```

### 9.3 Portable SIMD

```tml
module simd

/// Portable vector types
pub type Vec4[T] {
    data: [T; 4],
}

extend Vec4[F32] {
    pub func splat(value: F32) -> This
    pub func add(this, other: This) -> This
    pub func sub(this, other: This) -> This
    pub func mul(this, other: This) -> This
    pub func div(this, other: This) -> This
    pub func min(this, other: This) -> This
    pub func max(this, other: This) -> This
    pub func sqrt(this) -> This
    pub func sum(this) -> F32
}
```

## 10. Examples

### 10.1 Memory Pool Allocator

```tml
module pool_allocator

type Block {
    next: *mut Block,
}

type Pool {
    start: *mut U8,
    size: U64,
    block_size: U64,
    free_list: *mut Block,
}

extend Pool {
    public lowlevel func new(memory: *mut U8, size: U64, block_size: U64) -> This {
        assert(block_size >= size_of[Block]())
        assert(size >= block_size)

        var pool: This = This {
            start: memory,
            size: size,
            block_size: block_size,
            free_list: memory as *mut Block,
        }

        // Initialize free list
        let num_blocks: U64 = size / block_size
        var current: ptr Node = pool.free_list

        loop i in 0 to (num_blocks - 1) {
            let next: ptr Block = (current as *mut U8).add(block_size) as *mut Block
            current.write(Block { next: next })
            current = next
        }
        current.write(Block { next: null })

        return pool
    }

    public lowlevel func alloc(this) -> *mut U8 {
        if this.free_list.is_null() {
            return null
        }

        let block: ptr Block = this.free_list
        this.free_list = block.read().next
        return block as *mut U8
    }

    public lowlevel func free(this, ptr: *mut U8) {
        let block: ptr Block = ptr as *mut Block
        block.write(Block { next: this.free_list })
        this.free_list = block
    }
}
```

### 10.2 Lock-Free Stack

```tml
module lock_free_stack

type Node[T] {
    value: T,
    next: *mut Node[T],
}

type Stack[T] {
    head: AtomicPtr[Node[T]],
}

extend Stack[T] {
    pub func new() -> This {
        return This { head: AtomicPtr.new(null) }
    }

    pub func push(this, value: T) {
        lowlevel {
            let node: ptr Node[T] = alloc[Node[T]]()
            node.write(Node { value: value, next: null })

            loop {
                let head: ptr Node[T] = this.head.load(MemoryOrdering.Relaxed)
                (*node).next = head

                when this.head.compare_exchange_weak(
                    head, node, MemoryOrdering.Release, MemoryOrdering.Relaxed
                ) {
                    Ok(_) -> break,
                    Err(_) -> continue,
                }
            }
        }
    }

    pub func pop(this) -> Maybe[T] {
        lowlevel {
            loop {
                let head: ptr Node[T] = this.head.load(MemoryOrdering.Acquire)
                if head.is_null() {
                    return Nothing
                }

                let next: ptr Node[T] = (*head).next

                when this.head.compare_exchange_weak(
                    head, next, MemoryOrdering.Release, MemoryOrdering.Relaxed
                ) {
                    Ok(_) -> {
                        let value: T = head.read().value
                        dealloc(head)
                        return Just(value)
                    },
                    Err(_) -> continue,
                }
            }
        }
    }
}
```

### 10.3 SIMD String Search

```tml
@when(target_feature = "sse4.2")
module fast_search

import simd.x86.sse42.*

pub func find_char(haystack: ref [U8], needle: U8) -> Maybe[U64] {
    let len: U64 = haystack.len()
    let ptr: ptr U8 = haystack.as_ptr()

    // Process 16 bytes at a time
    let needle_vec: i8x16 = set1_i8x16(needle as I8)
    var i: U64 = 0

    lowlevel {
        loop while i + 16 <= len {
            let chunk: i8x16 = loadu_i8x16(ptr.add(i) as *const I8)
            let mask: i8x16 = cmpeq_i8x16(chunk, needle_vec)
            let bits: I32 = movemask_i8x16(mask)

            if bits != 0 {
                return Just(i + bits.trailing_zeros() as U64)
            }

            i += 16
        }

        // Handle remaining bytes
        loop while i < len {
            if ptr.add(i).read() == needle {
                return Just(i)
            }
            i += 1
        }
    }

    return Nothing
}
```

---

*Previous: [21-TARGETS.md](./21-TARGETS.md)*
*Next: [23-INTRINSICS.md](./23-INTRINSICS.md) — Compiler Intrinsics*
