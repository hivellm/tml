# TML Standard Library: Memory Allocation

> `std.alloc` — Memory allocators and allocation strategies.

## Overview

The alloc package provides memory allocation primitives and allocator implementations. It allows fine-grained control over memory management for performance-critical applications.

## Import

```tml
import std.alloc
import std.alloc.{GlobalAlloc, Arena, Pool}
```

---

## Core Traits

### Allocator

The fundamental trait for memory allocators.

```tml
/// Memory allocator trait
public trait Allocator {
    /// Allocates memory for the given layout
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError]

    /// Deallocates memory
    /// SAFETY: ptr must have been allocated by this allocator with the given layout
    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout)

    /// Reallocates memory to a new size
    /// Default implementation allocates new, copies, deallocates old
    func reallocate(
        mut this,
        ptr: *mut U8,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result[*mut U8, AllocError] {
        let new_ptr = this.allocate(new_layout)!
        unsafe {
            intrinsics.copy_nonoverlapping(ptr, new_ptr, old_layout.size.min(new_layout.size))
            this.deallocate(ptr, old_layout)
        }
        return Ok(new_ptr)
    }

    /// Allocates zeroed memory
    func allocate_zeroed(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        let ptr = this.allocate(layout)!
        unsafe {
            intrinsics.write_bytes(ptr, 0, layout.size)
        }
        return Ok(ptr)
    }
}
```

### Layout

Describes the memory layout requirements.

```tml
/// Memory layout descriptor
public type Layout {
    size: U64,
    align: U64,
}

extend Layout {
    /// Creates a layout for a type
    public func of[T]() -> Layout {
        return Layout {
            size: size_of[T](),
            align: align_of[T](),
        }
    }

    /// Creates a layout from size and alignment
    public func from_size_align(size: U64, align: U64) -> Result[Layout, LayoutError] {
        // Alignment must be a power of 2
        if align == 0 or (align & (align - 1)) != 0 then {
            return Err(LayoutError.InvalidAlignment)
        }
        // Size must be a multiple of alignment
        if size % align != 0 then {
            return Err(LayoutError.SizeNotAligned)
        }
        return Ok(Layout { size: size, align: align })
    }

    /// Creates a layout for an array
    public func array[T](n: U64) -> Result[Layout, LayoutError] {
        let elem_layout = Layout.of[T]()
        let size = elem_layout.size.checked_mul(n)?
        return Ok(Layout { size: size, align: elem_layout.align })
    }

    /// Extends this layout with another, returning combined layout and offset
    public func extend(this, next: Layout) -> Result[(Layout, U64), LayoutError] {
        let padding = this.padding_needed_for(next.align)
        let offset = this.size + padding
        let new_size = offset + next.size
        return Ok((Layout { size: new_size, align: this.align.max(next.align) }, offset))
    }

    /// Returns padding needed to align to the given alignment
    public func padding_needed_for(this, align: U64) -> U64 {
        let misalign = this.size % align
        if misalign == 0 then 0 else align - misalign
    }

    /// Pads layout to its alignment
    public func pad_to_align(this) -> Layout {
        let padding = this.padding_needed_for(this.align)
        Layout { size: this.size + padding, align: this.align }
    }
}
```

### Errors

```tml
/// Allocation error
public type AllocError = OutOfMemory | InvalidLayout

/// Layout error
public type LayoutError = InvalidAlignment | SizeNotAligned | Overflow
```

---

## Global Allocator

The default allocator used by all standard collections.

