# std::aio — Async I/O Event Loop

> **Status**: IMPLEMENTED (2026-02-25)
> **Module path**: `std::aio` (Phase 3 of async architecture)
> **Layer model**: 3-layer design (Platform FFI → TML Poller/TimerWheel → Event Loop + High-Level APIs)

## 1. Overview

The `std::aio` module provides a **single-threaded event loop** for I/O multiplexing and timer management, following the **Node.js (libuv) and Rust (tokio) architecture**. It combines **OS-level I/O polling** with an **efficient hashed timer wheel**, enabling scalable concurrent networking without OS threads.

```tml
use std::aio::{Poller, TimerWheel, EventLoop}
```

**Key design principles:**
- **Single-threaded** — One event loop per thread (like Node.js). Simpler concurrency model than work-stealing.
- **Token-based dispatch** — Each I/O source gets a U32 token. Events report tokens → O(1) callback lookup.
- **Callback-driven** — Callbacks are opaque I64 function pointers, cast to `func(I64)` at fire time. Similar to Node.js event model.
- **Monotonic timers** — Timer wheel advances with real system time, integrating with poller timeout.
- **Platform abstraction** — Single C FFI layer (epoll on Linux, WSAPoll on Windows) for cross-platform polling.

## 2. 3-Layer Architecture

### Layer 1: Platform FFI (`compiler/runtime/net/poll.c`)

Minimal C code (~200 lines) abstracting OS I/O polling:

| Function | Purpose |
|----------|---------|
| `sys_poll_create()` | Create a poller instance (epoll/WSAPoll) |
| `sys_poll_destroy(poller)` | Destroy the poller |
| `sys_poll_add(poller, socket, token, interests)` | Register a socket for events |
| `sys_poll_modify(poller, socket, token, interests)` | Update socket interests |
| `sys_poll_remove(poller, socket)` | Unregister a socket |
| `sys_poll_wait(poller, events_out, max_events, timeout_ms)` | Wait for events (returns count) |

**Event output format** (portable 8-byte struct):
```c
typedef struct {
    uint32_t token;    // User-assigned token (0-4B)
    uint32_t flags;    // READABLE=1, WRITABLE=2, ERROR=4, HUP=8
} PollEvent;
```

**Implementations:**
- **Linux**: `epoll_create1` + `epoll_ctl` + `epoll_wait`. Token stored in `epoll_event.data.u32`.
- **Windows**: `WSAPoll` API with dynamic `WSAPOLLFD` array. Adequate for hundreds of connections; upgrade path to wepoll/AFD for thousands.

### Layer 2: TML Poller & Timer Wheel

Pure TML implementations providing the core building blocks.

#### Poller (TML wrapper)

```tml
pub type Poller {
    handle: I64
}

pub const READABLE: U32 = 1
pub const WRITABLE: U32 = 2
pub const READWRITE: U32 = 3
pub const ERROR: U32 = 4
pub const HUP: U32 = 8

pub type PollEvent {
    token: U32,
    flags: U32,
}

impl Poller {
    /// Create a new poller instance.
    pub func new() -> Poller

    /// Register a socket with the poller.
    pub func add(mut this, socket: I64, token: U32, interests: U32) -> Outcome[Unit, IoError]

    /// Update interests for a registered socket.
    pub func modify(mut this, socket: I64, token: U32, interests: U32) -> Outcome[Unit, IoError]

    /// Remove a socket from the poller.
    pub func remove(mut this, socket: I64) -> Outcome[Unit, IoError]

    /// Wait for events. Returns number of ready events.
    /// timeout_ms: -1 = block forever, 0 = non-blocking, >0 = timeout in milliseconds
    pub func wait(mut this, timeout_ms: I32) -> I32

    /// Get the i-th event from the last wait() call.
    pub func event_at(this, i: I32) -> PollEvent

    /// Destroy the poller and free resources.
    pub func destroy(mut this)
}
```

#### TimerWheel (Hashed 2-level timer wheel)

Efficient O(1) schedule/cancel/fire for thousands of timers. Layout:
- **Level 0**: 64 slots × 1ms granularity = 0-63ms
- **Level 1**: 64 slots × 64ms granularity = 64-4095ms
- **Overflow list**: Timers >4096ms (cascaded on advance)

```tml
pub type TimerWheel {
    handle: I64
}

pub type TimerId {
    value: I64
}

impl TimerWheel {
    /// Create a new timer wheel.
    pub func new(start_ms: I64) -> TimerWheel

    /// Schedule a callback to fire after delay_ms milliseconds.
    /// callback: I64 opaque function pointer, cast to func(I64) at fire time
    /// user_data: I64 opaque value passed to callback
    pub func schedule(mut this, delay_ms: I64, callback: I64, user_data: I64) -> TimerId

    /// Cancel a scheduled timer.
    pub func cancel(mut this, id: TimerId)

    /// Advance the timer wheel to current_ms and fire expired timers.
    /// Returns the number of timers that fired.
    pub func advance(mut this, current_ms: I64) -> I64

    /// Get the deadline (in ms) of the next pending timer, or -1 if none.
    pub func next_deadline_ms(this) -> I64

    /// Get the number of active timers.
    pub func len(this) -> I64

    /// Get the number of active timers (alias for len).
    pub func active_count(this) -> I64

    /// Destroy the timer wheel and free resources.
    pub func destroy(mut this)
}
```

