# Async HTTP Runtime Implementation

## Overview

Implement a complete async HTTP stack following Tokio's architecture pattern with:
- Multiplexed I/O, timers, scheduler, thread-pool for blocking tasks
- Futures/async-await model (cooperative polling) instead of callbacks
- OS primitives (epoll/kqueue/IOCP) via mio-like abstraction
- NestJS-style decorator structure for HTTP routing
- Complete HTTP features: headers, event streams, HTTPS, TLS, HTTP/3

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    HTTP Application Layer                    │
│  @Controller, @Get, @Post, @Middleware decorators           │
├─────────────────────────────────────────────────────────────┤
│                      HTTP Protocol Layer                     │
│  HTTP/1.1, HTTP/2, HTTP/3, Headers, EventStream, WebSocket  │
├─────────────────────────────────────────────────────────────┤
│                      TLS/Crypto Layer                        │
│  TLS 1.3, Certificate handling, ALPN negotiation            │
├─────────────────────────────────────────────────────────────┤
│                    Async Runtime Layer                       │
│  Executor, Scheduler, Timer wheel, Thread-pool              │
├─────────────────────────────────────────────────────────────┤
│                      Reactor Layer (mio)                     │
│  Event loop, Interest registration, Wake mechanism          │
├─────────────────────────────────────────────────────────────┤
│                    OS Abstraction Layer                      │
│  epoll (Linux), kqueue (macOS/BSD), IOCP (Windows)          │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1: OS Abstraction Layer (mio-like)

### Task 1.1: Platform-specific event APIs
- [ ] **Linux epoll wrapper**
  - `epoll_create1`, `epoll_ctl`, `epoll_wait`
  - Edge-triggered vs level-triggered modes
  - `EPOLLIN`, `EPOLLOUT`, `EPOLLERR`, `EPOLLHUP`, `EPOLLET`

- [ ] **macOS/BSD kqueue wrapper**
  - `kqueue`, `kevent`, `kevent64`
  - `EVFILT_READ`, `EVFILT_WRITE`, `EV_ADD`, `EV_DELETE`, `EV_CLEAR`

- [ ] **Windows IOCP wrapper**
  - `CreateIoCompletionPort`, `GetQueuedCompletionStatus`
  - `PostQueuedCompletionStatus` for wake-ups
  - Overlapped I/O structures

### Task 1.2: Unified selector abstraction
```tml
pub behavior Selector {
    func register(this, fd: RawFd, token: Token, interests: Interest) -> Outcome[(), IoError]
    func reregister(this, fd: RawFd, token: Token, interests: Interest) -> Outcome[(), IoError]
    func deregister(this, fd: RawFd) -> Outcome[(), IoError]
    func select(this, events: mut ref Events, timeout: Maybe[Duration]) -> Outcome[(), IoError]
}

pub type Interest {
    bits: U8,
}

impl Interest {
    pub const READABLE: Interest = Interest { bits: 0b01 }
    pub const WRITABLE: Interest = Interest { bits: 0b10 }

    pub func readable() -> Interest { Interest::READABLE }
    pub func writable() -> Interest { Interest::WRITABLE }
    pub func both() -> Interest { Interest { bits: 0b11 } }
}

pub type Token {
    value: U64,
}

pub type Events {
    inner: Vec[Event],
    capacity: U32,
}

pub type Event {
    token: Token,
    readiness: Interest,
}
```

### Task 1.3: Non-blocking socket wrapper
```tml
pub type MioTcpListener {
    inner: RawSocket,
}

impl MioTcpListener {
    pub func bind(addr: SocketAddr) -> Outcome[MioTcpListener, IoError]
    pub func accept(this) -> Outcome[(MioTcpStream, SocketAddr), IoError]
    pub func set_nonblocking(mut this, nonblocking: Bool) -> Outcome[(), IoError]
}

pub type MioTcpStream {
    inner: RawSocket,
}

impl MioTcpStream {
    pub func connect(addr: SocketAddr) -> Outcome[MioTcpStream, IoError]
    pub func read(mut this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    pub func write(mut this, buf: ref [U8]) -> Outcome[U64, IoError]
    pub func shutdown(this, how: Shutdown) -> Outcome[(), IoError]
}
```

### Task 1.4: Waker mechanism
```tml
/// Cross-thread wake-up mechanism
pub type Waker {
    #if LINUX
    eventfd: I32,
    #elif MACOS
    pipe: (I32, I32),  // (read_fd, write_fd)
    #elif WINDOWS
    event: Handle,
    #endif
}

impl Waker {
    pub func new(selector: ref Selector, token: Token) -> Outcome[Waker, IoError]
    pub func wake(this) -> Outcome[(), IoError]
}
```

---

## Phase 2: Async Runtime Core