```tml
/// The global allocator
public static GLOBAL: GlobalAlloc = GlobalAlloc {}

/// Global allocator type (wraps system allocator)
public type GlobalAlloc {}

implement Allocator for GlobalAlloc {
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        unsafe {
            let ptr = libc.aligned_alloc(layout.align, layout.size)
            if ptr.is_null() then {
                return Err(AllocError.OutOfMemory)
            }
            return Ok(ptr as *mut U8)
        }
    }

    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout) {
        libc.free(ptr as *mut Void)
    }

    func reallocate(
        mut this,
        ptr: *mut U8,
        old_layout: Layout,
        new_layout: Layout,
    ) -> Result[*mut U8, AllocError] {
        // If alignment is compatible, use realloc
        if old_layout.align == new_layout.align then {
            unsafe {
                let new_ptr = libc.realloc(ptr as *mut Void, new_layout.size)
                if new_ptr.is_null() then {
                    return Err(AllocError.OutOfMemory)
                }
                return Ok(new_ptr as *mut U8)
            }
        }
        // Otherwise fall back to default implementation
        return Allocator.reallocate(this, ptr, old_layout, new_layout)
    }
}
```

### Allocation Functions

```tml
/// Allocates memory using the global allocator
public func alloc(layout: Layout) -> Result[*mut U8, AllocError] {
    return GLOBAL.allocate(layout)
}

/// Allocates zeroed memory using the global allocator
public func alloc_zeroed(layout: Layout) -> Result[*mut U8, AllocError] {
    return GLOBAL.allocate_zeroed(layout)
}

/// Deallocates memory using the global allocator
/// SAFETY: ptr must have been allocated by the global allocator
public unsafe func dealloc(ptr: *mut U8, layout: Layout) {
    GLOBAL.deallocate(ptr, layout)
}

/// Reallocates memory using the global allocator
public func realloc(
    ptr: *mut U8,
    old_layout: Layout,
    new_layout: Layout,
) -> Result[*mut U8, AllocError] {
    return GLOBAL.reallocate(ptr, old_layout, new_layout)
}
```

---

## Arena Allocator

A bump allocator that deallocates all memory at once.

```tml
/// Arena allocator - fast bump allocation, bulk deallocation
public type Arena {
    chunks: Vec[ArenaChunk],
    current: U64,       // Index of current chunk
    offset: U64,        // Offset within current chunk
}

type ArenaChunk {
    ptr: *mut U8,
    size: U64,
}

const DEFAULT_CHUNK_SIZE: U64 = 4096

extend Arena {
    /// Creates a new arena with default chunk size
    public func new() -> Arena {
        return Arena.with_capacity(DEFAULT_CHUNK_SIZE)
    }

    /// Creates an arena with specified initial capacity
    public func with_capacity(capacity: U64) -> Arena {
        var arena = Arena {
            chunks: Vec.new(),
            current: 0,
            offset: 0,
        }
        arena.grow(capacity)
        return arena
    }

    /// Allocates memory from the arena
    public func alloc[T](mut this) -> &mut T {
        let layout = Layout.of[T]()
        let ptr = this.alloc_layout(layout)
        return unsafe { &mut *(ptr as *mut T) }
    }

    /// Allocates memory with specific layout
    public func alloc_layout(mut this, layout: Layout) -> *mut U8 {
        // Align the offset
        let aligned = self.align_offset(layout.align)

        // Check if we have space in current chunk
        if this.current < this.chunks.len() then {
            let chunk = &this.chunks[this.current]
            if aligned + layout.size <= chunk.size then {
                let ptr = unsafe { chunk.ptr.add(aligned) }
                this.offset = aligned + layout.size
                return ptr
            }
        }

        // Need a new chunk
        this.grow(layout.size.max(DEFAULT_CHUNK_SIZE))
        return this.alloc_layout(layout)
    }

    /// Allocates a slice of elements
    public func alloc_slice[T](mut this, len: U64) -> &mut [T] {
        let layout = Layout.array[T](len).unwrap()
        let ptr = this.alloc_layout(layout)
        return unsafe { slice.from_raw_parts_mut(ptr as *mut T, len) }
    }

    /// Resets the arena, keeping allocated chunks
    public func reset(mut this) {
        this.current = 0
        this.offset = 0
    }

    /// Returns total allocated bytes
    public func allocated(this) -> U64 {
        var total: U64 = 0
        loop i in 0..this.current {
            total = total + this.chunks[i].size
        }
        total = total + this.offset
        return total
    }

    func align_offset(this, align: U64) -> U64 {
        let misalign = this.offset % align
        if misalign == 0 then this.offset else this.offset + (align - misalign)
    }

    func grow(mut this, min_size: U64) {
        let size = min_size.max(DEFAULT_CHUNK_SIZE)
        let ptr = alloc(Layout { size: size, align: 16 }).unwrap()

        this.chunks.push(ArenaChunk { ptr: ptr, size: size })
        this.current = this.chunks.len() - 1
        this.offset = 0
    }
}

implement Drop for Arena {
    func drop(mut this) {
        loop chunk in this.chunks.iter() {
            unsafe {
                dealloc(chunk.ptr, Layout { size: chunk.size, align: 16 })
            }
        }
    }
}

implement Allocator for Arena {
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        return Ok(this.alloc_layout(layout))
    }

    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout) {
        // Arena doesn't deallocate individual allocations
    }
}
```

