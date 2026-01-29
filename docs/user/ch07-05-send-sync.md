# Send and Sync: Thread Safety Markers

TML uses two marker behaviors—`Send` and `Sync`—to verify thread safety
at compile time. Understanding these is key to writing correct concurrent code.

## What Are Send and Sync?

### Send

A type is `Send` if it's safe to **transfer ownership** to another thread.

```tml
use core::marker::Send

// I32 is Send - can be moved to another thread
let x: I32 = 42
thread::spawn(do() {
    println(x)  // x was moved to this thread
})
```

Most types are `Send`. Notable exceptions include types containing raw
pointers or thread-local data.

### Sync

A type is `Sync` if it's safe to **share references** between threads.

```tml
use core::marker::Sync

// I32 is Sync - can be shared via &I32
let x: I32 = 42
let r: ref I32 = ref x

// Multiple threads can have ref I32 simultaneously
// (assuming the lifetime is valid)
```

A type `T` is `Sync` if and only if `ref T` is `Send`.

## How Types Derive Send/Sync

The compiler automatically derives Send and Sync for types:

### Primitive Types

All primitive types are Send and Sync:

```tml
// All of these are Send + Sync
let a: I32 = 0
let b: Bool = true
let c: F64 = 3.14
let d: U8 = 255
```

### Structs

A struct is Send if all its fields are Send. Same for Sync:

```tml
// Point is Send + Sync because I32 is Send + Sync
type Point { x: I32, y: I32 }

// Container[T] is Send if T is Send
type Container[T] { value: T }

// Container[I32] is Send + Sync
// Container[Ptr[I32]] is NOT Send (pointers are not Send)
```

### Enums

An enum is Send if all its variants' payloads are Send:

```tml
// Maybe[T] is Send if T is Send
enum Maybe[T] {
    Just(T),
    Nothing
}

// Maybe[I32] is Send + Sync
// Maybe[Ptr[I32]] is NOT Send
```

### Tuples and Arrays

Tuples and arrays are Send/Sync if their elements are:

```tml
// (I32, Bool) is Send + Sync
let tuple: (I32, Bool) = (42, true)

// [I32; 5] is Send + Sync
let array: [I32; 5] = [1, 2, 3, 4, 5]
```

## Reference Rules

References have special rules:

| Type | Send if... | Sync if... |
|------|-----------|-----------|
| `ref T` | T is Sync | T is Sync |
| `mut ref T` | T is Send | Never |

### Shared References (ref T)

A shared reference `ref T` can be sent to another thread only if `T` is
`Sync`—meaning `T` is safe for concurrent read access:

```tml
let x: I32 = 42
let r: ref I32 = ref x

// r can be sent because I32 is Sync
// Multiple threads can read through r
```

### Mutable References (mut ref T)

A mutable reference `mut ref T` can be sent to another thread if `T` is
`Send`, but it's never `Sync`:

```tml
var x: I32 = 0
let r: mut ref I32 = mut ref x

// r can be sent (moved) to one thread
// r is NOT Sync - only one thread can have it
```

## Standard Library Types

### Types That Are Send + Sync

| Type | Why |
|------|-----|
| `AtomicI32`, `AtomicBool`, etc. | Designed for concurrent access |
| `Mutex[T]` (where T: Send) | Provides synchronized access |
| `RwLock[T]` (where T: Send+Sync) | Provides synchronized access |
| `Arc[T]` (where T: Send+Sync) | Atomic reference counting |
| `Condvar` | Thread-safe condition variable |
| `Barrier` | Thread-safe barrier |
| `Once` | Thread-safe one-time init |
| `OnceLock[T]` (where T: Send+Sync) | Thread-safe lazy init |

### Types That Are Send but NOT Sync

| Type | Why not Sync |
|------|--------------|
| `Sender[T]` | Internal state, but can be cloned |
| `Receiver[T]` | Single consumer |
| `MutexGuard[T]` | Holds a lock, must not be shared |

### Types That Are NOT Send

| Type | Why not Send |
|------|--------------|
| `Ptr[T]` | Raw pointers have no safety guarantees |
| `Rc[T]` | Non-atomic reference counting |
| `MutexGuard[T]` | Must unlock on same thread |

## Practical Implications

### Thread Spawn Requires Send

The closure passed to `thread::spawn` must be `Send`, and its return value
must be `Send`:

