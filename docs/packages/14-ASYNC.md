# TML Standard Library: Async Runtime

> `std::async` — Asynchronous programming with futures and async/await.

> **Implementation Status (2026-02-24)**: Basic cooperative multitasking is implemented via C runtime FFI (`Executor`, `Timer`, `yield_now`, `Channel`). Full `async func`/`await` syntax and state machine codegen are **NOT YET IMPLEMENTED**. The API below is the **target spec** — see "Current Implementation" section for what works today.

## Current Implementation

The following primitives are working via `lib/std/src/runtime/` + `compiler/runtime/concurrency/async.c`:

- `Executor` — poll-loop executor with `Executor::run()` and task queue
- `Timer` — polling timer via `GetTickCount64`/`clock_gettime`
- `yield_now` — cooperative yield state machine
- `Channel` — bounded SPSC channel with `send()`/`recv()`/`close()`
- 12 tests across 5 files (executor lifecycle, timer, yield, channel)

**NOT yet implemented**: `async func`, `await` expression, `Future` behavior, `spawn()`, event loop (epoll/IOCP), work-stealing scheduler, `select!`/`join!`.

---

## Overview (Target Spec)

The async package provides an asynchronous runtime for concurrent programming. It includes the Future trait, async/await syntax support, and runtime implementations for executing async tasks.

## Import

```tml
use std::async
use std::async.{spawn, block_on, timeout, sleep}
```

---

## Core Traits

### Future

The fundamental trait for asynchronous computation.

```tml
/// An asynchronous computation that may not have completed yet
pub behavior Future {
    /// The type of value produced on completion
    type Output

    /// Attempt to resolve the future to a value
    func poll(mut this, cx: mut ref Context) -> Poll[This.Output]
}

/// The result of polling a future
pub type Poll[T] = Ready(T) | Pending

extend Poll[T] {
    /// Returns true if the future is ready
    pub func is_ready(this) -> Bool {
        when this {
            Ready(_) -> true,
            Pending -> false,
        }
    }

    /// Returns true if the future is pending
    pub func is_pending(this) -> Bool {
        when this {
            Pending -> true,
            Ready(_) -> false,
        }
    }

    /// Maps the ready value
    pub func map[U](this, f: func(T) -> U) -> Poll[U] {
        when this {
            Ready(t) -> Ready(f(t)),
            Pending -> Pending,
        }
    }
}
```

### Context and Waker

```tml
/// Context for polling a future
pub type Context {
    waker: Waker,
}

extend Context {
    /// Returns a reference to the waker
    pub func waker(this) -> ref Waker {
        return ref this.waker
    }

    /// Creates a new context from a waker
    pub func from_waker(waker: Waker) -> Context {
        return Context { waker: waker }
    }
}

/// Handle to wake up a task
pub type Waker {
    data: *const Unit,
    vtable: &'static WakerVTable,
}

type WakerVTable {
    clone: func(*const Unit) -> Waker,
    wake: func(*const Unit),
    wake_by_ref: func(*const Unit),
    drop: func(*const Unit),
}

extend Waker {
    /// Wakes up the task associated with this waker
    pub func wake(this) {
        (this.vtable.wake)(this.data)
    }

    /// Wakes up without consuming the waker
    pub func wake_by_ref(this) {
        (this.vtable.wake_by_ref)(this.data)
    }
}

extend Waker with Duplicate {
    func clone(this) -> Waker {
        (this.vtable.clone)(this.data)
    }
}

extend Waker with Disposable {
    func drop(mut this) {
        (this.vtable.drop)(this.data)
    }
}
```

---

## Async/Await Syntax

### Async Functions

```tml
/// Async function declaration
pub async func fetch_data(url: String) -> Outcome[Data, Error] {
    let response = http.get(url).await!
    let body = response.body().await!
    return Ok(parse(body)!)
}

/// Async blocks
let future = async {
    let a = compute_a().await
    let b = compute_b().await
    return a + b
}
```

### Await

