# Tasks: Multi-Threaded Async Runtime

## Progress: 0% (0/72 tasks complete)

## Phase 1: Thread-Safe Task Queue

### 1.1 Lock-Free MPMC Queue
- [ ] 1.1.1 Create `compiler/runtime/mt_queue.h` with MPMC queue interface
- [ ] 1.1.2 Create `compiler/runtime/mt_queue.c` implementation
- [ ] 1.1.3 Implement `MtQueue` struct with atomic head/tail pointers
- [ ] 1.1.4 Implement `mt_queue_create(capacity)` - create bounded queue
- [ ] 1.1.5 Implement `mt_queue_destroy(queue)` - free queue resources
- [ ] 1.1.6 Implement `mt_queue_push(queue, item)` - thread-safe enqueue
- [ ] 1.1.7 Implement `mt_queue_pop(queue)` - thread-safe dequeue (blocking)
- [ ] 1.1.8 Implement `mt_queue_try_pop(queue)` - non-blocking dequeue
- [ ] 1.1.9 Implement `mt_queue_steal(queue)` - steal half items for work-stealing

### 1.2 Per-Worker Local Queues
- [ ] 1.2.1 Implement `LocalQueue` struct for single-producer operations
- [ ] 1.2.2 Implement `local_queue_push(queue, task)` - fast local push (LIFO)
- [ ] 1.2.3 Implement `local_queue_pop(queue)` - LIFO pop for cache locality
- [ ] 1.2.4 Implement `local_queue_steal_batch(queue, dest, count)` - batch steal
- [ ] 1.2.5 Add overflow handling (push to global queue when full)

## Phase 2: Work-Stealing Scheduler

### 2.1 Worker Thread Infrastructure
- [ ] 2.1.1 Create `compiler/runtime/mt_worker.h` header
- [ ] 2.1.2 Create `compiler/runtime/mt_worker.c` implementation
- [ ] 2.1.3 Implement `Worker` struct (thread handle, local queue, state)
- [ ] 2.1.4 Implement `worker_create(runtime, id)` - create worker thread
- [ ] 2.1.5 Implement `worker_run(worker)` - main worker loop
- [ ] 2.1.6 Implement `worker_park(worker)` - sleep when no work
- [ ] 2.1.7 Implement `worker_unpark(worker)` - wake sleeping worker
- [ ] 2.1.8 Implement `worker_shutdown(worker)` - graceful shutdown