---

## Pool Allocator

A fixed-size block allocator for same-sized objects.

```tml
/// Pool allocator for fixed-size objects
public type Pool[T] {
    blocks: Vec[*mut T],
    free_list: *mut FreeNode,
    block_size: U64,
}

type FreeNode {
    next: *mut FreeNode,
}

const DEFAULT_BLOCK_SIZE: U64 = 64

extend Pool[T] {
    /// Creates a new pool
    public func new() -> Pool[T] {
        return Pool.with_block_size(DEFAULT_BLOCK_SIZE)
    }

    /// Creates a pool with specified block size
    public func with_block_size(block_size: U64) -> Pool[T] {
        var pool = Pool[T] {
            blocks: Vec.new(),
            free_list: null,
            block_size: block_size,
        }
        pool.grow()
        return pool
    }

    /// Allocates an object from the pool
    public func alloc(mut this) -> *mut T {
        if this.free_list.is_null() then {
            this.grow()
        }

        let node = this.free_list
        this.free_list = unsafe { (*node).next }
        return node as *mut T
    }

    /// Returns an object to the pool
    /// SAFETY: ptr must have been allocated from this pool
    public unsafe func free(mut this, ptr: *mut T) {
        let node = ptr as *mut FreeNode
        (*node).next = this.free_list
        this.free_list = node
    }

    /// Creates and initializes an object
    public func create(mut this, value: T) -> &mut T {
        let ptr = this.alloc()
        unsafe {
            ptr.write(value)
            return &mut *ptr
        }
    }

    /// Destroys an object and returns it to the pool
    /// SAFETY: ptr must have been allocated from this pool
    public unsafe func destroy(mut this, ptr: *mut T) {
        ptr.drop_in_place()
        this.free(ptr)
    }

    func grow(mut this) {
        let layout = Layout.array[T](this.block_size).unwrap()
        let ptr = alloc(layout).unwrap() as *mut T

        this.blocks.push(ptr)

        // Build free list
        loop i in 0..this.block_size {
            let node = unsafe { ptr.add(i) as *mut FreeNode }
            unsafe {
                (*node).next = this.free_list
            }
            this.free_list = node
        }
    }
}

implement Drop for Pool[T] {
    func drop(mut this) {
        let layout = Layout.array[T](this.block_size).unwrap()
        loop ptr in this.blocks.iter() {
            unsafe {
                dealloc(*ptr as *mut U8, layout)
            }
        }
    }
}
```

---

## Stack Allocator

A LIFO allocator for temporary allocations.