```tml
/// The .await syntax suspends until the future completes
let result = some_future.await

/// With error propagation
let data = fetch_data(url).await!

/// With inline fallback
let data = fetch_data(url).await else default_data()
```

---

## Runtime

### Runtime Configuration

```tml
/// Async runtime builder
pub type RuntimeBuilder {
    worker_threads: Maybe[U64],
    thread_name: Maybe[String],
    on_thread_start: Maybe[func()],
    on_thread_stop: Maybe[func()],
    enable_io: Bool,
    enable_time: Bool,
}

extend RuntimeBuilder {
    /// Creates a new runtime builder
    pub func new() -> RuntimeBuilder {
        return RuntimeBuilder {
            worker_threads: None,
            thread_name: None,
            on_thread_start: None,
            on_thread_stop: None,
            enable_io: true,
            enable_time: true,
        }
    }

    /// Sets the number of worker threads
    pub func worker_threads(mut this, count: U64) -> RuntimeBuilder {
        this.worker_threads = Just(count)
        return this
    }

    /// Sets the thread name prefix
    pub func thread_name(mut this, name: String) -> RuntimeBuilder {
        this.thread_name = Just(name)
        return this
    }

    /// Sets callback for thread start
    pub func on_thread_start(mut this, f: func()) -> RuntimeBuilder {
        this.on_thread_start = Just(f)
        return this
    }

    /// Builds a multi-threaded runtime
    pub func build(this) -> Outcome[Runtime, RuntimeError] {
        Runtime.new(this)
    }

    /// Builds a single-threaded runtime
    pub func build_current_thread(this) -> Outcome[Runtime, RuntimeError] {
        Runtime.new_current_thread(this)
    }
}

/// The async runtime
pub type Runtime {
    // Internal state
}

extend Runtime {
    /// Creates a new multi-threaded runtime
    pub func new(config: RuntimeBuilder) -> Outcome[Runtime, RuntimeError]

    /// Creates a new single-threaded runtime
    pub func new_current_thread(config: RuntimeBuilder) -> Outcome[Runtime, RuntimeError]

    /// Blocks on a future
    pub func block_on[F: Future](this, future: F) -> F.Output {
        // Enter runtime context and poll until complete
    }

    /// Spawns a task on the runtime
    pub func spawn[F: Future](this, future: F) -> JoinHandle[F.Output]
        where F: Send + 'static, F.Output: Send + 'static

    /// Spawns a blocking task
    pub func spawn_blocking[F](this, f: F) -> JoinHandle[F.Output]
        where F: FnOnce() -> F.Output + Send + 'static, F.Output: Send + 'static

    /// Returns a handle to the runtime
    pub func handle(this) -> RuntimeHandle

    /// Shuts down the runtime
    pub func shutdown_timeout(this, timeout: Duration)

    /// Shuts down the runtime, waiting for all tasks
    pub func shutdown_background(this)
}
```

### Runtime Handle

```tml
/// Handle to the runtime for spawning tasks
pub type RuntimeHandle {
    // Internal
}

extend RuntimeHandle {
    /// Returns the current runtime handle
    pub func current() -> RuntimeHandle

    /// Tries to get the current runtime handle
    pub func try_current() -> Maybe[RuntimeHandle]

    /// Spawns a task
    pub func spawn[F: Future](this, future: F) -> JoinHandle[F.Output]
        where F: Send + 'static, F.Output: Send + 'static

    /// Spawns a blocking task
    pub func spawn_blocking[F](this, f: F) -> JoinHandle[F.Output]
        where F: FnOnce() -> F.Output + Send + 'static, F.Output: Send + 'static

    /// Blocks on a future within the runtime
    pub func block_on[F: Future](this, future: F) -> F.Output
}
```

---

## Task Spawning

### spawn

```tml
/// Spawns a task on the current runtime
pub func spawn[F: Future](future: F) -> JoinHandle[F.Output]
    where F: Send + 'static, F.Output: Send + 'static
{
    RuntimeHandle.current().spawn(future)
}

/// Spawns a task with a name
pub func spawn_named[F: Future](name: String, future: F) -> JoinHandle[F.Output]
    where F: Send + 'static, F.Output: Send + 'static
```

