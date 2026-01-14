# Specification: Network Standard Library

## Document Info
- **Spec ID**: NET-001
- **Version**: 1.0.0
- **Status**: DRAFT

---

## ADDED: `std::net::socket` Module

### ADDED: `IpAddr` Enum

The `IpAddr` type SHALL represent an IP address (IPv4 or IPv6).

```tml
pub enum IpAddr {
    V4(Ipv4Addr),
    V6(Ipv6Addr),
}
```

#### Scenario: Parse IPv4 Address
- **Given** a string `"192.168.1.1"`
- **When** `IpAddr::parse("192.168.1.1")` is called
- **Then** it SHALL return `Ok(IpAddr::V4(Ipv4Addr::new(192, 168, 1, 1)))`

#### Scenario: Parse IPv6 Address
- **Given** a string `"::1"`
- **When** `IpAddr::parse("::1")` is called
- **Then** it SHALL return `Ok(IpAddr::V6(Ipv6Addr::localhost()))`

---

### ADDED: `SocketAddr` Struct

The `SocketAddr` type SHALL represent a socket address (IP + port).

```tml
pub struct SocketAddr {
    ip: IpAddr,
    port: U16,
}
```

#### Scenario: Create Socket Address
- **Given** an IP `192.168.1.1` and port `8080`
- **When** `SocketAddr::new(ip, 8080)` is called
- **Then** the address SHALL have `port() == 8080`

---

## ADDED: `std::net::tcp` Module

### ADDED: `TcpListener` Struct

The `TcpListener` type SHALL listen for incoming TCP connections.

```tml
pub struct TcpListener {
    // Platform-specific handle
}

impl TcpListener {
    pub func bind(addr: SocketAddr) -> Outcome[TcpListener, NetError];
    pub func accept(self: mut ref Self) -> Outcome[TcpStream, NetError];
    pub func local_addr(self: ref Self) -> SocketAddr;
    pub func incoming(self: mut ref Self) -> TcpIncoming;
}
```

#### Scenario: Bind TCP Listener
- **Given** an available port `8080`
- **When** `TcpListener::bind("127.0.0.1:8080".parse())` is called
- **Then** it SHALL return `Ok(listener)` where `listener.local_addr().port() == 8080`

#### Scenario: Bind to Used Port
- **Given** port `8080` is already in use
- **When** `TcpListener::bind("127.0.0.1:8080".parse())` is called
- **Then** it SHALL return `Err(NetError::AddrInUse)`

#### Scenario: Accept Connection
- **Given** a TcpListener bound to port `8080`
- **And** a client connects to `127.0.0.1:8080`
- **When** `listener.accept()` is called
- **Then** it SHALL return `Ok(stream)` where `stream` is a valid `TcpStream`

---

### ADDED: `TcpStream` Struct

The `TcpStream` type SHALL represent a TCP connection.

```tml
pub struct TcpStream {
    // Platform-specific handle
}

impl TcpStream {
    pub func connect(addr: SocketAddr) -> Outcome[TcpStream, NetError];
    pub func peer_addr(self: ref Self) -> SocketAddr;
    pub func local_addr(self: ref Self) -> SocketAddr;
    pub func shutdown(self: mut ref Self, how: Shutdown) -> Outcome[(), NetError];
    pub func set_nodelay(self: mut ref Self, nodelay: Bool) -> Outcome[(), NetError];
    pub func set_read_timeout(self: mut ref Self, dur: Maybe[Duration]) -> Outcome[(), NetError];
    pub func set_write_timeout(self: mut ref Self, dur: Maybe[Duration]) -> Outcome[(), NetError];
}

impl Read for TcpStream {
    pub func read(self: mut ref Self, buf: mut ref [U8]) -> Outcome[U64, IoError];
}

impl Write for TcpStream {
    pub func write(self: mut ref Self, buf: ref [U8]) -> Outcome[U64, IoError];
    pub func flush(self: mut ref Self) -> Outcome[(), IoError];
}
```

