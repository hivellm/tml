# std::aio — Async I/O Event Loop

## Overview

The `std::aio` module provides a single-threaded event loop combining I/O polling with timer management, similar to Node.js/libuv architecture. It enables efficient handling of thousands of concurrent I/O operations.

```tml
use std::aio                                    // all re-exports
use std::aio::{Poller, TimerWheel, EventLoop}
use std::aio::{PollEvent}
```

## Architecture

Three-layer design for flexibility and composability:

- **Layer 1**: Platform FFI (`compiler/runtime/net/poll.c`)
- **Layer 2**: Poller + TimerWheel (TML wrappers)
- **Layer 3**: EventLoop (orchestration and callback dispatch)

## Core Types

### Poller

Wrapper around platform I/O polling (epoll on Linux, WSAPoll on Windows):

```tml
type Poller {
  handle: I64  // opaque platform-specific pointer
}

func Poller::new() -> Outcome[Heap[Poller], Str]
  Create poller instance

func add(poller: ref Poller, fd: I64, token: U32, flags: U32) -> Outcome[(), Str]
  Register file descriptor with token and event flags
  Returns error if FD already registered

func modify(poller: ref Poller, fd: I64, token: U32, flags: U32) -> Outcome[(), Str]
  Update registered FD's event flags and token

func remove(poller: ref Poller, fd: I64) -> Outcome[(), Str]
  Unregister file descriptor from polling

func wait(poller: ref Poller, timeout_ms: I64) -> Outcome[I64, Str]
  Poll for events, returns number of ready events
  Timeout: 0 = non-blocking, -1 = block forever, N = wait N ms

func event_at(poller: ref Poller, index: I64) -> {token: U32, flags: U32}
  Get event at index from last wait() call (0-indexed)
```

### Event Flags

```tml
const READABLE: U32 = 0x1        // Socket has data to read
const WRITABLE: U32 = 0x2        // Socket ready to write
const READWRITE: U32 = 0x3       // Both read and write
const ERROR: U32 = 0x4           // Error condition
const HUP: U32 = 0x8             // Peer hung up (connection closed)
```

### TimerWheel

Efficient O(1) timer management with 2-level hashing:

```tml
type TimerWheel {
  level0: List[List[I64]],    // 64 slots × 1ms (0-63ms)
  level1: List[List[I64]],    // 64 slots × 64ms (64-4095ms)
  overflow: List[{delay: U64, callback: I64}]
}

func TimerWheel::new(pool_size: U64) -> Heap[TimerWheel]
  Create timer wheel (pool_size = initial capacity for overflow list)

func schedule(wheel: ref TimerWheel, delay_ms: U64, callback: I64) -> U32
  Schedule timer callback, return timer ID
  Callback: I64 function pointer (opaque to TimerWheel)

func cancel(wheel: ref TimerWheel, timer_id: U32) -> Bool
  Cancel timer, return true if found and cancelled

func advance(wheel: ref TimerWheel, elapsed_ms: U64) -> I64
  Advance clock by elapsed time, return number of timers fired
  Caller must invoke callbacks for fired timers

func next_deadline_ms(wheel: ref TimerWheel) -> U64
  Get milliseconds until next timer fires
  Return 0 if no timers pending, large number if none

func len(wheel: ref TimerWheel) -> U64
  Get number of pending timers
```

### EventLoop

Single-threaded event loop orchestration:

```tml
type EventLoop {
  poller: Heap[Poller],
  timer_wheel: Heap[TimerWheel],
  callbacks: HashMap[U32, I64]  // token -> callback
}

func EventLoop::new() -> Outcome[Heap[EventLoop], Str]
  Create new event loop

func register(loop: ref EventLoop, fd: I64) -> Outcome[U32, Str]
  Register socket, auto-allocate token (returns token or error)

func modify(loop: ref EventLoop, fd: I64, flags: U32) -> Outcome[(), Str]
  Update event flags for registered socket
  Flags: READABLE, WRITABLE, READWRITE, ERROR, HUP

func deregister(loop: ref EventLoop, fd: I64) -> Outcome[(), Str]
  Unregister socket and clear callbacks

func on_readable(loop: ref EventLoop, fd: I64, callback: I64) -> Outcome[(), Str]
  Set readable callback (I64 function pointer)
  Signature: func(token: U32) -> Outcome[(), Str]

func on_writable(loop: ref EventLoop, fd: I64, callback: I64) -> Outcome[(), Str]
  Set writable callback (I64 function pointer)

func on_error(loop: ref EventLoop, fd: I64, callback: I64) -> Outcome[(), Str]
  Set error callback (I64 function pointer)

func set_timeout(loop: ref EventLoop, delay_ms: U64, callback: I64) -> U32
  Schedule timer callback, return timer ID

func clear_timer(loop: ref EventLoop, timer_id: U32) -> Bool
  Cancel scheduled timer, return true if found

func run(loop: ref EventLoop) -> Outcome[(), Str]
  Main loop (blocks until error)
  Integrates poller.wait() + timer.advance() + callback dispatch

func poll_once(loop: ref EventLoop, timeout_ms: I64) -> Outcome[(), Str]
  Single iteration for external loop integration
  Useful for integrating into existing event loops
```

## Platform Support

| Platform | Implementation | Details |
|----------|---|---|
| **Linux** | epoll | epoll_create1, epoll_ctl, epoll_wait (~150 lines C) |
| **Windows** | WSAPoll | WSAPoll with dynamic poll array management (~180 lines C) |
| **macOS** | kqueue | Via compatibility layer (~150 lines C) |

## Example: Simple Echo Server

```tml
use std::aio::{EventLoop, READABLE, WRITABLE}
use std::net::Socket

func main() {
  let loop = EventLoop::new().unwrap()
  let server = Socket::listen("127.0.0.1:8000").unwrap()

  // Register server socket for incoming connections
  loop.register(server.fd()).unwrap()
  
  // Set readable callback (fires when client connects)
  loop.on_readable(server.fd(), do(token) {
    print("Client connected on token {token}\n")
    Ok(())
  }).unwrap()

  // Set timeout callback (fires after 5 seconds)
  loop.set_timeout(5000, do(token) {
    print("5 second timeout fired\n")
    Ok(())
  })

  // Main loop
  loop.run().unwrap()
}
```

## Example: Concurrent HTTP Server

```tml
use std::aio::EventLoop
use std::net::Socket
use std::http::{Request, Response, Status}

func main() {
  let loop = EventLoop::new().unwrap()
  let server = Socket::listen("127.0.0.1:3000").unwrap()
  
  loop.register(server.fd()).unwrap()

  loop.on_readable(server.fd(), do(token) {
    when server.accept() {
      Ok(client) => {
        loop.register(client.fd()).unwrap()
        loop.on_readable(client.fd(), do(ct) {
          handle_http_request(client).unwrap()
          client.close().unwrap()
          Ok(())
        }).unwrap()
      },
      Err(e) => print("Accept error: {e}\n")
    }
    Ok(())
  }).unwrap()

  loop.run().unwrap()
}

func handle_http_request(client: ref Socket) -> Outcome[(), Str] {
  let request = client.read_http_request().unwrap()
  let response = Response::new(Status::OK)
    .with_body("Hello, World!")
  client.write(response.to_bytes()).unwrap()
  Ok(())
}
```

## Use Cases

- **Web Servers**: Handle thousands of concurrent HTTP connections
- **Proxy/Load Balancer**: Non-blocking forwarding with latency tracking
- **Message Brokers**: High-throughput event routing
- **Game Servers**: Fixed-timestep game updates with network I/O
- **Real-Time Dashboards**: WebSocket updates with minimal latency

## Performance Characteristics

- **Socket registration**: O(1) amortized
- **Event polling**: O(1) per ready event
- **Timer scheduling**: O(1) average case
- **Memory overhead**: ~100 bytes per socket + callbacks

## See Also

- [std::net](./02-NET.md) — Network sockets (TCP, UDP)
- [std::async](./14-ASYNC.md) — Async/await (built on EventLoop)
- [std::runtime](./39-RUNTIME.md) — Task executor and channel runtime
- [std::time](./16-DATETIME.md) — Time and duration types

---

*Previous: [26-MATH.md](./26-MATH.md)*
*Next: [28-HASH.md](./28-HASH.md)*
