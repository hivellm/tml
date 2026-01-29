# Arc and Shared Ownership

`Arc[T]` (Atomically Reference Counted) provides shared ownership of data
across multiple threads. When you need multiple threads to own the same
data, `Arc` is your solution.

## Why Arc?

Consider this problem: you want multiple threads to read the same data.
Normal ownership rules say one owner only. `Arc` solves this by:

1. Storing data on the heap with a reference count
2. Using atomic operations to safely count references
3. Dropping the data when the last reference is dropped

```tml
use std::sync::Arc

func main() {
    // Create an Arc holding a value
    let shared: Arc[I32] = Arc::new(42)

    // Clone to create another owner
    let shared2: Arc[I32] = shared.duplicate()

    // Both point to the same data
    assert_eq(*shared.get(), 42)
    assert_eq(*shared2.get(), 42)

    // Reference count is 2
    assert_eq(shared.strong_count(), 2)
}
```

## Basic Operations

### Creating and Cloning

```tml
use std::sync::Arc

func main() {
    let a: Arc[I32] = Arc::new(100)

    // duplicate() increments the reference count
    let b: Arc[I32] = a.duplicate()
    let c: Arc[I32] = a.duplicate()

    // All three point to the same value
    assert_eq(a.strong_count(), 3)

    // Access the value
    assert_eq(*a.get(), 100)
    assert_eq(*b.get(), 100)
    assert_eq(*c.get(), 100)
}
```

### Getting the Value

```tml
use std::sync::Arc

func main() {
    let arc: Arc[I32] = Arc::new(42)

    // get() returns a reference
    let value: ref I32 = arc.get()
    println(*value)  // 42

    // Dereference directly
    println(*arc.get())  // 42
}
```

### Checking Reference Count

```tml
use std::sync::Arc

func main() {
    let a: Arc[I32] = Arc::new(0)
    assert_eq(a.strong_count(), 1)

    let b: Arc[I32] = a.duplicate()
    assert_eq(a.strong_count(), 2)

    drop(b)
    assert_eq(a.strong_count(), 1)
}
```

### Pointer Equality

Check if two Arcs point to the same allocation:

```tml
use std::sync::Arc

func main() {
    let a: Arc[I32] = Arc::new(42)
    let b: Arc[I32] = a.duplicate()
    let c: Arc[I32] = Arc::new(42)  // Different allocation!

    // a and b point to same allocation
    assert(a.ptr_eq(ref b))

    // a and c have same value but different allocations
    assert(not a.ptr_eq(ref c))
    assert_eq(*a.get(), *c.get())  // Same value though
}
```

## Arc with Mutex: Shared Mutable State

`Arc[T]` provides shared ownership, but `T` is immutable. To share
**mutable** state, combine `Arc` with `Mutex`:

```tml
use std::sync::{Arc, Mutex}
use std::thread

func main() {
    // Arc[Mutex[I32]] = shared ownership + mutable access
    let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))

    let handles: Vec[JoinHandle[Unit]] = Vec::new()

    for i in 0 to 10 {
        let c: Arc[Mutex[I32]] = counter.duplicate()
        let handle: JoinHandle[Unit] = thread::spawn(do() {
            let guard: MutexGuard[I32] = c.lock()
            *guard.get_mut() = *guard.get_mut() + 1
        })
        handles.push(handle)
    }

    // Wait for all threads
    for handle in handles {
        handle.join()
    }

    // Check final value
    let guard: MutexGuard[I32] = counter.lock()
    assert_eq(*guard.get(), 10)
}
```

## Weak References

`Weak[T]` is a non-owning reference that doesn't keep the data alive.
Use it to break reference cycles or for optional access.

### Creating Weak References

```tml
use std::sync::{Arc, Weak}

func main() {
    let strong: Arc[I32] = Arc::new(42)
    assert_eq(strong.strong_count(), 1)
    assert_eq(strong.weak_count(), 0)

    // Create a weak reference
    let weak: Weak[I32] = strong.downgrade()
    assert_eq(strong.strong_count(), 1)  // Unchanged
    assert_eq(strong.weak_count(), 1)    // Now 1
}
```

### Upgrading Weak to Arc

```tml
use std::sync::{Arc, Weak}

func main() {
    let strong: Arc[I32] = Arc::new(42)
    let weak: Weak[I32] = strong.downgrade()

    // Upgrade succeeds while strong reference exists
    when weak.upgrade() {
        Just(arc) => println("Value: ", *arc.get()),  // Value: 42
        Nothing => println("Data was dropped")
    }

    // Drop the strong reference
    drop(strong)

    // Now upgrade fails
    when weak.upgrade() {
        Just(arc) => println("Value: ", *arc.get()),
        Nothing => println("Data was dropped")  // This runs
    }
}
```