```tml
use std::thread

// This works: I32 is Send
thread::spawn(do() -> I32 {
    return 42
})

// This would NOT work: Ptr[I32] is not Send
// let p: Ptr[I32] = ...
// thread::spawn(do() {
//     println(*p)  // Error: Ptr[I32] is not Send
// })
```

### Arc Requires Send + Sync

`Arc[T]` is only `Send` and `Sync` when `T` is both:

```tml
use std::sync::Arc

// Works: I32 is Send + Sync
let shared: Arc[I32] = Arc::new(42)
let clone: Arc[I32] = shared.duplicate()

// Works: Mutex[I32] is Send + Sync
let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
```

### Mutex Makes Non-Sync Types Shareable

`Mutex[T]` is `Sync` even if `T` is only `Send`. This is how you share
mutable state:

```tml
use std::sync::Mutex

// Vec[I32] is Send but interior mutability makes it not Sync
// Mutex[Vec[I32]] IS Sync - safe to share across threads
let shared: Mutex[Vec[I32]] = Mutex::new(Vec::new())

// Multiple threads can safely access through the mutex
let guard: MutexGuard[Vec[I32]] = shared.lock()
guard.get_mut().push(42)
```

## Common Patterns

### Sharing Immutable Data

For read-only data, just use `Arc`:

```tml
use std::sync::Arc

let config: Arc[Config] = Arc::new(load_config())

// All threads can read config
for i in 0 to 4 {
    let c: Arc[Config] = config.duplicate()
    thread::spawn(do() {
        println(c.get().setting)
    })
}
```

### Sharing Mutable Data

For mutable data, use `Arc[Mutex[T]]`:

```tml
use std::sync::{Arc, Mutex}

let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))

for i in 0 to 10 {
    let c: Arc[Mutex[I32]] = counter.duplicate()
    thread::spawn(do() {
        let guard: MutexGuard[I32] = c.lock()
        *guard.get_mut() = *guard.get_mut() + 1
    })
}
```

### Read-Heavy Shared Data

For data that's read often and written rarely, use `Arc[RwLock[T]]`:

```tml
use std::sync::{Arc, RwLock}

let cache: Arc[RwLock[HashMap[Str, I32]]] = Arc::new(RwLock::new(HashMap::new()))

// Multiple readers
for i in 0 to 10 {
    let c: Arc[RwLock[HashMap[Str, I32]]] = cache.duplicate()
    thread::spawn(do() {
        let guard: RwLockReadGuard[HashMap[Str, I32]] = c.read()
        // Read from cache...
    })
}

// Single writer
let c: Arc[RwLock[HashMap[Str, I32]]] = cache.duplicate()
thread::spawn(do() {
    let guard: RwLockWriteGuard[HashMap[Str, I32]] = c.write()
    guard.get_mut().insert("key", 42)
})
```

## Compiler Errors

The compiler catches Send/Sync violations at compile time:

```tml
// Error: Ptr[I32] does not implement Send
let p: Ptr[I32] = ...
thread::spawn(do() {
    println(*p)  // Compile error!
})

// Error: MutexGuard cannot be sent across threads
let m: Mutex[I32] = Mutex::new(0)
let guard: MutexGuard[I32] = m.lock()
thread::spawn(do() {
    println(*guard.get())  // Compile error!
})
```

## Summary Table

| Type | Send | Sync |
|------|------|------|
| Primitives (I32, Bool, etc.) | Yes | Yes |
| `ref T` | If T: Sync | If T: Sync |
| `mut ref T` | If T: Send | No |
| `Ptr[T]` | No | No |
| `AtomicT` | Yes | Yes |
| `Mutex[T]` | If T: Send | If T: Send |
| `RwLock[T]` | If T: Send+Sync | If T: Send+Sync |
| `Arc[T]` | If T: Send+Sync | If T: Send+Sync |
| `Weak[T]` | If T: Send+Sync | If T: Send+Sync |
| `Sender[T]` | If T: Send | If T: Send |
| `Receiver[T]` | If T: Send | No |
| `MutexGuard[T]` | No | No |

## Best Practices

1. **Trust the compiler** - If it compiles, your thread safety is correct
2. **Start with Arc[Mutex[T]]** - It's the simplest correct solution
3. **Upgrade to RwLock** - When profiling shows read contention
4. **Use channels** - When passing data, not sharing it
5. **Avoid raw pointers** - They bypass all safety checks