```tml
/// Stack allocator - LIFO allocation pattern
public type StackAlloc {
    buffer: *mut U8,
    size: U64,
    offset: U64,
    markers: Vec[U64],
}

extend StackAlloc {
    /// Creates a stack allocator with given size
    public func new(size: U64) -> StackAlloc {
        let ptr = alloc(Layout { size: size, align: 16 }).unwrap()
        return StackAlloc {
            buffer: ptr,
            size: size,
            offset: 0,
            markers: Vec.new(),
        }
    }

    /// Allocates memory from the stack
    public func alloc(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        let aligned = this.align_offset(layout.align)
        if aligned + layout.size > this.size then {
            return Err(AllocError.OutOfMemory)
        }
        let ptr = unsafe { this.buffer.add(aligned) }
        this.offset = aligned + layout.size
        return Ok(ptr)
    }

    /// Pushes a marker for later rollback
    public func push_marker(mut this) -> StackMarker {
        let marker = this.offset
        this.markers.push(marker)
        return StackMarker { offset: marker }
    }

    /// Pops to the last marker
    public func pop_marker(mut this) {
        when this.markers.pop() {
            Some(offset) -> this.offset = offset,
            None -> this.offset = 0,
        }
    }

    /// Resets to a specific marker
    public func reset_to(mut this, marker: StackMarker) {
        assert(marker.offset <= this.offset, "invalid marker")
        this.offset = marker.offset

        // Remove any markers after this point
        loop this.markers.last().map(do(m) *m > marker.offset).unwrap_or(false) {
            this.markers.pop()
        }
    }

    /// Resets the entire stack
    public func reset(mut this) {
        this.offset = 0
        this.markers.clear()
    }

    /// Returns remaining capacity
    public func remaining(this) -> U64 {
        this.size - this.offset
    }

    func align_offset(this, align: U64) -> U64 {
        let misalign = this.offset % align
        if misalign == 0 then this.offset else this.offset + (align - misalign)
    }
}

/// Marker for stack rollback
public type StackMarker {
    offset: U64,
}

implement Drop for StackAlloc {
    func drop(mut this) {
        unsafe {
            dealloc(this.buffer, Layout { size: this.size, align: 16 })
        }
    }
}
```

---

## Slab Allocator

Manages multiple pools for different size classes.

```tml
/// Slab allocator with multiple size classes
public type Slab {
    small: Pool[Block64],     // 1-64 bytes
    medium: Pool[Block256],   // 65-256 bytes
    large: Pool[Block1024],   // 257-1024 bytes
    huge: Vec[HugeAlloc],     // > 1024 bytes
}

type Block64 { data: [U8; 64] }
type Block256 { data: [U8; 256] }
type Block1024 { data: [U8; 1024] }

type HugeAlloc {
    ptr: *mut U8,
    layout: Layout,
}

extend Slab {
    /// Creates a new slab allocator
    public func new() -> Slab {
        return Slab {
            small: Pool.new(),
            medium: Pool.new(),
            large: Pool.new(),
            huge: Vec.new(),
        }
    }
}

implement Allocator for Slab {
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        let size = layout.size.max(layout.align)

        if size <= 64 then {
            return Ok(this.small.alloc() as *mut U8)
        } else if size <= 256 then {
            return Ok(this.medium.alloc() as *mut U8)
        } else if size <= 1024 then {
            return Ok(this.large.alloc() as *mut U8)
        } else {
            let ptr = alloc(layout)!
            this.huge.push(HugeAlloc { ptr: ptr, layout: layout })
            return Ok(ptr)
        }
    }

    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout) {
        let size = layout.size.max(layout.align)

        if size <= 64 then {
            this.small.free(ptr as *mut Block64)
        } else if size <= 256 then {
            this.medium.free(ptr as *mut Block256)
        } else if size <= 1024 then {
            this.large.free(ptr as *mut Block1024)
        } else {
            // Find and remove from huge list
            loop i in 0..this.huge.len() {
                if this.huge[i].ptr == ptr then {
                    let alloc = this.huge.remove(i)
                    dealloc(alloc.ptr, alloc.layout)
                    break
                }
            }
        }
    }
}
```

---

## Allocator Combinators

### Fallback Allocator