#### Scenario: Connect to Server
- **Given** a TCP server listening on `127.0.0.1:8080`
- **When** `TcpStream::connect("127.0.0.1:8080".parse())` is called
- **Then** it SHALL return `Ok(stream)` with a valid connection

#### Scenario: Connect to Unavailable Server
- **Given** no server is listening on `127.0.0.1:9999`
- **When** `TcpStream::connect("127.0.0.1:9999".parse())` is called
- **Then** it SHALL return `Err(NetError::ConnectionRefused)`

#### Scenario: Read and Write Data
- **Given** an established TCP connection
- **When** `stream.write(b"hello")` is called
- **And** the server echoes the data
- **And** `stream.read(buf)` is called
- **Then** `buf` SHALL contain `"hello"`

---

## ADDED: `std::net::udp` Module

### ADDED: `UdpSocket` Struct

The `UdpSocket` type SHALL represent a UDP socket for datagram communication.

```tml
pub struct UdpSocket {
    // Platform-specific handle
}

impl UdpSocket {
    pub func bind(addr: SocketAddr) -> Outcome[UdpSocket, NetError];
    pub func send_to(self: ref Self, buf: ref [U8], addr: SocketAddr) -> Outcome[U64, NetError];
    pub func recv_from(self: ref Self, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), NetError];
    pub func connect(self: mut ref Self, addr: SocketAddr) -> Outcome[(), NetError];
    pub func send(self: ref Self, buf: ref [U8]) -> Outcome[U64, NetError];
    pub func recv(self: ref Self, buf: mut ref [U8]) -> Outcome[U64, NetError];
    pub func set_broadcast(self: mut ref Self, on: Bool) -> Outcome[(), NetError];
}
```

#### Scenario: Send and Receive Datagram
- **Given** a UDP socket bound to `127.0.0.1:8081`
- **And** another UDP socket bound to `127.0.0.1:8082`
- **When** socket1 calls `send_to(b"ping", "127.0.0.1:8082")`
- **And** socket2 calls `recv_from(buf)`
- **Then** `buf` SHALL contain `"ping"`
- **And** the sender address SHALL be `127.0.0.1:8081`

---

## ADDED: `std::net::http` Module

### ADDED: `Method` Enum

The `Method` type SHALL represent HTTP request methods.

```tml
pub enum Method {
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
```

---

### ADDED: `StatusCode` Struct

The `StatusCode` type SHALL represent HTTP response status codes.

```tml
pub struct StatusCode(U16);

impl StatusCode {
    pub const OK: StatusCode = StatusCode(200);
    pub const CREATED: StatusCode = StatusCode(201);
    pub const BAD_REQUEST: StatusCode = StatusCode(400);
    pub const UNAUTHORIZED: StatusCode = StatusCode(401);
    pub const FORBIDDEN: StatusCode = StatusCode(403);
    pub const NOT_FOUND: StatusCode = StatusCode(404);
    pub const INTERNAL_SERVER_ERROR: StatusCode = StatusCode(500);

    pub func is_success(self: ref Self) -> Bool;
    pub func is_client_error(self: ref Self) -> Bool;
    pub func is_server_error(self: ref Self) -> Bool;
}
```

---

### ADDED: `Request[B]` Struct

The `Request` type SHALL represent an HTTP request.

```tml
pub struct Request[B] {
    method: Method,
    uri: Uri,
    version: Version,
    headers: Headers,
    body: B,
}

impl[B] Request[B] {
    pub func builder() -> RequestBuilder[()];
    pub func method(self: ref Self) -> ref Method;
    pub func uri(self: ref Self) -> ref Uri;
    pub func headers(self: ref Self) -> ref Headers;
    pub func body(self: ref Self) -> ref B;
}
```

#### Scenario: Build GET Request
- **Given** a URI `"/api/users"`
- **When** `Request::builder().method(Method::Get).uri("/api/users").build()` is called
- **Then** the request SHALL have `method() == Method::Get`
- **And** `uri().path() == "/api/users"`

---

### ADDED: `Response[B]` Struct

The `Response` type SHALL represent an HTTP response.