### JoinHandle

```tml
/// Handle to a spawned task
pub type JoinHandle[T] {
    // Internal
}

extend JoinHandle[T] {
    /// Awaits the task completion
    pub async func await(this) -> Outcome[T, JoinError]

    /// Aborts the task
    pub func abort(this)

    /// Returns true if the task is finished
    pub func is_finished(this) -> Bool

    /// Returns the task ID
    pub func id(this) -> TaskId
}

/// Error when joining a task
pub type JoinError = Cancelled | Panicked
```

### spawn_blocking

```tml
/// Spawns a blocking operation on a dedicated thread pool
pub func spawn_blocking[F, R](f: F) -> JoinHandle[R]
    where F: FnOnce() -> R + Send + 'static, R: Send + 'static
{
    RuntimeHandle.current().spawn_blocking(f)
}

// Example:
async func process_file(path: String) -> Outcome[Data, Error] {
    // Run blocking I/O on thread pool
    let content = spawn_blocking(do() {
        std.fs.read_to_string(path)
    }).await!!

    return Ok(parse(content)!)
}
```

### spawn_local

```tml
/// Spawns a !Send task on the local task set
pub func spawn_local[F: Future](future: F) -> JoinHandle[F.Output]
    where F: 'static, F.Output: 'static

/// Local task set for !Send futures
pub type LocalSet {
    // Internal
}

extend LocalSet {
    pub func new() -> LocalSet

    /// Runs the local set until all tasks complete
    pub async func run_until[F: Future](this, future: F) -> F.Output

    /// Spawns a local task
    pub func spawn_local[F: Future](this, future: F) -> JoinHandle[F.Output]
        where F: 'static, F.Output: 'static
}
```

---

## Combinators

### join

```tml
/// Waits for all futures concurrently
pub async func join[A, B](a: A, b: B) -> (A.Output, B.Output)
    where A: Future, B: Future
{
    // Polls both futures until both complete
}

/// Join for tuples
pub async func join3[A, B, C](a: A, b: B, c: C) -> (A.Output, B.Output, C.Output)
    where A: Future, B: Future, C: Future

pub async func join4[A, B, C, D](a: A, b: B, c: C, d: D) -> (A.Output, B.Output, C.Output, D.Output)
    where A: Future, B: Future, C: Future, D: Future

/// Join for a vector of futures
pub async func join_all[F: Future](futures: Vec[F]) -> Vec[F.Output]
```

### select

```tml
/// Waits for the first future to complete
pub async func select[A, B](a: A, b: B) -> Either[A.Output, B.Output]
    where A: Future, B: Future

/// Either type for select results
pub type Either[L, R] = Left(L) | Right(R)

/// Select from multiple futures, returning the first result and remaining futures
pub macro select! {
    // See std.sync for usage
}
```

### try_join

```tml
/// Joins futures that return Results, short-circuiting on first error
pub async func try_join[A, B, E](a: A, b: B) -> Outcome[(A.Output.Ok, B.Output.Ok), E]
    where
        A: Future[Output = Outcome[_, E]],
        B: Future[Output = Outcome[_, E]]

pub async func try_join_all[F, T, E](futures: Vec[F]) -> Outcome[Vec[T], E]
    where F: Future[Output = Outcome[T, E]]
```

### race

```tml
/// Returns the first future to complete, cancelling the other
pub async func race[A, B](a: A, b: B) -> A.Output
    where A: Future, B: Future[Output = A.Output]

/// Race all futures, returning first to complete
pub async func race_all[F: Future](futures: Vec[F]) -> F.Output
```

---

## Time

### sleep

