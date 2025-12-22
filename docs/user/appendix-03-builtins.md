# Appendix C - Builtin Functions

TML provides several builtin functions that are always available.

## I/O Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `print(...)` | `(...) -> Unit` | Print values (no newline) |
| `println(...)` | `(...) -> Unit` | Print values with newline |

```tml
print("Hello ")
println("World!")  // Hello World!
println(42)        // 42
println(true)      // true
```

## Memory Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc(size)` | `(I32) -> ptr` | Allocate bytes |
| `dealloc(ptr)` | `(ptr) -> Unit` | Free memory |
| `read_i32(ptr)` | `(ptr) -> I32` | Read 32-bit int |
| `write_i32(ptr, val)` | `(ptr, I32) -> Unit` | Write 32-bit int |
| `ptr_offset(ptr, n)` | `(ptr, I32) -> ptr` | Offset pointer |

```tml
let mem = alloc(4)
write_i32(mem, 42)
println(read_i32(mem))  // 42
dealloc(mem)
```

## Atomic Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load(ptr)` | `(ptr) -> I32` | Atomic read |
| `atomic_store(ptr, val)` | `(ptr, I32) -> Unit` | Atomic write |
| `atomic_add(ptr, val)` | `(ptr, I32) -> I32` | Fetch and add |
| `atomic_sub(ptr, val)` | `(ptr, I32) -> I32` | Fetch and subtract |
| `atomic_exchange(ptr, val)` | `(ptr, I32) -> I32` | Swap |
| `atomic_cas(ptr, exp, des)` | `(ptr, I32, I32) -> Bool` | Compare-and-swap |
| `atomic_and(ptr, val)` | `(ptr, I32) -> I32` | Atomic AND |
| `atomic_or(ptr, val)` | `(ptr, I32) -> I32` | Atomic OR |

## Synchronization Functions

### Spinlock

| Function | Signature | Description |
|----------|-----------|-------------|
| `spin_lock(ptr)` | `(ptr) -> Unit` | Acquire spinlock |
| `spin_unlock(ptr)` | `(ptr) -> Unit` | Release spinlock |
| `spin_trylock(ptr)` | `(ptr) -> Bool` | Try acquire |

### Memory Fences

| Function | Signature | Description |
|----------|-----------|-------------|
| `fence()` | `() -> Unit` | Full memory barrier |
| `fence_acquire()` | `() -> Unit` | Acquire fence |
| `fence_release()` | `() -> Unit` | Release fence |

## Channel Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `channel_create()` | `() -> ptr` | Create channel |
| `channel_send(ch, val)` | `(ptr, I32) -> Bool` | Blocking send |
| `channel_recv(ch)` | `(ptr) -> I32` | Blocking receive |
| `channel_try_send(ch, val)` | `(ptr, I32) -> Bool` | Non-blocking send |
| `channel_try_recv(ch, out)` | `(ptr, ptr) -> Bool` | Non-blocking recv |
| `channel_len(ch)` | `(ptr) -> I32` | Get length |
| `channel_close(ch)` | `(ptr) -> Unit` | Close channel |
| `channel_destroy(ch)` | `(ptr) -> Unit` | Free channel |

## Mutex Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `mutex_create()` | `() -> ptr` | Create mutex |
| `mutex_lock(m)` | `(ptr) -> Unit` | Acquire lock |
| `mutex_unlock(m)` | `(ptr) -> Unit` | Release lock |
| `mutex_try_lock(m)` | `(ptr) -> Bool` | Try acquire |
| `mutex_destroy(m)` | `(ptr) -> Unit` | Free mutex |

## WaitGroup Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `waitgroup_create()` | `() -> ptr` | Create WaitGroup |
| `waitgroup_add(wg, n)` | `(ptr, I32) -> Unit` | Add to counter |
| `waitgroup_done(wg)` | `(ptr) -> Unit` | Decrement counter |
| `waitgroup_wait(wg)` | `(ptr) -> Unit` | Wait for zero |
| `waitgroup_destroy(wg)` | `(ptr) -> Unit` | Free WaitGroup |

## Thread Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread_spawn(fn, arg)` | `(ptr, ptr) -> ptr` | Start thread |
| `thread_join(handle)` | `(ptr) -> Unit` | Wait for thread |
| `thread_yield()` | `() -> Unit` | Yield CPU |
| `thread_sleep(ms)` | `(I32) -> Unit` | Sleep milliseconds |
| `thread_id()` | `() -> I32` | Get current thread ID |
