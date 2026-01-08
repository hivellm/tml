# Proposal: Multi-Threaded Async Runtime (Tokio-style)

## Status
- **Created**: 2025-01-08
- **Status**: Draft
- **Priority**: High
- **Dependencies**: `complete-core-library-gaps` task (async primitives)

## Why

TML needs a high-performance multi-threaded async runtime to enable scalable concurrent I/O operations, similar to Rust's Tokio. The current runtime has two separate systems that are not integrated:

1. **thread.c** - OS threads with Go-style blocking channels
2. **async.c** - Single-threaded cooperative async executor

For high-performance servers (HTTP, databases, real-time systems), we need these combined:
- Async I/O to handle thousands of connections efficiently without blocking
- Multiple threads to utilize all CPU cores
- Work-stealing to balance load dynamically across workers
- Integrated I/O polling (epoll/kqueue/IOCP) for non-blocking operations

### Inspiration

- **Tokio (Rust)**: Multi-threaded work-stealing runtime with integrated I/O
- **Go Runtime**: Goroutines with M:N scheduling
- **Node.js libuv**: Event loop with thread pool for blocking operations

### Use Cases

1. **HTTP Servers** - Handle 10K+ concurrent connections
2. **Database Clients** - Async queries without blocking
3. **Real-time Systems** - WebSocket servers, game servers
4. **Microservices** - High-throughput RPC frameworks

## What Changes

### New Files

```
compiler/runtime/
├── mt_runtime.h          # Multi-threaded runtime interface
├── mt_runtime.c          # Runtime core implementation
├── mt_worker.h           # Worker thread interface
├── mt_worker.c           # Worker implementation with local queues
├── mt_queue.h            # Thread-safe queue interface
├── mt_queue.c            # Lock-free MPMC queue implementation
├── io_reactor.h          # I/O reactor abstraction
├── io_reactor.c          # Platform detection and dispatch
├── io_reactor_epoll.c    # Linux epoll implementation
├── io_reactor_iocp.c     # Windows IOCP implementation
├── io_reactor_kqueue.c   # macOS/BSD kqueue implementation
├── timer_wheel.h         # Timer wheel interface
├── timer_wheel.c         # Hierarchical timer wheel
├── async_sync.h          # Async synchronization primitives
└── async_sync.c          # AsyncMutex, AsyncChannel, AsyncSemaphore
```

### Modified Files

- `compiler/runtime/async.h` - Add compatibility with multi-threaded runtime
- `compiler/runtime/async.c` - Refactor to support pluggable executor
- `compiler/src/codegen/llvm_ir_gen.cpp` - Generate runtime initialization
- `lib/std/src/net/` - New async networking types

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      MtRuntime                                   │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Global Queue (MPMC)                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│         ┌────────────────────┼────────────────────┐             │
│         ▼                    ▼                    ▼             │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐       │
│  │  Worker 0   │◄───►│  Worker 1   │◄───►│  Worker N   │       │
│  │ Local Queue │steal│ Local Queue │steal│ Local Queue │       │
│  └─────────────┘     └─────────────┘     └─────────────┘       │
│                              │                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              I/O Reactor (epoll/IOCP/kqueue)            │    │
│  └─────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Timer Wheel                           │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

## Impact

- **Breaking change**: NO (new feature, existing single-threaded mode still works)
- **User benefit**:
  - Handle 10K+ concurrent connections
  - Linear scaling with CPU cores
  - Sub-microsecond task scheduling latency
  - Unified async/threading model

## Success Criteria

1. Tasks can be spawned from any thread and executed on any worker
2. Work-stealing balances load across workers automatically
3. I/O operations don't block worker threads
4. Timers fire within acceptable tolerance (~1ms)
5. Graceful shutdown completes all pending tasks
6. No data races or deadlocks under stress testing
7. Linear scaling up to available CPU core count
8. Memory overhead under 1KB per task
9. Works on Windows (IOCP), Linux (epoll), macOS (kqueue)

## Example Usage (Future TML Syntax)

```tml
use std::net::TcpListener

@runtime("multi", workers: 4)
async func main() {
    let listener = TcpListener::bind("127.0.0.1:8080").await?

    loop {
        let (socket, addr) = listener.accept().await?

        // Spawns task on any available worker thread
        spawn async {
            handle_connection(socket).await
        }
    }
}

async func handle_connection(socket: TcpStream) {
    let buf = [0u8; 1024]
    loop {
        let n = socket.read(buf).await?
        if n == 0 { break }
        socket.write_all(buf[0 to n]).await?
    }
}
```

## References

- Tokio internals: https://tokio.rs/tokio/tutorial
- Work-stealing paper: "Scheduling Multithreaded Computations by Work Stealing"
- IOCP documentation: https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports
- epoll documentation: https://man7.org/linux/man-pages/man7/epoll.7.html
- Timer wheel paper: "Hashed and Hierarchical Timing Wheels"