```tml
/// Sleeps for the given duration
pub async func sleep(duration: Duration) {
    Sleep.new(duration).await
}

/// Sleeps until the given instant
pub async func sleep_until(deadline: Instant) {
    Sleep.until(deadline).await
}

/// Sleep future
pub type Sleep {
    deadline: Instant,
}

extend Sleep with Future {
    type Output = Unit

    func poll(mut this, cx: mut ref Context) -> Poll[Unit] {
        if Instant.now() >= this.deadline then {
            return Ready(())
        }
        // Register waker with timer
        RUNTIME.timer().register(this.deadline, cx.waker().clone())
        return Pending
    }
}
```

### timeout

```tml
/// Wraps a future with a timeout
pub async func timeout[F: Future](duration: Duration, future: F) -> Outcome[F.Output, TimeoutError] {
    select! {
        result = future => Ok(result),
        _ = sleep(duration) => Err(TimeoutError),
    }
}

/// Timeout error
pub type TimeoutError

extend TimeoutError {
    pub func elapsed() -> TimeoutError {
        return TimeoutError
    }
}

/// Timeout wrapper type
pub type Timeout[F: Future] {
    future: F,
    deadline: Instant,
}

extend Timeout[F] with Future where F: Future {
    type Output = Outcome[F.Output, TimeoutError]

    func poll(mut this, cx: mut ref Context) -> Poll[Outcome[F.Output, TimeoutError]] {
        // First check if inner future is ready
        when this.future.poll(cx) {
            Ready(value) -> return Ready(Ok(value)),
            Pending -> {},
        }

        // Check timeout
        if Instant.now() >= this.deadline then {
            return Ready(Err(TimeoutError))
        }

        // Register timer
        RUNTIME.timer().register(this.deadline, cx.waker().clone())
        return Pending
    }
}
```

### interval

```tml
/// Creates an interval that yields at a fixed rate
pub func interval(period: Duration) -> Interval {
    Interval.new(period)
}

/// Creates an interval starting at a specific time
pub func interval_at(start: Instant, period: Duration) -> Interval {
    Interval.at(start, period)
}

/// Interval stream
pub type Interval {
    period: Duration,
    next: Instant,
}

extend Interval {
    /// Waits for the next tick
    pub async func tick(mut this) -> Instant {
        sleep_until(this.next).await
        let now = Instant.now()
        this.next = this.next + this.period
        return now
    }

    /// Resets the interval
    pub func reset(mut this) {
        this.next = Instant.now() + this.period
    }
}
```

---

## Async Traits

### AsyncRead

```tml
/// Async reading from a source
pub behavior AsyncRead {
    /// Attempts to read data into buf
    func poll_read(
        mut this,
        cx: mut ref Context,
        buf: mut ref [U8],
    ) -> Poll[Outcome[U64, IoError]]
}

/// Extension methods for AsyncRead
extend AsyncRead {
    /// Reads some bytes
    pub async func read(mut this, buf: mut ref [U8]) -> Outcome[U64, IoError] {
        poll_fn(do(cx) this.poll_read(cx, buf)).await
    }

    /// Reads exactly n bytes
    pub async func read_exact(mut this, buf: mut ref [U8]) -> Outcome[Unit, IoError] {
        var filled: U64 = 0
        loop filled < buf.len() {
            let n = this.read(&mut buf[filled..]).await!
            if n == 0 then {
                return Err(IoError.UnexpectedEof)
            }
            filled = filled + n
        }
        return Ok(())
    }

    /// Reads to end of stream
    pub async func read_to_end(mut this, buf: mut ref Vec[U8]) -> Outcome[U64, IoError] {
        var read: U64 = 0
        var chunk = [0u8; 4096]
        loop {
            let n = this.read(&mut chunk).await!
            if n == 0 then break
            buf.extend_from_slice(&chunk[..n])
            read = read + n
        }
        return Ok(read)
    }

    /// Reads to string
    pub async func read_to_string(mut this, buf: mut ref String) -> Outcome[U64, IoError] {
        var bytes = Vec.new()
        let n = this.read_to_end(&mut bytes).await!
        let s = String.from_utf8(bytes).map_err(|_| IoError.InvalidData)!
        buf.push_str(ref s)
        return Ok(n)
    }
}
```