### Task 2.1: Future trait and Poll type
```tml
/// Result of polling a Future
pub type Poll[T] {
    Ready(T),
    Pending,
}

/// Core async abstraction
pub behavior Future {
    type Output

    func poll(mut this, cx: mut ref Context) -> Poll[Self::Output]
}

/// Waker context passed to poll
pub type Context {
    waker: Waker,
}

impl Context {
    pub func waker(this) -> ref Waker { ref this.waker }
}
```

### Task 2.2: Task and spawning
```tml
/// A spawned async task
pub type Task[T] {
    id: TaskId,
    future: Pin[Heap[dyn Future[Output = T]>>,
    state: TaskState,
    waker: TaskWaker,
}

pub type TaskId {
    value: U64,
}

pub type TaskState {
    Idle,
    Scheduled,
    Running,
    Completed,
}

/// Task waker that re-schedules the task
pub type TaskWaker {
    task_id: TaskId,
    scheduler: Sync[Scheduler],
}

impl Wake for TaskWaker {
    func wake(this) {
        this.scheduler.schedule(this.task_id)
    }
}
```

### Task 2.3: Single-threaded executor
```tml
pub type LocalExecutor {
    tasks: Slab[Task[()]],
    ready_queue: VecDeque[TaskId],
    selector: Selector,
    events: Events,
}

impl LocalExecutor {
    pub func new() -> Outcome[LocalExecutor, IoError]

    pub func spawn[F: Future](mut this, future: F) -> JoinHandle[F::Output]

    pub func block_on[F: Future](mut this, future: F) -> F::Output {
        // Pin the future
        let pinned = pin!(future)

        loop {
            // Create waker for main task
            let waker = // ...
            let cx = Context { waker }

            // Poll the future
            when pinned.poll(mut cx) {
                Poll::Ready(result) => return result
                Poll::Pending => {
                    // Wait for events
                    this.selector.select(mut this.events, Nothing)
                    this.process_events()
                }
            }
        }
    }

    func process_events(mut this) {
        for event in this.events.iter() {
            // Wake corresponding task
            if let Just(task) = this.tasks.get(event.token.value) {
                this.ready_queue.push_back(task.id)
            }
        }
    }
}
```

### Task 2.4: Multi-threaded runtime (work-stealing)
```tml
pub type Runtime {
    /// Shared scheduler state
    scheduler: Sync[Scheduler],
    /// Worker threads
    workers: Vec[Worker],
    /// Blocking thread pool
    blocking_pool: ThreadPool,
    /// Global I/O driver
    io_driver: Sync[IoDriver],
    /// Timer wheel
    timer: Sync[TimerWheel],
}

pub type Scheduler {
    /// Global task queue (MPMC)
    global_queue: ConcurrentQueue[TaskId],
    /// Per-worker local queues
    local_queues: Vec[LocalQueue],
    /// Idle workers for stealing
    idle_workers: AtomicBitset,
}

pub type Worker {
    id: WorkerId,
    /// Local run queue (SPSC, can be stolen from)
    local_queue: LocalQueue,
    /// Reference to scheduler
    scheduler: Sync[Scheduler],
    /// Thread handle
    thread: Maybe[JoinHandle[()]>,
}

impl Worker {
    func run(mut this) {
        loop {
            // 1. Try local queue
            if let Just(task) = this.local_queue.pop() {
                this.run_task(task)
                continue
            }

            // 2. Try global queue
            if let Just(task) = this.scheduler.global_queue.pop() {
                this.run_task(task)
                continue
            }

            // 3. Try stealing from other workers
            if let Just(task) = this.steal_from_others() {
                this.run_task(task)
                continue
            }

            // 4. Park and wait for wake-up
            this.park()
        }
    }

    func steal_from_others(this) -> Maybe[TaskId] {
        // Work-stealing algorithm
        let num_workers = this.scheduler.local_queues.len()
        let start = random() % num_workers

        for i in 0 to num_workers {
            let victim = (start + i) % num_workers
            if victim != this.id.value {
                if let Just(task) = this.scheduler.local_queues[victim].steal() {
                    return Just(task)
                }
            }
        }
        Nothing
    }
}
```

### Task 2.5: Timer wheel
```tml
/// Hierarchical timer wheel for efficient timeout handling
pub type TimerWheel {
    /// Wheel levels (e.g., ms, seconds, minutes)
    levels: [TimerLevel; 4],
    /// Current time in ticks
    current_tick: U64,
    /// Tick duration
    tick_duration: Duration,
}

pub type TimerLevel {
    slots: [TimerSlot; 256],
    current_slot: U8,
}

pub type TimerSlot {
    timers: LinkedList[TimerEntry],
}

pub type TimerEntry {
    deadline: Instant,
    waker: Waker,
}

impl TimerWheel {
    pub func insert(mut this, deadline: Instant, waker: Waker) -> TimerHandle
    pub func cancel(mut this, handle: TimerHandle)
    pub func advance(mut this, now: Instant) -> Vec[Waker]
}

/// Sleep future
pub type Sleep {
    deadline: Instant,
    handle: Maybe[TimerHandle],
    registered: Bool,
}

impl Future for Sleep {
    type Output = ()

    func poll(mut this, cx: mut ref Context) -> Poll[()] {
        if Instant::now() >= this.deadline {
            return Poll::Ready(())
        }

        if not this.registered {
            // Register with timer wheel
            let handle = RUNTIME.timer.insert(this.deadline, cx.waker().duplicate())
            this.handle = Just(handle)
            this.registered = true
        }

        Poll::Pending
    }
}

/// Public sleep function
pub async func sleep(duration: Duration) {
    Sleep {
        deadline: Instant::now() + duration,
        handle: Nothing,
        registered: false,
    }.await
}
```