**Example:**
```tml
use std::aio::timer_wheel::TimerWheel

func on_timer(user_data: I64) {
    print("Timer fired with data: {user_data}\n")
}

var tw = TimerWheel::new(0)
let t1 = tw.schedule(100, on_timer as I64, 42)
let t2 = tw.schedule(50, on_timer as I64, 99)

// Advance to 60ms: t2 fires
let fired = tw.advance(60)  // fired == 1

tw.cancel(t1)
tw.destroy()
```

### Layer 3: Event Loop & High-Level APIs

#### EventLoop (Orchestration)

Single-threaded event loop combining Poller + TimerWheel + callback dispatch.

```tml
pub type EventLoop {
    poller: Poller,
    timers: TimerWheel,
    sources: I64,                    // I/O source array (heap-allocated)
    sources_capacity: I64,
    sources_count: I64,
    next_token: U32,
    running: Bool,
    start_time_ms: I64,
}

impl EventLoop {
    /// Create a new event loop.
    pub func new() -> EventLoop

    /// Register a socket for event monitoring. Returns a token.
    pub func register(mut this, socket_handle: I64, interests: U32) -> U32

    /// Update interests for a registered socket.
    pub func modify(mut this, token: U32, interests: U32)

    /// Unregister a socket from the event loop.
    pub func deregister(mut this, token: U32)

    /// Set a readable callback (fires when socket is readable).
    pub func on_readable(mut this, token: U32, callback: I64)

    /// Set a writable callback (fires when socket is writable).
    pub func on_writable(mut this, token: U32, callback: I64)

    /// Set an error callback (fires on error/HUP).
    pub func on_error(mut this, token: U32, callback: I64)

    /// Set user data for a token (passed to all callbacks for that token).
    pub func set_user_data(mut this, token: U32, user_data: I64)

    /// Schedule a timer callback.
    pub func set_timeout(mut this, delay_ms: I64, callback: I64, user_data: I64) -> TimerId

    /// Cancel a timer.
    pub func clear_timer(mut this, id: TimerId)

    /// Run the event loop until stopped or all sources/timers are exhausted.
    pub func run(mut this)

    /// Run one iteration of the event loop (non-blocking or with timeout).
    pub func poll_once(mut this, timeout_ms: I32)

    /// Stop the event loop.
    pub func stop(mut this)

    /// Destroy the event loop and free all resources.
    pub func destroy(mut this)

    // Internal helpers (public for testing)
    pub func find_source_by_token(this, token: U32) -> I64
    pub func grow_sources(mut this)
    pub func get_time_ms(this) -> I64
}
```

**Run loop phases** (main event loop logic):
```
loop (running and (sources_count > 0 or timers.len() > 0)) {
    1. Calculate poll timeout from next timer deadline
    2. Call poller.wait(timeout_ms) — blocks until I/O ready or timeout
    3. Advance timers to current time → fire expired callbacks
    4. For each ready I/O event:
       - Look up IoSource by token
       - Call on_readable/on_writable/on_error callback if set
    5. If no sources and no timers → exit loop
}
```

**Callback signature:**
```tml
func my_callback(user_data: I64) {
    // user_data is opaque I64 set via set_user_data()
}
```

Then cast to and set:
```tml
el.on_readable(token, my_callback as I64)
```

## 3. Module Structure

| File | Lines | Description |
|------|-------|-------------|
| `poller.tml` | 162 | Poller wrapper around platform FFI |
| `timer_wheel.tml` | 439 | Hashed 2-level timer wheel implementation |
| `event_loop.tml` | 377 | Single-threaded event loop orchestration |
| `mod.tml` | 15 | Module exports |
| `poller.test.tml` | 80 | 8 tests: create/destroy, wait, event flags |
| `timer_wheel.test.tml` | 180 | 8 tests: schedule/cancel, pool growth, edge cases |
| `event_loop.test.tml` | 120 | 12 tests: source/timer management, callbacks |

**Total**: ~1,190 lines of TML code + 28 tests (all passing)

## 4. Usage Examples

### 4.1 Basic Timer

```tml
use std::aio::event_loop::EventLoop

func timer_callback(user_data: I64) {
    print("Timer fired: {user_data}\n")
}

func main() -> I32 {
    let mut el = EventLoop::new()

    // Schedule a timer for 100ms
    let t = el.set_timeout(100, timer_callback as I64, 42)

    // Run the event loop
    el.run()

    el.destroy()
    return 0
}
```

### 4.2 I/O with Callbacks

```tml
use std::aio::event_loop::EventLoop
use std::net::TcpListener

func on_readable(user_data: I64) {
    // user_data is the socket handle
    let socket = user_data as I64
    // read from socket...
}

func main() -> I32 {
    let mut el = EventLoop::new()
    let listener = TcpListener::bind("127.0.0.1:8000")!

    // Register listener socket
    let token = el.register(listener.socket_handle(), 1)  // READABLE
    el.on_readable(token, on_readable as I64)
    el.set_user_data(token, listener.socket_handle())

    // Run the event loop
    el.run()

    el.destroy()
    return 0
}
```