```tml
pub struct Response[B] {
    status: StatusCode,
    version: Version,
    headers: Headers,
    body: B,
}

impl[B] Response[B] {
    pub func builder() -> ResponseBuilder[()];
    pub func ok() -> Response[()];
    pub func not_found() -> Response[()];
    pub func status(self: ref Self) -> StatusCode;
    pub func headers(self: ref Self) -> ref Headers;
    pub func body(self: ref Self) -> ref B;
}
```

---

### ADDED: `HttpClient` Struct

The `HttpClient` type SHALL make HTTP requests with connection pooling.

```tml
pub struct HttpClient {
    // Connection pool, TLS config, timeouts
}

impl HttpClient {
    pub func new() -> HttpClient;
    pub func builder() -> HttpClientBuilder;
    pub func get(self: ref Self, uri: Str) -> Outcome[Response[Vec[U8]], HttpError];
    pub func post(self: ref Self, uri: Str, body: ref [U8]) -> Outcome[Response[Vec[U8]], HttpError];
    pub func send(self: ref Self, req: Request[B]) -> Outcome[Response[Vec[U8]], HttpError];
}
```

#### Scenario: Send GET Request
- **Given** an HTTP server at `http://localhost:8080/health`
- **When** `client.get("http://localhost:8080/health")` is called
- **Then** it SHALL return `Ok(response)` with `response.status() == StatusCode::OK`

---

## ADDED: `std::net::server` Module (Decorator Framework)

### ADDED: `@Controller` Decorator

The `@Controller` decorator SHALL mark a class as an HTTP controller.

```tml
@Controller("/api/users")
pub class UserController {
    // Route handlers
}
```

#### Behavior
- The decorator MUST accept a path prefix string
- All routes in the controller SHALL be prefixed with this path
- The path MUST start with `/`

---

### ADDED: Route Decorators

Route decorators SHALL map HTTP methods to handler functions.

```tml
@Controller("/api/users")
pub class UserController {
    @Get("/")
    pub func list(self: ref Self) -> Response[Json] { ... }

    @Get("/:id")
    pub func get_by_id(self: ref Self, @Param("id") id: I64) -> Response[Json] { ... }

    @Post("/")
    pub func create(self: ref Self, @Body user: UserDto) -> Response[Json] { ... }

    @Put("/:id")
    pub func update(self: ref Self, @Param("id") id: I64, @Body user: UserDto) -> Response[Json] { ... }

    @Delete("/:id")
    pub func delete(self: ref Self, @Param("id") id: I64) -> Response[()> { ... }
}
```

#### Scenario: Route Matching
- **Given** a controller with `@Get("/:id")` at prefix `/api/users`
- **When** a GET request to `/api/users/42` is received
- **Then** the router SHALL call the handler with `id = 42`

---

### ADDED: Parameter Decorators

Parameter decorators SHALL inject request data into handler parameters.

| Decorator | Source | Description |
|-----------|--------|-------------|
| `@Param(name)` | URL path | Route parameter (`:id`) |
| `@Query(name)` | Query string | Query parameter (`?page=1`) |
| `@Body` | Request body | Deserialized body |
| `@Header(name)` | Headers | Specific header value |
| `@Req` | Request | Raw request object |
| `@Res` | Response | Response builder |

#### Scenario: Parameter Injection
- **Given** a handler `func search(@Query("q") query: Str, @Query("page") page: Maybe[I32])`
- **When** a request to `/search?q=hello&page=2` is received
- **Then** `query` SHALL be `"hello"`
- **And** `page` SHALL be `Just(2)`

#### Scenario: Missing Optional Parameter
- **Given** a handler with `@Query("page") page: Maybe[I32]`
- **When** a request without `page` query parameter is received
- **Then** `page` SHALL be `Nothing`

---

### ADDED: `@Middleware` Decorator

The `@Middleware` decorator SHALL mark a class as middleware.