```tml
/// Tries primary allocator, falls back to secondary on failure
public type Fallback[P: Allocator, S: Allocator] {
    primary: P,
    secondary: S,
}

extend Fallback[P, S] where P: Allocator, S: Allocator {
    public func new(primary: P, secondary: S) -> Fallback[P, S] {
        return Fallback { primary: primary, secondary: secondary }
    }
}

implement Allocator for Fallback[P, S] where P: Allocator, S: Allocator {
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        when this.primary.allocate(layout) {
            Ok(ptr) -> return Ok(ptr),
            Err(_) -> return this.secondary.allocate(layout),
        }
    }

    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout) {
        // Need to track which allocator was used
        // Simplified: try primary first
        this.primary.deallocate(ptr, layout)
    }
}
```

### Stats Allocator

```tml
/// Wraps an allocator and tracks statistics
public type Stats[A: Allocator] {
    inner: A,
    allocated: AtomicU64,
    deallocated: AtomicU64,
    peak: AtomicU64,
    count: AtomicU64,
}

extend Stats[A] where A: Allocator {
    public func new(inner: A) -> Stats[A] {
        return Stats {
            inner: inner,
            allocated: AtomicU64.new(0),
            deallocated: AtomicU64.new(0),
            peak: AtomicU64.new(0),
            count: AtomicU64.new(0),
        }
    }

    public func stats(this) -> AllocStats {
        return AllocStats {
            allocated: this.allocated.load(Ordering.Relaxed),
            deallocated: this.deallocated.load(Ordering.Relaxed),
            peak: this.peak.load(Ordering.Relaxed),
            count: this.count.load(Ordering.Relaxed),
        }
    }

    public func current_usage(this) -> U64 {
        let alloc = this.allocated.load(Ordering.Relaxed)
        let dealloc = this.deallocated.load(Ordering.Relaxed)
        return alloc - dealloc
    }
}

public type AllocStats {
    allocated: U64,
    deallocated: U64,
    peak: U64,
    count: U64,
}

implement Allocator for Stats[A] where A: Allocator {
    func allocate(mut this, layout: Layout) -> Result[*mut U8, AllocError] {
        let ptr = this.inner.allocate(layout)!

        let size = layout.size
        let new_alloc = this.allocated.fetch_add(size, Ordering.Relaxed) + size
        this.count.fetch_add(1, Ordering.Relaxed)

        // Update peak
        var peak = this.peak.load(Ordering.Relaxed)
        let current = new_alloc - this.deallocated.load(Ordering.Relaxed)
        loop current > peak {
            when this.peak.compare_exchange_weak(peak, current, Ordering.Relaxed, Ordering.Relaxed) {
                Ok(_) -> break,
                Err(p) -> peak = p,
            }
        }

        return Ok(ptr)
    }

    unsafe func deallocate(mut this, ptr: *mut U8, layout: Layout) {
        this.inner.deallocate(ptr, layout)
        this.deallocated.fetch_add(layout.size, Ordering.Relaxed)
    }
}
```

---

## Box and Smart Pointers

### Box

Heap-allocated value with single ownership.

```tml
/// Owned heap allocation
public type Box[T, A: Allocator = GlobalAlloc] {
    ptr: *mut T,
    alloc: A,
}

extend Box[T] {
    /// Allocates a value on the heap
    public func new(value: T) -> Box[T] {
        return Box.new_in(value, GLOBAL)
    }
}

extend Box[T, A] where A: Allocator {
    /// Allocates with a specific allocator
    public func new_in(value: T, alloc: A) -> Box[T, A] {
        let layout = Layout.of[T]()
        let ptr = alloc.allocate(layout).unwrap() as *mut T
        unsafe {
            ptr.write(value)
        }
        return Box { ptr: ptr, alloc: alloc }
    }

    /// Returns the inner value, consuming the Box
    public func into_inner(this) -> T {
        unsafe {
            let value = this.ptr.read()
            let layout = Layout.of[T]()
            this.alloc.deallocate(this.ptr as *mut U8, layout)
            mem.forget(this)
            return value
        }
    }

    /// Leaks the Box, returning a static reference
    public func leak(this) -> &'static mut T {
        let ptr = this.ptr
        mem.forget(this)
        return unsafe { &mut *ptr }
    }
}

implement Deref for Box[T, A] where A: Allocator {
    type Target = T

    func deref(this) -> &T {
        return unsafe { &*this.ptr }
    }
}

implement DerefMut for Box[T, A] where A: Allocator {
    func deref_mut(mut this) -> &mut T {
        return unsafe { &mut *this.ptr }
    }
}

implement Drop for Box[T, A] where A: Allocator {
    func drop(mut this) {
        unsafe {
            this.ptr.drop_in_place()
            let layout = Layout.of[T]()
            this.alloc.deallocate(this.ptr as *mut U8, layout)
        }
    }
}
```