### Task 2.6: Blocking thread pool
```tml
/// Thread pool for blocking operations
pub type ThreadPool {
    workers: Vec[PoolWorker],
    queue: ConcurrentQueue[BlockingTask],
    shutdown: AtomicBool,
}

pub type BlockingTask {
    func_ptr: fn() -> (),
    waker: Waker,
}

impl ThreadPool {
    pub func new(size: U32) -> ThreadPool

    pub func spawn_blocking[F, R](this, f: F) -> JoinHandle[R]
    where F: FnOnce() -> R + Send
    {
        // Wrap in task, add to queue, return handle
    }
}

/// Run blocking code without blocking the async runtime
pub async func spawn_blocking[F, R](f: F) -> R
where F: FnOnce() -> R + Send
{
    RUNTIME.blocking_pool.spawn_blocking(f).await
}
```

---

## Phase 3: Async I/O Primitives

### Task 3.1: AsyncRead and AsyncWrite traits
```tml
pub behavior AsyncRead {
    func poll_read(
        mut this,
        cx: mut ref Context,
        buf: mut ref [U8]
    ) -> Poll[Outcome[U64, IoError]]
}

pub behavior AsyncWrite {
    func poll_write(
        mut this,
        cx: mut ref Context,
        buf: ref [U8]
    ) -> Poll[Outcome[U64, IoError]]

    func poll_flush(
        mut this,
        cx: mut ref Context
    ) -> Poll[Outcome[(), IoError]]

    func poll_shutdown(
        mut this,
        cx: mut ref Context
    ) -> Poll[Outcome[(), IoError]]
}
```

### Task 3.2: Async TCP types
```tml
pub type TcpListener {
    inner: MioTcpListener,
    io: Sync[IoRegistration],
}

impl TcpListener {
    pub async func bind(addr: SocketAddr) -> Outcome[TcpListener, IoError] {
        let listener = MioTcpListener::bind(addr)?
        listener.set_nonblocking(true)?

        let io = RUNTIME.io_driver.register(listener.as_raw_fd())?

        Ok(TcpListener { inner: listener, io })
    }

    pub async func accept(this) -> Outcome[(TcpStream, SocketAddr), IoError] {
        loop {
            when this.inner.accept() {
                Ok((stream, addr)) => {
                    let tcp_stream = TcpStream::from_mio(stream)?
                    return Ok((tcp_stream, addr))
                }
                Err(e) if e.is_would_block() => {
                    // Wait for readable
                    this.io.readable().await?
                }
                Err(e) => return Err(e)
            }
        }
    }
}

pub type TcpStream {
    inner: MioTcpStream,
    io: Sync[IoRegistration],
}

impl TcpStream {
    pub async func connect(addr: SocketAddr) -> Outcome[TcpStream, IoError]

    pub async func read(mut this, buf: mut ref [U8]) -> Outcome[U64, IoError] {
        loop {
            when this.inner.read(buf) {
                Ok(n) => return Ok(n)
                Err(e) if e.is_would_block() => {
                    this.io.readable().await?
                }
                Err(e) => return Err(e)
            }
        }
    }

    pub async func write(mut this, buf: ref [U8]) -> Outcome[U64, IoError] {
        loop {
            when this.inner.write(buf) {
                Ok(n) => return Ok(n)
                Err(e) if e.is_would_block() => {
                    this.io.writable().await?
                }
                Err(e) => return Err(e)
            }
        }
    }

    pub async func write_all(mut this, buf: ref [U8]) -> Outcome[(), IoError] {
        let written: U64 = 0
        while written < buf.len() {
            written = written + this.write(buf[written..]).await?
        }
        Ok(())
    }
}
```

### Task 3.3: Async UDP socket
```tml
pub type UdpSocket {
    inner: MioUdpSocket,
    io: Sync[IoRegistration],
}

impl UdpSocket {
    pub async func bind(addr: SocketAddr) -> Outcome[UdpSocket, IoError]
    pub async func send_to(this, buf: ref [U8], addr: SocketAddr) -> Outcome[U64, IoError]
    pub async func recv_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
}
```