```tml
@Middleware
pub class LoggerMiddleware {
    impl Middleware {
        pub func handle(self: ref Self, req: Request, next: Next) -> Response {
            let start = Instant::now();
            let response = next.run(req);
            let duration = start.elapsed();
            log::info("{} {} - {}ms", req.method(), req.uri(), duration.as_millis());
            return response;
        }
    }
}
```

#### Scenario: Middleware Execution Order
- **Given** global middleware `[A, B]` and controller middleware `[C]`
- **When** a request is processed
- **Then** middleware SHALL execute in order: `A -> B -> C -> Handler -> C -> B -> A`

---

### ADDED: `@Guard` Decorator

The `@Guard` decorator SHALL implement access control.

```tml
@Guard
pub class AuthGuard {
    impl Guard {
        pub func can_activate(self: ref Self, ctx: ExecutionContext) -> Bool {
            let token = ctx.request().headers().get("Authorization");
            return self.auth_service.validate_token(token);
        }
    }
}

@Controller("/api/admin")
@UseGuard(AuthGuard)
pub class AdminController {
    // Protected routes
}
```

#### Scenario: Guard Blocks Unauthorized Request
- **Given** a route protected by `AuthGuard`
- **When** a request without valid token is received
- **Then** the guard SHALL return `false`
- **And** the response SHALL be `403 Forbidden`

---

### ADDED: `Application` Bootstrap

The `Application` type SHALL bootstrap the server framework.

```tml
pub struct Application {
    // DI container, router, middleware stack
}

impl Application {
    pub func create(modules: [Module]) -> Application;
    pub func use_global_middleware[M: Middleware](self: mut ref Self);
    pub func listen(self: mut ref Self, port: U16) -> Outcome[(), NetError];
    pub func shutdown(self: mut ref Self) -> Outcome[(), NetError];
}
```

#### Scenario: Application Startup
- **Given** an application with `UserController`
- **When** `app.listen(8080)` is called
- **Then** the server SHALL accept connections on port `8080`
- **And** routes from `UserController` SHALL be registered

---

## ADDED: Performance Requirements

### Throughput
- HTTP/1.1 server MUST handle >= 100,000 requests/second on a 8-core machine
- Connection establishment MUST complete in < 1ms (local)

### Latency
- p50 latency MUST be < 1ms for simple requests
- p99 latency MUST be < 10ms for simple requests

### Memory
- Idle server MUST use < 10MB memory
- Memory usage MUST remain stable under sustained load (no leaks)
- Each connection SHOULD use < 4KB memory overhead

### Concurrency
- Server MUST handle >= 10,000 concurrent connections
- Server MUST NOT deadlock under any load pattern

---

## ADDED: Error Handling

### `NetError` Enum

All network operations SHALL return `Outcome[T, NetError]` where errors are:

```tml
pub enum NetError {
    /// Connection was refused by remote host
    ConnectionRefused,
    /// Connection was reset by remote host
    ConnectionReset,
    /// Connection timed out
    TimedOut,
    /// Address is already in use
    AddrInUse,
    /// Address is not available
    AddrNotAvailable,
    /// Operation would block (non-blocking mode)
    WouldBlock,
    /// Invalid input provided
    InvalidInput(Str),
    /// DNS resolution failed
    DnsError(Str),
    /// TLS/SSL error
    TlsError(Str),
    /// Platform-specific error
    Os(I32),
}
```

### Error Recovery
- Transient errors (WouldBlock, TimedOut) SHOULD be retryable
- Fatal errors (ConnectionRefused, ConnectionReset) SHOULD close the connection
- Resource errors (AddrInUse) SHOULD be reported immediately

---

## Security Requirements

### TLS
- HTTPS connections MUST use TLS 1.2 or higher
- Server SHOULD support TLS 1.3
- Insecure cipher suites MUST NOT be used

### Input Validation
- All network input MUST be validated before use
- Buffer sizes MUST be bounded
- Timeouts MUST be enforced on all operations

### Resource Limits
- Maximum connections per IP SHOULD be configurable
- Maximum request body size MUST be configurable
- Maximum header size MUST be bounded (default: 8KB)