### 2.2 Work-Stealing Algorithm
- [ ] 2.2.1 Implement random victim selection for stealing
- [ ] 2.2.2 Implement exponential backoff on failed steals
- [ ] 2.2.3 Implement global queue fallback when all local queues empty
- [ ] 2.2.4 Implement batch stealing (steal half of victim's queue)

## Phase 3: Multi-Threaded Executor

### 3.1 Runtime Core
- [ ] 3.1.1 Create `compiler/runtime/mt_runtime.h` header
- [ ] 3.1.2 Create `compiler/runtime/mt_runtime.c` implementation
- [ ] 3.1.3 Implement `MtRuntime` struct (workers array, global queue, reactor)
- [ ] 3.1.4 Implement `mt_runtime_new(num_workers)` - create runtime
- [ ] 3.1.5 Implement `mt_runtime_new_current_thread()` - single-thread mode
- [ ] 3.1.6 Implement `mt_runtime_destroy(runtime)` - cleanup all resources
- [ ] 3.1.7 Implement `mt_runtime_block_on(runtime, task)` - run until complete
- [ ] 3.1.8 Implement `mt_runtime_spawn(runtime, task)` - spawn background task
- [ ] 3.1.9 Implement `mt_runtime_shutdown(runtime)` - graceful shutdown

### 3.2 Thread-Safe Waker
- [ ] 3.2.1 Implement atomic waker that works across threads
- [ ] 3.2.2 Implement wake-by-ref (avoid cloning waker)
- [ ] 3.2.3 Implement cross-thread wake notification

## Phase 4: I/O Polling Integration

### 4.1 I/O Reactor Abstraction
- [ ] 4.1.1 Create `compiler/runtime/io_reactor.h` header
- [ ] 4.1.2 Create `compiler/runtime/io_reactor.c` implementation
- [ ] 4.1.3 Implement `IoReactor` struct (platform-specific handle)
- [ ] 4.1.4 Implement `io_reactor_new()` - create reactor
- [ ] 4.1.5 Implement `io_reactor_destroy(reactor)` - cleanup
- [ ] 4.1.6 Implement `io_reactor_poll(reactor, timeout)` - wait for events
- [ ] 4.1.7 Implement `io_reactor_wake(reactor)` - interrupt poll

### 4.2 Linux epoll Implementation
- [ ] 4.2.1 Create `compiler/runtime/io_reactor_epoll.c`
- [ ] 4.2.2 Implement epoll_create, epoll_ctl, epoll_wait wrappers
- [ ] 4.2.3 Implement edge-triggered mode for performance
- [ ] 4.2.4 Handle EAGAIN/EWOULDBLOCK properly

### 4.3 Windows IOCP Implementation
- [ ] 4.3.1 Create `compiler/runtime/io_reactor_iocp.c`
- [ ] 4.3.2 Implement completion port creation and management
- [ ] 4.3.3 Implement overlapped I/O submission
- [ ] 4.3.4 Implement GetQueuedCompletionStatus processing

### 4.4 macOS/BSD kqueue Implementation
- [ ] 4.4.1 Create `compiler/runtime/io_reactor_kqueue.c`
- [ ] 4.4.2 Implement kqueue, kevent wrappers
- [ ] 4.4.3 Handle EVFILT_READ, EVFILT_WRITE filters

### 4.5 Async TCP Primitives
- [ ] 4.5.1 Implement `AsyncTcpListener` struct
- [ ] 4.5.2 Implement `async_tcp_bind(addr)` - create listener
- [ ] 4.5.3 Implement `async_tcp_accept(listener)` - async accept
- [ ] 4.5.4 Implement `AsyncTcpStream` struct
- [ ] 4.5.5 Implement `async_tcp_connect(addr)` - async connect
- [ ] 4.5.6 Implement `async_read(stream, buf)` - async read
- [ ] 4.5.7 Implement `async_write(stream, buf)` - async write

## Phase 5: Timer Wheel

### 5.1 Hierarchical Timer Wheel
- [ ] 5.1.1 Create `compiler/runtime/timer_wheel.h` header
- [ ] 5.1.2 Create `compiler/runtime/timer_wheel.c` implementation
- [ ] 5.1.3 Implement `TimerWheel` struct (multiple granularity levels)
- [ ] 5.1.4 Implement `timer_wheel_new()` - create timer wheel
- [ ] 5.1.5 Implement `timer_wheel_insert(wheel, deadline, waker)` - add timer
- [ ] 5.1.6 Implement `timer_wheel_cancel(wheel, timer_id)` - cancel timer
- [ ] 5.1.7 Implement `timer_wheel_poll(wheel)` - fire expired timers
- [ ] 5.1.8 Integrate timer wheel with I/O reactor poll timeout

## Phase 6: Async Synchronization Primitives

### 6.1 Async Mutex
- [ ] 6.1.1 Create `compiler/runtime/async_sync.h` header
- [ ] 6.1.2 Create `compiler/runtime/async_sync.c` implementation
- [ ] 6.1.3 Implement `AsyncMutex` struct with waiter queue
- [ ] 6.1.4 Implement `async_mutex_lock(mutex)` - async lock acquisition
- [ ] 6.1.5 Implement `async_mutex_unlock(guard)` - release lock

### 6.2 Async Channel (MPMC)
- [ ] 6.2.1 Implement `AsyncChannel` struct (bounded MPMC)
- [ ] 6.2.2 Implement `async_channel_new(capacity)` - create channel
- [ ] 6.2.3 Implement `async_channel_send(ch, value)` - async send
- [ ] 6.2.4 Implement `async_channel_recv(ch)` - async receive
- [ ] 6.2.5 Implement `async_channel_close(ch)` - close channel

## Phase 7: Testing & Validation

### 7.1 Unit Tests
- [ ] 7.1.1 Test MPMC queue under high contention
- [ ] 7.1.2 Test work-stealing correctness
- [ ] 7.1.3 Test I/O reactor with concurrent connections
- [ ] 7.1.4 Test timer accuracy under load
- [ ] 7.1.5 Test graceful shutdown with pending tasks

### 7.2 Integration Tests
- [ ] 7.2.1 Echo server handling 1K concurrent connections
- [ ] 7.2.2 Echo server handling 10K concurrent connections
- [ ] 7.2.3 Compare throughput with single-threaded executor

### 7.3 Stress Tests
- [ ] 7.3.1 Spawn 1 million tasks
- [ ] 7.3.2 High-frequency timer creation/cancellation
- [ ] 7.3.3 Rapid connect/disconnect cycles

## Validation Checklist

- [ ] V.1 Tasks can be spawned from any thread
- [ ] V.2 Work-stealing balances load across workers
- [ ] V.3 I/O operations don't block worker threads
- [ ] V.4 Timers fire within ~1ms tolerance
- [ ] V.5 Graceful shutdown completes pending tasks
- [ ] V.6 No data races under ThreadSanitizer
- [ ] V.7 No deadlocks under stress testing
- [ ] V.8 Linear scaling up to CPU core count
- [ ] V.9 Works on Linux (epoll)
- [ ] V.10 Works on Windows (IOCP)
- [ ] V.11 Works on macOS (kqueue)