### 4.3 poll_once for External Loop Integration

```tml
use std::aio::event_loop::EventLoop

func main() -> I32 {
    let mut el = EventLoop::new()
    // ... register sources ...

    // Integrate with external event loop
    loop (should_continue) {
        el.poll_once(100)  // Wait 100ms for events
        // ... do other work ...
    }

    el.destroy()
    return 0
}
```

## 5. Test Coverage

28 tests across 3 files, all passing:

### poller.test.tml (8 tests)
- `test_poller_create_destroy` — Create/destroy lifecycle
- `test_poller_wait_timeout` — Wait with timeout on empty poller
- `test_poller_wait_nonblocking` — Non-blocking (timeout=0) wait
- `test_poll_event_flags` — Flag operations (is_readable, is_writable, etc.)
- `test_poll_event_flags_combined` — Combined READWRITE flag
- `test_poll_event_flags_error` — ERROR flag handling
- `test_poll_event_flags_hup` — HUP (hang-up) flag handling
- `test_poll_event_token_storage` — Token preservation across events

### timer_wheel.test.tml (8 tests)
- `test_timer_wheel_create_destroy` — Lifecycle
- `test_timer_wheel_schedule_and_advance` — Basic scheduling and firing
- `test_timer_wheel_cancel` — Timer cancellation
- `test_timer_wheel_pool_growth` — 100+ timers with dynamic pool growth
- `test_timer_wheel_l1_timers` — Level 1 timers (64-4095ms range)
- `test_timer_wheel_overflow_timers` — Overflow timers (>4096ms) with cascading
- `test_timer_wheel_next_deadline` — Deadline calculation with cancellation
- `test_timer_wheel_len` — Active timer count tracking

### event_loop.test.tml (12 tests)
- `test_event_loop_create_destroy` — Lifecycle
- `test_event_loop_register_sources` — Source registration and token allocation
- `test_event_loop_timer_scheduling` — set_timeout integration
- `test_event_loop_timer_cancel` — clear_timer integration
- `test_event_loop_get_time_ms` — Monotonic clock
- `test_event_loop_source_lookup` — find_source_by_token helper
- `test_event_loop_grow_sources` — Dynamic source array growth
- `test_event_loop_poll_once_no_events` — Single iteration with timeout
- `test_event_loop_set_user_data` — User data storage and retrieval
- `test_event_loop_on_callbacks` — Callback registration (no-ops on non-existent tokens)
- `test_event_loop_modify_interests` — Interest modification
- `test_event_loop_deregister` — Source deregistration

## 6. Performance Characteristics

| Operation | Time Complexity | Space |
|-----------|-----------------|-------|
| `register()` | O(1) amortized | 48 bytes per source |
| `modify()` | O(n) linear search | 0 (in-place) |
| `deregister()` | O(n) swap-with-last | 0 (shrink) |
| `poller.wait()` | O(ready events) | Kernel |
| `timer.schedule()` | O(1) | 32 bytes per timer |
| `timer.cancel()` | O(1) | 0 (mark as inactive) |
| `timer.advance()` | O(1 + fires) | 0 (in-place cascade) |
| `event_loop.run()` | O(total events) | Stack only |

## 7. Phase 4: High-Level APIs (Pending)

The event loop (Phase 3) is the foundation for Phase 4, which will add:

```tml
pub type TcpServer { ... }       // Listener registration + accept on event loop
pub type TcpClient { ... }       // Stream registration + data/end/error handlers
pub type UdpHandle { ... }       // UDP socket with on_message callback
pub func sleep(ms: I64) { ... }  // Timer-based delay
```

These will provide a more ergonomic API for networked applications, building directly on top of the EventLoop.

## 8. Design Decisions

### Why callbacks, not async/await?
TML doesn't yet have compiler support for async/await syntax. Callbacks (like Node.js) work today with function pointers. Async/await can be added later as syntactic sugar over the same event loop.

### Why single-threaded?
Simpler concurrency model. Multi-threaded work-stealing (like tokio) is a future enhancement.

### Why token-based dispatch?
O(1) callback lookup. Avoids HashMap overhead for hot-path event dispatch.

### Why 2-level timer wheel, not min-heap?
O(1) schedule/cancel, better cache locality for high-frequency timers. Min-heap is O(log n).

### Why minimize C code?
TML is migrating toward pure TML. The 6-function FFI is the minimum needed for OS-level I/O polling.

## 9. Future Enhancements

1. **Phase 4 (Q1 2026)**: High-level async TCP/UDP APIs
2. **Phase 5 (Q2 2026)**: async/await syntax support in compiler
3. **Multi-threaded event loops**: Work-stealing task scheduler
4. **Integration with HTTP server**: WebSocket upgrade patterns with DuplexStream
5. **UDP multicast/broadcast** support in high-level API
