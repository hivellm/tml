# Channels (Go-style)

Channels provide a way for concurrent tasks to communicate by passing
messages. This approach—"share memory by communicating"—helps avoid
many concurrency pitfalls.

## What is a Channel?

A channel is a typed conduit through which you can send and receive values.
Think of it as a thread-safe queue.

## Channel Functions

| Function | Description |
|----------|-------------|
| `channel_create()` | Create a new channel |
| `channel_send(ch, value)` | Send value (blocks if full) |
| `channel_recv(ch)` | Receive value (blocks if empty) |
| `channel_try_send(ch, value)` | Non-blocking send, returns bool |
| `channel_try_recv(ch, out_ptr)` | Non-blocking receive, returns bool |
| `channel_len(ch)` | Get number of items in channel |
| `channel_close(ch)` | Mark channel as closed |
| `channel_destroy(ch)` | Free channel memory |

## Creating and Using Channels

```tml
func main() {
    let ch = channel_create()
    println("Channel created")

    // Check initial length
    println(channel_len(ch))  // 0

    // Send some values
    channel_try_send(ch, 42)
    channel_try_send(ch, 100)

    println(channel_len(ch))  // 2

    // Receive values
    let out = alloc(4)

    if channel_try_recv(ch, out) {
        println(read_i32(out))  // 42
    }

    if channel_try_recv(ch, out) {
        println(read_i32(out))  // 100
    }

    println(channel_len(ch))  // 0

    dealloc(out)
    channel_close(ch)
    channel_destroy(ch)
}
```

## Non-Blocking Operations

Use `try_send` and `try_recv` when you don't want to block:

```tml
func main() {
    let ch = channel_create()
    let out = alloc(4)

    // Try to receive from empty channel
    if channel_try_recv(ch, out) {
        println("Got value")
    } else {
        println("Channel empty")  // This runs
    }

    // Send a value
    channel_try_send(ch, 42)

    // Now receive succeeds
    if channel_try_recv(ch, out) {
        println("Got: ", read_i32(out))  // Got: 42
    }

    dealloc(out)
    channel_destroy(ch)
}
```

## Channel Patterns

### Producer-Consumer

One side produces values, another consumes:

```tml
func main() {
    let ch = channel_create()
    let out = alloc(4)

    // Producer: send numbers 1-5
    for i in 1 through 5 {
        channel_try_send(ch, i)
    }

    // Consumer: receive all
    loop {
        if channel_try_recv(ch, out) {
            println(read_i32(out))
        } else {
            break
        }
    }

    dealloc(out)
    channel_destroy(ch)
}
```

### Pipeline

Chain channels together:

```tml
func main() {
    let ch1 = channel_create()
    let ch2 = channel_create()
    let out = alloc(4)

    // Stage 1: Generate numbers
    for i in 1 through 3 {
        channel_try_send(ch1, i)
    }

    // Stage 2: Double each number
    loop {
        if channel_try_recv(ch1, out) {
            let value = read_i32(out)
            channel_try_send(ch2, value * 2)
        } else {
            break
        }
    }

    // Stage 3: Print results
    loop {
        if channel_try_recv(ch2, out) {
            println(read_i32(out))  // 2, 4, 6
        } else {
            break
        }
    }

    dealloc(out)
    channel_destroy(ch1)
    channel_destroy(ch2)
}
```

## Why Channels?

Channels are preferred over shared memory because:

1. **Clear ownership**: Data is transferred, not shared
2. **No locks needed**: The channel handles synchronization
3. **Easier reasoning**: Message flow is explicit
4. **Fewer bugs**: No race conditions on the data itself

## Best Practices

1. **Close channels when done**: Signals receivers that no more data is coming
2. **Don't send on closed channels**: Causes errors
3. **Always clean up**: Call `channel_destroy` when finished
4. **Prefer non-blocking for polling**: Use `try_recv` in loops