### Task 3.4: Buffered I/O
```tml
pub type BufReader[R: AsyncRead] {
    inner: R,
    buf: Vec[U8],
    pos: U64,
    cap: U64,
}

impl[R: AsyncRead] AsyncRead for BufReader[R] {
    // Buffered read implementation
}

impl[R: AsyncRead] BufReader[R] {
    pub async func read_line(mut this, line: mut ref Str) -> Outcome[U64, IoError]
    pub async func read_until(mut this, delim: U8, buf: mut ref Vec[U8]) -> Outcome[U64, IoError]
}

pub type BufWriter[W: AsyncWrite] {
    inner: W,
    buf: Vec[U8],
}

impl[W: AsyncWrite] AsyncWrite for BufWriter[W] {
    // Buffered write implementation
}
```

---

## Phase 4: TLS/Crypto Layer

### Task 4.1: TLS configuration
```tml
pub type TlsConfig {
    /// Certificate chain (PEM or DER)
    certificates: Vec[Certificate],
    /// Private key
    private_key: PrivateKey,
    /// CA certificates for verification
    root_certs: Vec[Certificate],
    /// ALPN protocols (e.g., ["h2", "http/1.1"])
    alpn_protocols: Vec[Str],
    /// Minimum TLS version
    min_version: TlsVersion,
    /// Maximum TLS version
    max_version: TlsVersion,
}

pub type TlsVersion {
    Tls12,
    Tls13,
}

pub type Certificate {
    der: Vec[U8],
}

pub type PrivateKey {
    der: Vec[U8],
}
```

### Task 4.2: TLS connector and acceptor
```tml
pub type TlsConnector {
    config: TlsConfig,
}

impl TlsConnector {
    pub func new(config: TlsConfig) -> TlsConnector

    pub async func connect[S: AsyncRead + AsyncWrite](
        this,
        domain: Str,
        stream: S
    ) -> Outcome[TlsStream[S], TlsError]
}

pub type TlsAcceptor {
    config: TlsConfig,
}

impl TlsAcceptor {
    pub func new(config: TlsConfig) -> TlsAcceptor

    pub async func accept[S: AsyncRead + AsyncWrite](
        this,
        stream: S
    ) -> Outcome[TlsStream[S], TlsError]
}

pub type TlsStream[S] {
    inner: S,
    tls_state: TlsState,
    /// Negotiated ALPN protocol
    alpn_protocol: Maybe[Str],
}

impl[S: AsyncRead + AsyncWrite] AsyncRead for TlsStream[S] { ... }
impl[S: AsyncRead + AsyncWrite] AsyncWrite for TlsStream[S] { ... }
```

### Task 4.3: Native TLS backend integration
```tml
// Use platform-native TLS:
// - Windows: SChannel
// - macOS: Security.framework
// - Linux: OpenSSL or rustls-like pure implementation

#if WINDOWS
mod tls_schannel { ... }
#elif MACOS
mod tls_security { ... }
#elif LINUX
mod tls_openssl { ... }
#endif
```

---

## Phase 5: HTTP Protocol Layer

### Task 5.1: HTTP types
```tml
pub type Method {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head,
    Options,
    Connect,
    Trace,
}

pub type Version {
    Http10,
    Http11,
    Http2,
    Http3,
}

pub type StatusCode {
    code: U16,
}

impl StatusCode {
    pub const OK: StatusCode = StatusCode { code: 200 }
    pub const CREATED: StatusCode = StatusCode { code: 201 }
    pub const BAD_REQUEST: StatusCode = StatusCode { code: 400 }
    pub const UNAUTHORIZED: StatusCode = StatusCode { code: 401 }
    pub const NOT_FOUND: StatusCode = StatusCode { code: 404 }
    pub const INTERNAL_SERVER_ERROR: StatusCode = StatusCode { code: 500 }

    pub func is_success(this) -> Bool { this.code >= 200 and this.code < 300 }
    pub func is_redirect(this) -> Bool { this.code >= 300 and this.code < 400 }
    pub func is_client_error(this) -> Bool { this.code >= 400 and this.code < 500 }
    pub func is_server_error(this) -> Bool { this.code >= 500 }
}

pub type HeaderName {
    inner: Str,
}

impl HeaderName {
    pub const CONTENT_TYPE: HeaderName = HeaderName { inner: "content-type" }
    pub const CONTENT_LENGTH: HeaderName = HeaderName { inner: "content-length" }
    pub const AUTHORIZATION: HeaderName = HeaderName { inner: "authorization" }
    pub const ACCEPT: HeaderName = HeaderName { inner: "accept" }
    pub const HOST: HeaderName = HeaderName { inner: "host" }
    pub const USER_AGENT: HeaderName = HeaderName { inner: "user-agent" }
    pub const TRANSFER_ENCODING: HeaderName = HeaderName { inner: "transfer-encoding" }
    pub const CONNECTION: HeaderName = HeaderName { inner: "connection" }
}

pub type HeaderValue {
    inner: Vec[U8],
}

pub type HeaderMap {
    entries: Vec[(HeaderName, HeaderValue)],
}

impl HeaderMap {
    pub func new() -> HeaderMap
    pub func insert(mut this, name: HeaderName, value: HeaderValue)
    pub func get(this, name: ref HeaderName) -> Maybe[ref HeaderValue]
    pub func get_all(this, name: ref HeaderName) -> Vec[ref HeaderValue]
    pub func remove(mut this, name: ref HeaderName) -> Maybe[HeaderValue]
    pub func iter(this) -> impl Iterator[Item = (ref HeaderName, ref HeaderValue)]
}
```