### AsyncWrite

```tml
/// Async writing to a sink
pub behavior AsyncWrite {
    /// Attempts to write data from buf
    func poll_write(
        mut this,
        cx: mut ref Context,
        buf: ref [U8],
    ) -> Poll[Outcome[U64, IoError]]

    /// Attempts to flush the output
    func poll_flush(
        mut this,
        cx: mut ref Context,
    ) -> Poll[Outcome[Unit, IoError]]

    /// Attempts to close the writer
    func poll_shutdown(
        mut this,
        cx: mut ref Context,
    ) -> Poll[Outcome[Unit, IoError]]
}

/// Extension methods for AsyncWrite
extend AsyncWrite {
    /// Writes some bytes
    pub async func write(mut this, buf: ref [U8]) -> Outcome[U64, IoError] {
        poll_fn(do(cx) this.poll_write(cx, buf)).await
    }

    /// Writes all bytes
    pub async func write_all(mut this, buf: ref [U8]) -> Outcome[Unit, IoError] {
        var written: U64 = 0
        loop written < buf.len() {
            let n = this.write(&buf[written..]).await!
            if n == 0 then {
                return Err(IoError.WriteZero)
            }
            written = written + n
        }
        return Ok(())
    }

    /// Flushes the output
    pub async func flush(mut this) -> Outcome[Unit, IoError] {
        poll_fn(do(cx) this.poll_flush(cx)).await
    }

    /// Shuts down the writer
    pub async func shutdown(mut this) -> Outcome[Unit, IoError] {
        poll_fn(do(cx) this.poll_shutdown(cx)).await
    }
}
```

### AsyncIterator

```tml
/// Async iterator (stream)
pub behavior AsyncIterator {
    type Item

    /// Polls for the next item
    func poll_next(mut this, cx: mut ref Context) -> Poll[Maybe[This.Item]]

    /// Size hint
    func size_hint(this) -> (U64, Maybe[U64]) {
        return (0, Nothing)
    }
}

/// Extension methods for AsyncIterator
extend AsyncIterator {
    /// Gets the next item
    pub async func next(mut this) -> Maybe[This.Item] {
        poll_fn(do(cx) this.poll_next(cx)).await
    }

    /// Collects all items
    pub async func collect[C: FromIterator[This.Item]](mut this) -> C {
        var items = Vec.new()
        loop item in this {
            items.push(item)
        }
        return C.from_iter(items)
    }

    /// Maps each item
    pub func map[B](this, f: func(This.Item) -> B) -> Map[This, B] {
        Map { stream: this, f: f }
    }

    /// Filters items
    pub func filter(this, f: func(ref This.Item) -> Bool) -> Filter[This] {
        Filter { stream: this, predicate: f }
    }

    /// Takes n items
    pub func take(this, n: U64) -> Take[This] {
        Take { stream: this, remaining: n }
    }

    /// Buffers items
    pub func buffered(this, n: U64) -> Buffered[This]
        where This.Item: Future
    {
        Buffered { stream: this, buffer_size: n, pending: Vec.new() }
    }
}
```

---

## Utilities

### poll_fn

```tml
/// Creates a future from a poll function
pub func poll_fn[T, F](f: F) -> PollFn[F]
    where F: FnMut(mut ref Context) -> Poll[T]
{
    return PollFn { f: f }
}

pub type PollFn[F] {
    f: F,
}

extend PollFn[F] with Future
    where F: FnMut(mut ref Context) -> Poll[T]
{
    type Output = T

    func poll(mut this, cx: mut ref Context) -> Poll[T] {
        (this.f)(cx)
    }
}
```

### ready!

```tml
/// Macro to propagate Pending in poll functions
pub macro ready! {
    ($e:expr) => {
        when $e {
            Poll.Ready(value) -> value,
            Poll.Pending -> return Poll.Pending,
        }
    }
}

// Example:
func poll_read(mut this, cx: mut ref Context, buf: mut ref [U8]) -> Poll[Outcome[U64, Error]] {
    let data = ready!(this.inner.poll_read(cx))!
    // ...
}
```

