# Proposal: OOP Optimizations for MIR and HIR

## Status
- **Created**: 2026-01-13
- **Status**: Draft
- **Priority**: Critical
- **Depends on**: `oop-csharp-style` (class/interface implementation complete)

## Why OOP is Hard (And Why We're Doing It Anyway)

Go and Rust deliberately avoided traditional OOP because inheritance and polymorphism are double-edged swords:

| Problem | Impact | Example |
|---------|--------|---------|
| Vtable indirection | ~5ns per call | Tight loops become slow |
| Heap fragmentation | Memory bloat | Long-running servers OOM |
| Cache misses | ~100ns penalty | Vtable not in L1 cache |
| Constructor chains | N allocations | A→B→C = 3 mallocs |
| GC pressure | Unpredictable pauses | Java's stop-the-world |

**The HTTP Server Problem:**
```
10,000 concurrent connections
Each request creates:
- Request object (500ns malloc)
- Headers object (500ns malloc)
- Body object (500ns malloc)
- Response object (500ns malloc)
- Plus destruction for each

= 4ms overhead per request just for allocation
= 40 seconds per second of requests (impossible!)
```

**TML's Answer:** We want OOP for developer ergonomics and LLM code generation, but we MUST eliminate the performance costs through aggressive compiler optimization.

## Core Philosophy

```
┌─────────────────────────────────────────────────────────────────┐
│                    TML OOP Performance Model                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Source Code          Compiler Analysis        Runtime         │
│   ───────────          ─────────────────        ───────         │
│                                                                 │
│   class Dog            ┌──────────────┐                         │
│     extends Animal  →  │ CHA: sealed? │  →  Direct call         │
│                        │ No subclass  │      (no vtable)        │
│                        └──────────────┘                         │
│                                                                 │
│   let d = Dog::new()   ┌──────────────┐                         │
│   d.bark()          →  │ Escape       │  →  Stack alloc         │
│   // d doesn't escape  │ Analysis     │      (no malloc)        │
│                        └──────────────┘                         │
│                                                                 │
│   @pool class Req      ┌──────────────┐                         │
│   Req::acquire()    →  │ Pool         │  →  Reuse object        │
│   req.release()        │ Transform    │      (no malloc)        │
│                        └──────────────┘                         │
│                                                                 │
│   arena.alloc[T]()     ┌──────────────┐                         │
│   // many objects   →  │ Arena        │  →  Bump pointer        │
│   arena.reset()        │ Allocation   │      O(1) free all      │
│                        └──────────────┘                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## What Changes

### 1. Virtual Dispatch Elimination (Zero-Cost When Possible)

**Problem:** Every virtual call goes through vtable lookup.

**Solution:** Devirtualize when the type is known at compile time.

```tml
// BEFORE: Always virtual dispatch
sealed class Dog extends Animal {
    override func speak(this) { print("woof") }
}

func main() {
    let dog = Dog::new()
    dog.speak()  // Vtable lookup: load vtable, load func ptr, indirect call
}
```

```llvm
; BEFORE (unoptimized)
%vtable = load ptr, ptr %dog
%speak_ptr = getelementptr %vtable.Dog, ptr %vtable, i32 0, i32 0
%speak = load ptr, ptr %speak_ptr
call void %speak(ptr %dog)  ; Indirect call - CPU can't predict

; AFTER (devirtualized)
call void @tml_Dog_speak(ptr %dog)  ; Direct call - CPU can inline
```

**Cases handled:**
- `let x = Dog::new()` → Exact type known
- `sealed class` → No subclasses possible
- `final func` → No override possible
- Single implementation → CHA proves only one concrete type

### 2. Stack Allocation via Escape Analysis

**Problem:** Classes always heap-allocated = malloc/free overhead.

**Solution:** Allocate on stack when object doesn't escape.

```tml
func process_request(data: Bytes) -> Str {
    let parser = RequestParser::new()  // Doesn't escape!
    let result = parser.parse(data)
    return result.body  // Only body escapes
}
```

```llvm
; BEFORE
%parser = call ptr @malloc(i64 128)
; ... use parser ...
call void @free(ptr %parser)