### Task 5.2: HTTP Request and Response
```tml
pub type Request[B] {
    method: Method,
    uri: Uri,
    version: Version,
    headers: HeaderMap,
    body: B,
}

impl[B] Request[B] {
    pub func method(this) -> ref Method { ref this.method }
    pub func uri(this) -> ref Uri { ref this.uri }
    pub func headers(this) -> ref HeaderMap { ref this.headers }
    pub func headers_mut(mut this) -> mut ref HeaderMap { mut ref this.headers }
    pub func body(this) -> ref B { ref this.body }
    pub func into_body(this) -> B { this.body }

    pub func map_body[B2, F: FnOnce(B) -> B2](this, f: F) -> Request[B2] {
        Request {
            method: this.method,
            uri: this.uri,
            version: this.version,
            headers: this.headers,
            body: f(this.body),
        }
    }
}

pub type Response[B] {
    status: StatusCode,
    version: Version,
    headers: HeaderMap,
    body: B,
}

impl[B] Response[B] {
    pub func status(this) -> StatusCode { this.status }
    pub func headers(this) -> ref HeaderMap { ref this.headers }
    pub func body(this) -> ref B { ref this.body }
    pub func into_body(this) -> B { this.body }

    pub func map_body[B2, F: FnOnce(B) -> B2](this, f: F) -> Response[B2]
}

/// Builder pattern for responses
pub type ResponseBuilder {
    status: StatusCode,
    headers: HeaderMap,
}

impl ResponseBuilder {
    pub func new() -> ResponseBuilder
    pub func status(mut this, status: StatusCode) -> mut ref ResponseBuilder
    pub func header(mut this, name: HeaderName, value: HeaderValue) -> mut ref ResponseBuilder
    pub func body[B](this, body: B) -> Response[B]
}
```

### Task 5.3: Body types
```tml
/// Empty body
pub type Empty;

/// Full body (buffered)
pub type Full {
    data: Vec[U8],
}

/// Streaming body
pub type StreamBody {
    stream: Pin[Heap[dyn Stream[Item = Outcome[Vec[U8], IoError]]>>,
}

/// Body abstraction
pub behavior Body {
    type Data: Buf
    type Error

    func poll_frame(
        mut this,
        cx: mut ref Context
    ) -> Poll[Maybe[Outcome[Frame[Self::Data], Self::Error]]]

    func size_hint(this) -> SizeHint
}

pub type Frame[T] {
    Data(T),
    Trailers(HeaderMap),
}
```

### Task 5.4: HTTP/1.1 codec
```tml
pub type Http1Codec {
    state: Http1State,
    max_headers: U32,
    max_header_size: U64,
}

pub type Http1State {
    Idle,
    ParsingRequestLine,
    ParsingHeaders,
    ParsingBody(BodyState),
    Done,
}

pub type BodyState {
    ContentLength(U64, U64),  // (total, remaining)
    Chunked(ChunkedState),
    UntilClose,
}

impl Http1Codec {
    pub func decode_request(
        mut this,
        buf: mut ref BytesMut
    ) -> Outcome[Maybe[Request[()>>, Http1Error]

    pub func decode_response(
        mut this,
        buf: mut ref BytesMut
    ) -> Outcome[Maybe[Response[()>>, Http1Error]

    pub func encode_request[B: Body](
        this,
        request: ref Request[B],
        buf: mut ref BytesMut
    ) -> Outcome[(), Http1Error]

    pub func encode_response[B: Body](
        this,
        response: ref Response[B],
        buf: mut ref BytesMut
    ) -> Outcome[(), Http1Error]
}
```

### Task 5.5: HTTP/2 implementation
```tml
pub type Http2Connection[T] {
    inner: T,
    state: Http2State,
    streams: HashMap[StreamId, Http2Stream],
    settings: Http2Settings,
    hpack_encoder: HpackEncoder,
    hpack_decoder: HpackDecoder,
    flow_control: FlowControl,
}

pub type StreamId {
    value: U32,
}

pub type Http2Settings {
    header_table_size: U32,
    enable_push: Bool,
    max_concurrent_streams: U32,
    initial_window_size: U32,
    max_frame_size: U32,
    max_header_list_size: U32,
}

pub type Http2Frame {
    Data(DataFrame),
    Headers(HeadersFrame),
    Priority(PriorityFrame),
    RstStream(RstStreamFrame),
    Settings(SettingsFrame),
    PushPromise(PushPromiseFrame),
    Ping(PingFrame),
    GoAway(GoAwayFrame),
    WindowUpdate(WindowUpdateFrame),
    Continuation(ContinuationFrame),
}

/// HPACK header compression
pub type HpackEncoder { ... }
pub type HpackDecoder { ... }
```