### yield_now

```tml
/// Yields execution to other tasks
pub async func yield_now() {
    YieldNow { yielded: false }.await
}

type YieldNow {
    yielded: Bool,
}

extend YieldNow with Future {
    type Output = Unit

    func poll(mut this, cx: mut ref Context) -> Poll[Unit] {
        if this.yielded then {
            return Ready(())
        }
        this.yielded = true
        cx.waker().wake_by_ref()
        return Pending
    }
}
```

---

## Examples

### Basic Async/Await

```tml
use std::async.{spawn, block_on, sleep}
use std::time.Duration

async func async_main() {
    print("Starting...")

    // Spawn concurrent tasks
    let handle1 = spawn(async {
        sleep(Duration.from_secs(1)).await
        print("Task 1 done")
        return 1
    })

    let handle2 = spawn(async {
        sleep(Duration.from_secs(2)).await
        print("Task 2 done")
        return 2
    })

    // Wait for both
    let (r1, r2) = (handle1.await.unwrap(), handle2.await.unwrap())
    print("Results: " + r1.to_string() + ", " + r2.to_string())
}

func main() {
    let runtime = Runtime.new(RuntimeBuilder.new()).unwrap()
    runtime.block_on(async_main())
}
```

### HTTP Server

```tml
use std::async.{spawn, Runtime}
use std::net.TcpListener
use std::http.{Request, Response}

async func handle_connection(stream: TcpStream) -> Outcome[Unit, Error] {
    let request = Request.parse(mut ref stream).await!

    let response = when request.path() {
        "/" -> Response.ok().body("Hello, World!"),
        "/api" -> Response.ok().json(ref get_data().await!),
        _ -> Response.not_found(),
    }

    response.write_to(mut ref stream).await!
    return Ok(())
}

async func server_main() -> Outcome[Unit, Error] {
    let listener = TcpListener.bind("127.0.0.1:8080").await!
    print("Listening on :8080")

    loop {
        let (stream, addr) = listener.accept().await!
        spawn(async {
            if let Err(e) = handle_connection(stream).await then {
                print("Error handling " + addr.to_string() + ": " + e.to_string())
            }
        })
    }
}
```

### Parallel Processing

```tml
use std::async.{spawn, join_all}

async func process_items(items: Vec[Item]) -> Vec[Result] {
    // Process all items concurrently
    let futures: Vec[_] = items.into_iter()
        .map(do(item) spawn(process_one(item)))
        .collect()

    let results = join_all(futures).await
    return results.into_iter()
        .map(do(r) r.unwrap())
        .collect()
}

async func process_one(item: Item) -> Result {
    // Some async processing
    let data = fetch_data(item.id).await!
    let transformed = transform(data).await!
    return Ok(transformed)
}
```

### Timeout and Retry

```tml
use std::async.{timeout, sleep}
use std::time.Duration

async func fetch_with_retry[T](
    url: String,
    max_retries: U64,
    timeout_duration: Duration,
) -> Outcome[T, Error] {
    var attempts: U64 = 0

    loop attempts < max_retries {
        attempts = attempts + 1

        when timeout(timeout_duration, fetch(url)).await {
            Ok(Ok(data)) -> return Ok(data),
            Ok(Err(e)) -> {
                print("Attempt " + attempts.to_string() + " failed: " + e.to_string())
            },
            Err(_) -> {
                print("Attempt " + attempts.to_string() + " timed out")
            },
        }

        // Exponential backoff
        sleep(Duration.from_millis(100 * (1 << attempts))).await
    }

    return Err(Error.MaxRetriesExceeded)
}
```

---

## See Also

- [std.sync](./13-SYNC.md) — Synchronization primitives
- [std.net](./02-NET.md) — Networking
- [std.http](./07-HTTP.md) — HTTP client/server
- [22-LOW-LEVEL.md](../specs/22-LOW-LEVEL.md) — Threading primitives