### Dangling Weak Reference

Create a weak reference that doesn't point to anything:

```tml
use std::sync::Weak

func main() {
    // Create a dangling weak reference
    let weak: Weak[I32] = Weak::new[I32]()

    // upgrade() always returns Nothing
    when weak.upgrade() {
        Just(arc) => println("Has data"),
        Nothing => println("No data")  // This runs
    }

    assert_eq(weak.strong_count(), 0)
}
```

## try_unwrap: Getting the Value Out

If there's exactly one strong reference, you can unwrap the Arc:

```tml
use std::sync::Arc

func main() {
    let arc: Arc[I32] = Arc::new(42)

    // Only one reference, unwrap succeeds
    when arc.try_unwrap() {
        Ok(value) => println("Got value: ", value),  // 42
        Err(arc) => println("Still shared")
    }
}

func main2() {
    let arc1: Arc[I32] = Arc::new(42)
    let arc2: Arc[I32] = arc1.duplicate()

    // Multiple references, unwrap fails
    when arc1.try_unwrap() {
        Ok(value) => println("Got value"),
        Err(arc) => {
            println("Still shared, count: ", arc.strong_count())  // 2
        }
    }
}
```

## get_mut: Exclusive Mutable Access

If there's exactly one strong reference (and no weak references), you can
get mutable access:

```tml
use std::sync::Arc

func main() {
    let arc: Arc[I32] = Arc::new(0)

    // Only one reference, can get mutable access
    when arc.get_mut() {
        Just(ptr) => {
            *ptr = 42
            println("Modified to 42")
        },
        Nothing => println("Still shared")
    }

    assert_eq(*arc.get(), 42)
}
```

## Thread Safety

`Arc[T]` is `Send` and `Sync` when `T` is `Send` and `Sync`:

```tml
use std::sync::Arc
use std::thread

func main() {
    let data: Arc[I32] = Arc::new(42)

    // Can send Arc to another thread
    let handle: JoinHandle[I32] = thread::spawn(do() -> I32 {
        return *data.get()
    })

    let result: I32 = handle.join().unwrap()
    assert_eq(result, 42)
}
```

## Common Patterns

### Sharing Read-Only Configuration

```tml
use std::sync::Arc

type Config {
    port: U16,
    host: Str,
    max_connections: I32
}

func main() {
    let config: Arc[Config] = Arc::new(Config {
        port: 8080,
        host: "localhost",
        max_connections: 100
    })

    // Share with multiple workers
    for i in 0 to 4 {
        let cfg: Arc[Config] = config.duplicate()
        thread::spawn(do() {
            println("Worker ", i, " using port ", cfg.get().port)
        })
    }
}
```

### Breaking Reference Cycles

Without weak references, cyclic structures would never be freed:

```tml
use std::sync::{Arc, Weak, Mutex}

type Node {
    value: I32,
    parent: Mutex[Maybe[Weak[Node]]],   // Weak to parent
    children: Mutex[Vec[Arc[Node]]]     // Strong to children
}

func main() {
    let root: Arc[Node] = Arc::new(Node {
        value: 0,
        parent: Mutex::new(Nothing),
        children: Mutex::new(Vec::new())
    })

    let child: Arc[Node] = Arc::new(Node {
        value: 1,
        parent: Mutex::new(Just(root.downgrade())),  // Weak ref to parent
        children: Mutex::new(Vec::new())
    })

    // Add child to root
    root.get().children.lock().get_mut().push(child.duplicate())

    // When root is dropped, child's weak parent becomes invalid
    // No cycle keeps them alive forever
}
```

## Arc vs Rc

| `Arc[T]` | `Rc[T]` |
|----------|---------|
| Thread-safe (atomic operations) | Single-threaded only |
| Slightly slower | Faster |
| `Send` + `Sync` | NOT `Send`, NOT `Sync` |
| Use for multi-threaded code | Use for single-threaded code |

TML's `Arc[T]` is in `std::sync`, while `Rc[T]` (when needed) would be
in a different module for single-threaded use.

## Best Practices

1. **Prefer Arc[Mutex[T]] for shared mutable state**
   ```tml
   let shared: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))
   ```

2. **Use Weak[T] to break cycles**
   - Parent -> Child: Strong reference
   - Child -> Parent: Weak reference

3. **Check strong_count() for debugging, not logic**
   - The count can change at any time in multi-threaded code
   - Don't make decisions based on the count

4. **Prefer channels for communication**
   - Arc is for shared state
   - Channels are for message passing
   - Use the right tool for the job