### Task 5.6: HTTP/3 with QUIC
```tml
/// QUIC transport
pub type QuicConnection {
    state: QuicState,
    streams: HashMap[QuicStreamId, QuicStream],
    crypto: QuicCrypto,
}

pub type QuicStreamId {
    value: U64,
}

/// HTTP/3 over QUIC
pub type Http3Connection[T] {
    quic: QuicConnection,
    qpack_encoder: QpackEncoder,
    qpack_decoder: QpackDecoder,
}

pub type Http3Frame {
    Data(Vec[U8]),
    Headers(Vec[U8]),  // QPACK encoded
    CancelPush(U64),
    Settings(Http3Settings),
    PushPromise(U64, Vec[U8]),
    GoAway(U64),
    MaxPushId(U64),
}
```

### Task 5.7: Server-Sent Events (EventStream)
```tml
pub type SseEvent {
    id: Maybe[Str],
    event: Maybe[Str],
    data: Str,
    retry: Maybe[U32],
}

impl SseEvent {
    pub func new(data: Str) -> SseEvent
    pub func id(mut this, id: Str) -> SseEvent
    pub func event(mut this, event: Str) -> SseEvent
    pub func retry(mut this, retry: U32) -> SseEvent
}

pub type SseStream {
    inner: Pin[Heap[dyn Stream[Item = SseEvent]>>,
}

impl SseStream {
    pub func new[S: Stream[Item = SseEvent]](stream: S) -> SseStream
}

/// Convert to HTTP response
impl IntoResponse for SseStream {
    func into_response(this) -> Response[StreamBody] {
        Response::builder()
            .status(StatusCode::OK)
            .header(HeaderName::CONTENT_TYPE, "text/event-stream")
            .header(HeaderName::CACHE_CONTROL, "no-cache")
            .header(HeaderName::CONNECTION, "keep-alive")
            .body(StreamBody::new(this.inner.map(do(event) event.encode())))
    }
}
```

---

## Phase 6: HTTP Server Framework (NestJS-style)

### Task 6.1: Decorator macros
```tml
/// Controller decorator - marks a class as HTTP controller
@Controller("/users")
pub class UserController {
    user_service: UserService,

    @Get("/")
    pub async func list_users(this, req: Request) -> Response {
        let users = this.user_service.get_all().await
        Response::json(users)
    }

    @Get("/:id")
    pub async func get_user(this, @Param("id") id: U64) -> Response {
        when this.user_service.get_by_id(id).await {
            Just(user) => Response::json(user)
            Nothing => Response::not_found()
        }
    }

    @Post("/")
    pub async func create_user(this, @Body user: CreateUserDto) -> Response {
        let created = this.user_service.create(user).await
        Response::created().json(created)
    }

    @Put("/:id")
    pub async func update_user(
        this,
        @Param("id") id: U64,
        @Body update: UpdateUserDto
    ) -> Response {
        this.user_service.update(id, update).await
        Response::ok()
    }

    @Delete("/:id")
    pub async func delete_user(this, @Param("id") id: U64) -> Response {
        this.user_service.delete(id).await
        Response::no_content()
    }
}
```

### Task 6.2: Parameter extraction decorators
```tml
/// Extract from URL path
@Param("name")

/// Extract from query string
@Query("page")

/// Extract from headers
@Header("Authorization")

/// Extract and parse request body
@Body

/// Extract raw request
@Req

/// Inject response builder
@Res
```

### Task 6.3: Middleware system
```tml
pub behavior Middleware {
    async func handle(
        this,
        req: Request,
        next: Next
    ) -> Response
}

pub type Next {
    inner: Heap[dyn FnOnce(Request) -> Pin[Heap[dyn Future[Output = Response]]]>,
}

impl Next {
    pub async func run(this, req: Request) -> Response {
        (this.inner)(req).await
    }
}

/// Example: Logging middleware
pub class LoggingMiddleware;

impl Middleware for LoggingMiddleware {
    async func handle(this, req: Request, next: Next) -> Response {
        let start = Instant::now()
        let method = req.method()
        let path = req.uri().path()

        let response = next.run(req).await

        let duration = start.elapsed()
        log::info("{method} {path} - {response.status()} ({duration:?})")

        response
    }
}

/// Apply middleware via decorator
@UseMiddleware(LoggingMiddleware)
@Controller("/api")
pub class ApiController { ... }
```