; AFTER (escape analysis proves non-escaping)
%parser = alloca %class.RequestParser, align 8  ; Stack allocation!
; ... use parser ...
; No free needed - stack cleanup automatic
```

**Benefit:** Stack alloc is ~50x faster than malloc.

### 3. Value Classes (Zero Overhead)

**Problem:** Small classes pay vtable overhead unnecessarily.

**Solution:** `@value` classes have struct semantics.

```tml
@value
class Point {
    x: F64
    y: F64

    func distance(this, other: Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx*dx + dy*dy).sqrt()
    }
}

// Auto-inferred @value for:
// - sealed class with no virtual methods
// - class implementing only static interface methods
```

**Layout comparison:**
```
Regular class:     [ vtable_ptr | x | y ]  = 24 bytes
@value class:      [ x | y ]               = 16 bytes (no vtable!)
```

### 4. Object Pooling (Native Support)

**Problem:** High-churn scenarios (HTTP, games) thrash allocator.

**Solution:** Built-in object pools with compiler integration.

```tml
@pool(size: 10000, thread_local: true)
class HttpRequest {
    method: HttpMethod
    path: Str
    headers: List[Header]
    body: Bytes

    func reset(mut this) {
        this.headers.clear()
        this.body.clear()
    }
}

// Usage - compiler transforms new() to pool acquire
func handle(conn: Connection) {
    let req = HttpRequest::new()  // Actually: pool.acquire()
    defer req.drop()              // Actually: pool.release(req)

    req.parse(conn.read())
    let resp = process(req)
    conn.write(resp)
}
```

**Performance:**
```
malloc/free:     ~500ns per operation
Pool acquire:    ~10ns (load from free list)
Pool release:    ~5ns (push to free list)
```

### 5. Arena Allocators

**Problem:** Related objects scattered across heap = cache misses.

**Solution:** Allocate together, free together.

```tml
func handle_request() {
    let arena = Arena::new(64.kb())
    defer arena.destroy()

    // All allocations contiguous in memory
    let req = arena.alloc[Request]()
    let headers = arena.alloc[Headers]()
    let body = arena.alloc[Body]()
    let response = arena.alloc[Response]()

    // Process request...

    // Single operation frees everything
    arena.reset()  // O(1) - just reset bump pointer
}
```

**Cache benefits:**
```
Heap allocated:     Request at 0x1000, Headers at 0x5000, Body at 0x9000
                    = 3 cache line fetches minimum

Arena allocated:    Request at 0x1000, Headers at 0x1080, Body at 0x1100
                    = 1 cache line fetch (all in same 64-byte line)
```

### 6. Constructor Chain Optimization

**Problem:** C extends B extends A = 3 constructor calls, 3 vtable writes.

**Solution:** Inline and fuse constructor chain.

```tml
class A { x: I32; func new(x: I32) -> A { A { x } } }
class B extends A { y: I32; func new(x: I32, y: I32) -> B { B { base: A::new(x), y } } }
class C extends B { z: I32; func new(x: I32, y: I32, z: I32) -> C { C { base: B::new(x, y), z } } }
```

```llvm
; BEFORE (naive)
define ptr @C_new(i32 %x, i32 %y, i32 %z) {
    %c = call ptr @malloc(...)
    %b = call ptr @B_new(%x, %y)      ; Allocates B!
    ; copy B to C.base
    call void @free(ptr %b)            ; Free temporary B
    ; ... initialize z ...
    store ptr @vtable.A, ...           ; Write A vtable
    store ptr @vtable.B, ...           ; Write B vtable
    store ptr @vtable.C, ...           ; Write C vtable
}

; AFTER (optimized)
define ptr @C_new(i32 %x, i32 %y, i32 %z) {
    %c = call ptr @malloc(...)         ; Single allocation
    store ptr @vtable.C, ...           ; Single vtable write
    store i32 %x, ...                  ; Direct field init
    store i32 %y, ...
    store i32 %z, ...
}
```

### 7. Destructor Optimization

**Problem:** Loop creates/destroys objects = N malloc + N free.

**Solution:** Hoist allocation, reuse object.

```tml
// BEFORE
loop i in 0 to 1000000 {
    let temp = Parser::new()  // 1M allocations!
    temp.parse(data[i])
}