---

## Examples

### Using Arena for Temporary Allocations

```tml
import std.alloc.Arena

func process_data(data: &[U8]) -> Result[Output, Error] {
    // Arena for temporary allocations during processing
    var arena = Arena.new()

    // All these allocations are fast bump allocations
    let parsed: &mut Parsed = arena.alloc()
    let buffer: &mut [U8] = arena.alloc_slice(1024)
    let temp: &mut TempData = arena.alloc()

    // Process using temporary storage
    parse_into(data, parsed)!
    transform(parsed, buffer)!
    let result = finalize(buffer, temp)!

    // Arena automatically freed when it goes out of scope
    return Ok(result)
}
```

### Using Pool for Game Objects

```tml
import std.alloc.Pool

type Entity {
    id: U64,
    position: Vec3,
    velocity: Vec3,
    health: I32,
}

type EntityManager {
    pool: Pool[Entity],
    entities: HashMap[U64, *mut Entity],
    next_id: U64,
}

extend EntityManager {
    func new() -> EntityManager {
        return EntityManager {
            pool: Pool.with_block_size(256),
            entities: HashMap.new(),
            next_id: 0,
        }
    }

    func spawn(mut this, position: Vec3) -> U64 {
        let id = this.next_id
        this.next_id = this.next_id + 1

        let entity = this.pool.create(Entity {
            id: id,
            position: position,
            velocity: Vec3.zero(),
            health: 100,
        })

        this.entities.insert(id, entity as *mut Entity)
        return id
    }

    func despawn(mut this, id: U64) {
        when this.entities.remove(&id) {
            Some(ptr) -> unsafe { this.pool.destroy(ptr) },
            None -> {},
        }
    }
}
```

### Custom Allocator for Collections

```tml
import std.alloc.{Arena, Allocator}
import std.collections.Vec

func with_custom_allocator() {
    let arena = Arena.with_capacity(1024 * 1024)  // 1MB

    // Vec using arena allocator
    var numbers: Vec[I32, Arena] = Vec.new_in(&arena)

    loop i in 0..1000 {
        numbers.push(i)
    }

    // All memory freed when arena is dropped
}
```

### Tracking Allocations

```tml
import std.alloc.{Stats, GlobalAlloc, GLOBAL}

func track_allocations() {
    var tracked = Stats.new(GLOBAL)

    {
        var data: Vec[I32, Stats[GlobalAlloc]] = Vec.new_in(&tracked)
        loop i in 0..100 {
            data.push(i)
        }

        let stats = tracked.stats()
        print("Allocated: " + stats.allocated.to_string() + " bytes")
        print("Peak usage: " + stats.peak.to_string() + " bytes")
    }

    let final_stats = tracked.stats()
    print("Final: " + final_stats.allocated.to_string() + " allocated, " +
          final_stats.deallocated.to_string() + " deallocated")
}
```

---

## See Also

- [06-MEMORY.md](../specs/06-MEMORY.md) — Ownership and borrowing
- [22-LOW-LEVEL.md](../specs/22-LOW-LEVEL.md) — Raw memory operations
- [std.collections](./10-COLLECTIONS.md) — Collection types