### Task 6.4: Guards (authorization)
```tml
pub behavior Guard {
    async func can_activate(this, context: ExecutionContext) -> Bool
}

pub type ExecutionContext {
    request: ref Request,
    handler: ref HandlerInfo,
}

/// JWT authentication guard
pub class JwtAuthGuard {
    jwt_service: JwtService,
}

impl Guard for JwtAuthGuard {
    async func can_activate(this, ctx: ExecutionContext) -> Bool {
        let auth_header = ctx.request.headers().get(HeaderName::AUTHORIZATION)
        when auth_header {
            Just(value) => {
                let token = value.to_str().strip_prefix("Bearer ")
                when token {
                    Just(t) => this.jwt_service.verify(t).await.is_ok()
                    Nothing => false
                }
            }
            Nothing => false
        }
    }
}

/// Apply guard via decorator
@UseGuard(JwtAuthGuard)
@Get("/protected")
pub async func protected_route(this) -> Response { ... }
```

### Task 6.5: Pipes (validation/transformation)
```tml
pub behavior Pipe[T] {
    func transform(this, value: Str) -> Outcome[T, PipeError]
}

/// Parse integer from string
pub class ParseIntPipe;

impl Pipe[I64] for ParseIntPipe {
    func transform(this, value: Str) -> Outcome[I64, PipeError] {
        value.parse[I64]().map_err(do(_) PipeError::validation("Invalid integer"))
    }
}

/// Validate with schema
pub class ValidationPipe[T: Validate] {
    _phantom: PhantomData[T],
}

impl[T: Validate + Deserialize] Pipe[T] for ValidationPipe[T] {
    func transform(this, value: Str) -> Outcome[T, PipeError] {
        let parsed: T = json::from_str(value)?
        parsed.validate()?
        Ok(parsed)
    }
}

/// Use pipe via decorator
@Get("/:id")
pub async func get_item(
    this,
    @Param("id", ParseIntPipe) id: I64
) -> Response { ... }
```

### Task 6.6: Exception filters
```tml
pub behavior ExceptionFilter {
    func catch(this, error: HttpError, context: ExecutionContext) -> Response
}

pub class GlobalExceptionFilter;

impl ExceptionFilter for GlobalExceptionFilter {
    func catch(this, error: HttpError, ctx: ExecutionContext) -> Response {
        let status = error.status()
        let body = json!({
            "statusCode": status.code,
            "message": error.message(),
            "path": ctx.request.uri().path(),
            "timestamp": Instant::now().to_iso8601()
        })

        Response::builder()
            .status(status)
            .header(HeaderName::CONTENT_TYPE, "application/json")
            .body(body.to_string())
    }
}
```

### Task 6.7: Dependency injection
```tml
/// Mark as injectable service
@Injectable
pub class UserService {
    db: DatabaseConnection,
    cache: CacheService,
}

/// Module definition
@Module(
    controllers: [UserController],
    providers: [UserService, CacheService],
    imports: [DatabaseModule],
    exports: [UserService]
)
pub class UserModule;

/// Application bootstrap
pub async func main() {
    let app = NestFactory::create(AppModule).await

    app.use_global_filter(GlobalExceptionFilter)
    app.use_global_middleware(LoggingMiddleware)
    app.enable_cors(CorsOptions::default())

    app.listen(3000).await
}
```

### Task 6.8: WebSocket support
```tml
@WebSocketGateway("/ws")
pub class ChatGateway {
    @OnConnection
    pub async func handle_connection(this, client: WebSocketClient) {
        log::info("Client connected: {client.id}")
    }

    @OnDisconnection
    pub async func handle_disconnect(this, client: WebSocketClient) {
        log::info("Client disconnected: {client.id}")
    }

    @SubscribeMessage("message")
    pub async func handle_message(
        this,
        client: WebSocketClient,
        payload: ChatMessage
    ) -> WsResponse {
        // Broadcast to all clients
        this.server.emit("message", payload)
        WsResponse::ack()
    }
}
```

---

## Phase 7: HTTP Client

### Task 7.1: Client builder
```tml
pub type Client {
    connector: HttpConnector,
    pool: ConnectionPool,
    default_headers: HeaderMap,
    timeout: Duration,
}

impl Client {
    pub func builder() -> ClientBuilder {
        ClientBuilder::new()
    }

    pub async func get(this, url: Str) -> Outcome[Response[Bytes], HttpError] {
        this.request(Method::Get, url).send().await
    }

    pub async func post(this, url: Str) -> RequestBuilder {
        this.request(Method::Post, url)
    }

    pub func request(this, method: Method, url: Str) -> RequestBuilder {
        RequestBuilder::new(this, method, url)
    }
}

pub type ClientBuilder {
    timeout: Duration,
    connect_timeout: Duration,
    pool_idle_timeout: Duration,
    pool_max_idle_per_host: U32,
    default_headers: HeaderMap,
    tls_config: Maybe[TlsConfig],
}

impl ClientBuilder {
    pub func timeout(mut this, timeout: Duration) -> ClientBuilder
    pub func connect_timeout(mut this, timeout: Duration) -> ClientBuilder
    pub func default_headers(mut this, headers: HeaderMap) -> ClientBuilder
    pub func tls_config(mut this, config: TlsConfig) -> ClientBuilder
    pub func build(this) -> Client
}
```