// AFTER (compiler transforms to)
let temp = Parser::new()      // 1 allocation
loop i in 0 to 1000000 {
    temp.reset()              // Just clear state
    temp.parse(data[i])
}
```

## Implementation Files

### HIR Passes

| File | Purpose |
|------|---------|
| `compiler/src/hir/analysis/class_hierarchy.cpp` | Build class hierarchy graph |
| `compiler/src/hir/passes/devirtualize.cpp` | Devirtualization pass |
| `compiler/src/hir/passes/speculative_devirt.cpp` | Speculative devirtualization |
| `compiler/src/hir/passes/value_class_inference.cpp` | Auto-detect @value classes |

### MIR Passes

| File | Purpose |
|------|---------|
| `compiler/src/mir/passes/class_escape_analysis.cpp` | Escape analysis for classes |
| `compiler/src/mir/passes/stack_allocate_class.cpp` | Stack allocation transform |
| `compiler/src/mir/passes/pool_transform.cpp` | Pool acquire/release transform |
| `compiler/src/mir/passes/constructor_inline.cpp` | Constructor chain inlining |
| `compiler/src/mir/passes/destructor_hoist.cpp` | Loop allocation hoisting |
| `compiler/src/mir/passes/class_layout_opt.cpp` | Field reordering |

### Runtime/Core Library

| File | Purpose |
|------|---------|
| `lib/core/pool.tml` | `Pool[T]` implementation |
| `lib/core/arena.tml` | `Arena` allocator |
| `compiler/runtime/pool.c` | Lock-free pool C implementation |
| `compiler/runtime/arena.c` | Bump allocator C implementation |

## Performance Targets

| Metric | Current (Naive) | Target | How |
|--------|-----------------|--------|-----|
| Virtual call | 5ns | < 1ns | Devirtualize + inline |
| Object create | 500ns | < 50ns | Stack alloc or pool |
| Object destroy | 300ns | < 10ns | Pool return or trivial dtor |
| 10K req/s | OOM risk | Stable | Arena per request |
| Memory fragmentation | High | Low | Pool + arena |

## Comparison with Other Languages

| Feature | Java/C# | Go | Rust | TML (Target) |
|---------|---------|-----|------|--------------|
| OOP syntax | Yes | No | No | Yes |
| Virtual dispatch | Always | N/A | N/A | Devirtualized when possible |
| Stack allocation | JIT escape analysis | Escape analysis | Default | Escape analysis |
| Object pools | Manual | sync.Pool | Manual | Native `@pool` |
| Arena allocator | No | No | bumpalo crate | Native `Arena` |
| GC | Yes | Yes | No | No |
| Destructor timing | GC decides | GC decides | Deterministic | Deterministic |

## Success Criteria

1. All existing tests pass
2. New OOP optimization tests pass
3. Benchmarks show:
   - Virtual call < 5% overhead vs direct call (when devirtualized)
   - 80%+ of local objects stack-allocated
   - Pool hit rate > 95% in high-churn scenarios
   - Arena allocation 10x faster than malloc
4. HTTP server benchmark handles 100K req/s without memory growth
5. Compile time increase < 10%

## Related Tasks

| Task | Relationship |
|------|--------------|
| `oop-csharp-style` | **Prerequisite** - Provides class/interface syntax and codegen |
| `memory-safety` | **Cross-cutting** - Audits OOP memory management |
| `implement-reflection` | **Uses** - OOP reflection depends on TypeInfo with class metadata |
| `advanced-optimizations` | **Parallel** - General optimizations apply to OOP code too |

## References

- [HotSpot JIT Devirtualization](https://shipilev.net/jvm/anatomy-quarks/)
- [Go Escape Analysis](https://go.dev/doc/faq#stack_or_heap)
- [LLVM WholeProgramDevirt](https://llvm.org/docs/TypeMetadata.html)
- [mimalloc Arena Design](https://github.com/microsoft/mimalloc)
- [Rust bumpalo](https://github.com/fitzgen/bumpalo)