### Task 7.2: Request builder
```tml
pub type RequestBuilder {
    client: ref Client,
    method: Method,
    url: Str,
    headers: HeaderMap,
    body: Maybe[Vec[U8]>,
    timeout: Maybe[Duration],
}

impl RequestBuilder {
    pub func header(mut this, name: HeaderName, value: HeaderValue) -> RequestBuilder
    pub func headers(mut this, headers: HeaderMap) -> RequestBuilder
    pub func body(mut this, body: Vec[U8]) -> RequestBuilder
    pub func json[T: Serialize](mut this, value: T) -> RequestBuilder
    pub func form[T: Serialize](mut this, value: T) -> RequestBuilder
    pub func timeout(mut this, timeout: Duration) -> RequestBuilder

    pub async func send(this) -> Outcome[Response[Bytes], HttpError]
}
```

### Task 7.3: Connection pooling
```tml
pub type ConnectionPool {
    connections: HashMap[PoolKey, Vec[PooledConnection]>,
    max_idle_per_host: U32,
    idle_timeout: Duration,
}

pub type PoolKey {
    scheme: Scheme,
    host: Str,
    port: U16,
}

pub type PooledConnection {
    stream: TcpStream,
    created_at: Instant,
    last_used: Instant,
}

impl ConnectionPool {
    pub async func get_connection(
        mut this,
        key: PoolKey
    ) -> Outcome[PooledConnection, HttpError]

    pub func return_connection(mut this, key: PoolKey, conn: PooledConnection)

    pub func cleanup_idle(mut this)
}
```

---

## Implementation Order

### Milestone 1: Core Async Runtime (Weeks 1-4)
1. OS abstraction layer (epoll/kqueue/IOCP)
2. Basic selector and event loop
3. Future trait and Poll type
4. Single-threaded executor with block_on
5. Timer wheel for sleep/timeout
6. Basic TCP async read/write

### Milestone 2: Multi-threaded Runtime (Weeks 5-8)
1. Work-stealing scheduler
2. Multi-threaded executor
3. Blocking thread pool
4. Channel primitives (mpsc, oneshot)
5. Synchronization (Mutex, RwLock for async)

### Milestone 3: HTTP/1.1 (Weeks 9-12)
1. HTTP types (Method, Status, Headers)
2. Request/Response types
3. HTTP/1.1 parser/codec
4. Basic HTTP server
5. Chunked transfer encoding
6. Keep-alive connections

### Milestone 4: TLS (Weeks 13-16)
1. TLS configuration types
2. Platform TLS backend integration
3. Async TLS handshake
4. ALPN negotiation
5. HTTPS server and client

### Milestone 5: HTTP/2 (Weeks 17-20)
1. HTTP/2 framing
2. HPACK compression
3. Stream multiplexing
4. Flow control
5. Server push

### Milestone 6: Framework (Weeks 21-24)
1. Router with path parameters
2. Decorator system (@Controller, @Get, etc.)
3. Middleware pipeline
4. Guards and pipes
5. Exception filters
6. Dependency injection

### Milestone 7: Advanced Features (Weeks 25-28)
1. Server-Sent Events
2. WebSocket support
3. HTTP client with connection pool
4. HTTP/3 (QUIC-based)

---

## Testing Strategy

### Unit Tests
- Each module has comprehensive unit tests
- Mock I/O for deterministic testing
- Property-based testing for parsers

### Integration Tests
```tml
@test
async func test_http_server() {
    let app = TestApp::new(AppModule).await

    let response = app.get("/users").await
    assert_eq(response.status(), StatusCode::OK)

    let users: Vec[User] = response.json().await
    assert(users.len() > 0)
}

@test
async func test_concurrent_requests() {
    let server = TestServer::start().await

    let futures = (0 to 100).map(do(i) {
        client.get(format!("{server.url}/item/{i}"))
    })

    let results = join_all(futures).await
    assert(results.iter().all(do(r) r.is_ok()))
}
```

### Benchmarks
- Requests per second
- Latency percentiles (p50, p95, p99)
- Memory usage under load
- Connection handling capacity

---

## Dependencies

### Required compiler features
- [x] Basic structs and enums
- [x] Methods and impl blocks
- [x] Generics
- [ ] Associated types in behaviors
- [ ] async/await syntax
- [ ] Pin type
- [ ] Closures with move semantics
- [ ] Decorator/attribute macros

### Runtime requirements
- [ ] Fixed-size array codegen (currently broken)
- [ ] Proper struct extraction from Outcome
- [ ] Platform socket APIs in runtime

---

## Notes

- Start with single-threaded executor before work-stealing
- HTTP/1.1 should be fully working before HTTP/2
- Use existing TLS libraries (OpenSSL bindings) initially
- Decorator macros will need compiler support
- Consider compatibility with existing Rust async ecosystem concepts
