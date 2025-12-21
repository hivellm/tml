# std.http — HTTP Client and Server

## 1. Overview

The `std.http` package provides HTTP/1.1 and HTTP/2 client and server functionality.

```tml
import std.http
import std.http.{Client, Request, Response, Server}
```

## 2. Capabilities

```tml
caps: [io.network.http]           // Full HTTP access
caps: [io.network.http.client]    // Client only
caps: [io.network.http.server]    // Server only
```

## 3. HTTP Client

### 3.1 Client

```tml
public type Client {
    config: ClientConfig,
    pool: ConnectionPool,
}

extend Client {
    /// Create with default configuration
    public func new() -> This

    /// Create with custom configuration
    public func with_config(config: ClientConfig) -> This

    /// Execute request
    public func execute(this, request: Request) -> Outcome[Response, HttpError]
    effects: [io.network.http]

    /// GET request
    public func get(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io.network.http]
    {
        return this.execute(Request.get(url)!)
    }

    /// POST request
    public func post(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io.network.http]
    {
        return this.execute(Request.post(url)!.body(body))
    }

    /// PUT request
    public func put(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io.network.http]

    /// DELETE request
    public func delete(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io.network.http]

    /// PATCH request
    public func patch(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io.network.http]

    /// HEAD request
    public func head(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io.network.http]
}
```

### 3.2 ClientConfig

```tml
public type ClientConfig {
    timeout: Maybe[Duration],
    connect_timeout: Maybe[Duration],
    read_timeout: Maybe[Duration],
    write_timeout: Maybe[Duration],
    max_redirects: U32,
    follow_redirects: Bool,
    user_agent: Maybe[String],
    default_headers: HeaderMap,
    proxy: Maybe[Proxy],
    tls_config: Maybe[TlsConfig],
    http2_only: Bool,
    max_connections_per_host: U32,
    connection_idle_timeout: Duration,
}

extend ClientConfig {
    public func new() -> This {
        return This {
            timeout: Just(Duration.from_secs(30)),
            connect_timeout: Just(Duration.from_secs(10)),
            read_timeout: None,
            write_timeout: None,
            max_redirects: 10,
            follow_redirects: true,
            user_agent: Just("TML-HTTP/1.0"),
            default_headers: HeaderMap.new(),
            proxy: None,
            tls_config: None,
            http2_only: false,
            max_connections_per_host: 10,
            connection_idle_timeout: Duration.from_secs(90),
        }
    }

    public func timeout(this, dur: Duration) -> This
    public func connect_timeout(this, dur: Duration) -> This
    public func no_redirects(this) -> This
    public func max_redirects(this, n: U32) -> This
    public func user_agent(this, ua: ref str) -> This
    public func default_header(this, name: ref str, value: ref str) -> This
    public func proxy(this, proxy: Proxy) -> This
    public func https_only(this) -> This
    public func http2_only(this) -> This
}
```

### 3.3 RequestBuilder

```tml
public type RequestBuilder {
    method: Method,
    url: Url,
    headers: HeaderMap,
    body: Maybe[Body],
    timeout: Maybe[Duration>,
}

extend RequestBuilder {
    public func new(method: Method, url: ref str) -> Outcome[This, UrlError]

    /// Add header
    public func header(this, name: ref str, value: ref str) -> This

    /// Add multiple headers
    public func headers(this, headers: HeaderMap) -> This

    /// Set body
    public func body(this, body: impl Into[Body]) -> This

    /// Set JSON body
    public func json[T: Serialize](this, value: ref T) -> Outcome[This, JsonError]

    /// Set form body
    public func form[T: Serialize](this, value: ref T) -> Outcome[This, FormError]

    /// Set request timeout
    public func timeout(this, dur: Duration) -> This

    /// Set bearer token
    public func bearer_auth(this, token: ref str) -> This

    /// Set basic auth
    public func basic_auth(this, username: ref str, password: Maybe[ref str]) -> This

    /// Build request
    public func build(this) -> Request
}

extend Request {
    public func get(url: ref str) -> Outcome[RequestBuilder, UrlError]
    public func post(url: ref str) -> Outcome[RequestBuilder, UrlError]
    public func put(url: ref str) -> Outcome[RequestBuilder, UrlError]
    public func delete(url: ref str) -> Outcome[RequestBuilder, UrlError]
    public func patch(url: ref str) -> Outcome[RequestBuilder, UrlError]
    public func head(url: ref str) -> Outcome[RequestBuilder, UrlError]
}
```

## 4. Request and Response

### 4.1 Request

```tml
public type Request {
    method: Method,
    url: Url,
    headers: HeaderMap,
    body: Maybe[Body],
}

extend Request {
    public func method(this) -> ref Method
    public func url(this) -> ref Url
    public func headers(this) -> ref HeaderMap
    public func body(this) -> Maybe[ref Body]
    public func body_mut(this) -> Maybe[mut ref Body]
}

public type Method = Get | Post | Put | Delete | Patch | Head | Options | Connect | Trace

extend Method {
    public func as_str(this) -> ref str
    public func from_str(s: ref str) -> Outcome[This, InvalidMethod]
    public func is_safe(this) -> Bool
    public func is_idempotent(this) -> Bool
}
```

### 4.2 Response

```tml
public type Response {
    status: StatusCode,
    headers: HeaderMap,
    body: Body,
    version: Version,
}

extend Response {
    /// Get status code
    public func status(this) -> StatusCode

    /// Check if successful (2xx)
    public func is_success(this) -> Bool {
        return this.status.is_success()
    }

    /// Check if redirect (3xx)
    public func is_redirect(this) -> Bool {
        return this.status.is_redirect()
    }

    /// Check if client error (4xx)
    public func is_client_error(this) -> Bool {
        return this.status.is_client_error()
    }

    /// Check if server error (5xx)
    public func is_server_error(this) -> Bool {
        return this.status.is_server_error()
    }

    /// Get headers
    public func headers(this) -> ref HeaderMap

    /// Get header value
    public func header(this, name: ref str) -> Maybe[ref str] {
        return this.headers.get(name)
    }

    /// Get content type
    public func content_type(this) -> Maybe[Mime>

    /// Get content length
    public func content_length(this) -> Maybe[U64]

    /// Read body as bytes
    public func bytes(this) -> Outcome[List[U8], HttpError]
    effects: [io.network.http]

    /// Read body as string
    public func text(this) -> Outcome[String, HttpError]
    effects: [io.network.http]

    /// Parse body as JSON
    public func json[T: Deserialize](this) -> Outcome[T, HttpError]
    effects: [io.network.http]

    /// Get body as reader
    public func body(this) -> ref Body

    /// Get mutable body
    public func body_mut(this) -> mut ref Body
}
```

### 4.3 StatusCode

```tml
public type StatusCode {
    code: U16,
}

extend StatusCode {
    // Common status codes
    public const OK: This = This { code: 200 }
    public const CREATED: This = This { code: 201 }
    public const ACCEPTED: This = This { code: 202 }
    public const NO_CONTENT: This = This { code: 204 }
    public const MOVED_PERMANENTLY: This = This { code: 301 }
    public const FOUND: This = This { code: 302 }
    public const NOT_MODIFIED: This = This { code: 304 }
    public const BAD_REQUEST: This = This { code: 400 }
    public const UNAUTHORIZED: This = This { code: 401 }
    public const FORBIDDEN: This = This { code: 403 }
    public const NOT_FOUND: This = This { code: 404 }
    public const METHOD_NOT_ALLOWED: This = This { code: 405 }
    public const CONFLICT: This = This { code: 409 }
    public const GONE: This = This { code: 410 }
    public const UNPROCESSABLE_ENTITY: This = This { code: 422 }
    public const TOO_MANY_REQUESTS: This = This { code: 429 }
    public const INTERNAL_SERVER_ERROR: This = This { code: 500 }
    public const BAD_GATEWAY: This = This { code: 502 }
    public const SERVICE_UNAVAILABLE: This = This { code: 503 }
    public const GATEWAY_TIMEOUT: This = This { code: 504 }

    public func from_u16(code: U16) -> Maybe[This]
    public func as_u16(this) -> U16
    public func reason_phrase(this) -> ref str
    public func is_informational(this) -> Bool { this.code >= 100 and this.code < 200 }
    public func is_success(this) -> Bool { this.code >= 200 and this.code < 300 }
    public func is_redirect(this) -> Bool { this.code >= 300 and this.code < 400 }
    public func is_client_error(this) -> Bool { this.code >= 400 and this.code < 500 }
    public func is_server_error(this) -> Bool { this.code >= 500 and this.code < 600 }
}
```

### 4.4 Headers

```tml
public type HeaderMap {
    inner: Map[HeaderName, List[HeaderValue]],
}

extend HeaderMap {
    public func new() -> This

    /// Insert header (replaces existing)
    public func insert(this, name: impl Into[HeaderName], value: impl Into[HeaderValue])

    /// Append header (adds to existing)
    public func append(this, name: impl Into[HeaderName], value: impl Into[HeaderValue])

    /// Get first value for header
    public func get(this, name: ref str) -> Maybe[ref str]

    /// Get all values for header
    public func get_all(this, name: ref str) -> ref [HeaderValue]

    /// Check if header exists
    public func contains(this, name: ref str) -> Bool

    /// Remove header
    public func remove(this, name: ref str) -> Maybe[HeaderValue]

    /// Iterate over all headers
    public func iter(this) -> HeaderIter
}

// Common headers
public const CONTENT_TYPE: ref str = "Content-Type"
public const CONTENT_LENGTH: ref str = "Content-Length"
public const AUTHORIZATION: ref str = "Authorization"
public const ACCEPT: ref str = "Accept"
public const USER_AGENT: ref str = "User-Agent"
public const HOST: ref str = "Host"
public const COOKIE: ref str = "Cookie"
public const SET_COOKIE: ref str = "Set-Cookie"
public const LOCATION: ref str = "Location"
public const CACHE_CONTROL: ref str = "Cache-Control"
```

### 4.5 Body

```tml
public type Body {
    kind: BodyKind,
}

type BodyKind =
    | Empty
    | Bytes(List[U8])
    | Stream(Heap[dyn Read>)

extend Body {
    public func empty() -> This
    public func from_bytes(bytes: impl Into[List[U8]>) -> This
    public func from_string(s: impl Into[String>) -> This
    public func from_reader(reader: impl Read + 'static) -> This

    /// Check if body is empty
    public func is_empty(this) -> Bool

    /// Get known length (if available)
    public func len(this) -> Maybe[U64]
}

// Conversions
extend ref str with Into[Body] { ... }
extend String with Into[Body] { ... }
extend ref [U8] with Into[Body] { ... }
extend List[U8] with Into[Body] { ... }
```

## 5. URL Handling

```tml
public type Url {
    scheme: String,
    host: String,
    port: Maybe[U16],
    path: String,
    query: Maybe[String],
    fragment: Maybe[String>,
    username: Maybe[String>,
    password: Maybe[String>,
}

extend Url {
    public func parse(s: ref str) -> Outcome[This, UrlError]

    public func scheme(this) -> ref str
    public func host(this) -> ref str
    public func port(this) -> Maybe[U16]
    public func port_or_default(this) -> U16
    public func path(this) -> ref str
    public func query(this) -> Maybe[ref str>
    public func query_pairs(this) -> QueryPairs
    public func fragment(this) -> Maybe[ref str]

    public func set_path(this, path: ref str)
    public func set_query(this, query: Maybe[ref str])
    public func set_fragment(this, fragment: Maybe[ref str>)

    public func join(this, path: ref str) -> Outcome[Url, UrlError]

    public func to_string(this) -> String
}

public type QueryPairs { ... }
extend QueryPairs with Iterator { type Item = (String, String) }
```

## 6. HTTP Server

### 6.1 Server

```tml
public type Server {
    config: ServerConfig,
}

extend Server {
    /// Create server bound to address
    public func bind(addr: impl ToSocketAddrs) -> Outcome[ServerBuilder, HttpError]
    effects: [io.network.http.server]

    /// Create HTTPS server
    public func bind_tls(
        addr: impl ToSocketAddrs,
        cert: Certificate,
        key: PrivateKey
    ) -> Outcome[ServerBuilder, HttpError]
    effects: [io.network.http.server]
}

public type ServerBuilder {
    addr: SocketAddr,
    tls: Maybe[TlsAcceptor],
    config: ServerConfig,
}

extend ServerBuilder {
    /// Set request handler
    public func serve[H: Handler](this, handler: H) -> Outcome[Unit, HttpError]
    effects: [io.network.http.server]

    /// Set router
    public func router(this, router: Router) -> Outcome[Unit, HttpError>
    effects: [io.network.http.server]

    /// Set max connections
    public func max_connections(this, n: U32) -> This

    /// Set keep-alive timeout
    public func keep_alive(this, dur: Duration) -> This

    /// Disable keep-alive
    public func no_keep_alive(this) -> This

    /// Enable HTTP/2
    public func http2(this) -> This
}
```

### 6.2 Handler Trait

```tml
public behaviorHandler {
    func handle(this, request: Request) -> Response
    effects: [io.network.http.server]
}

// Function as handler
extend func(Request) -> Response with Handler {
    func handle(this, request: Request) -> Response {
        return this(request)
    }
}

// Async handler
public behaviorAsyncHandler {
    async func handle(this, request: Request) -> Response
    effects: [io.network.http.server]
}
```

### 6.3 Router

```tml
public type Router {
    routes: List[Route],
    not_found: Maybe[Heap[dyn Handler]>,
}

extend Router {
    public func new() -> This

    /// Add route
    public func route(this, method: Method, path: ref str, handler: impl Handler + 'static) -> This

    /// GET route
    public func get(this, path: ref str, handler: impl Handler + 'static) -> This

    /// POST route
    public func post(this, path: ref str, handler: impl Handler + 'static) -> This

    /// PUT route
    public func put(this, path: ref str, handler: impl Handler + 'static) -> This

    /// DELETE route
    public func delete(this, path: ref str, handler: impl Handler + 'static) -> This

    /// Nested router
    public func nest(this, prefix: ref str, router: Router) -> This

    /// Set 404 handler
    public func not_found(this, handler: impl Handler + 'static) -> This

    /// Add middleware
    public func middleware(this, mw: impl Middleware + 'static) -> This
}

public type Route {
    method: Method,
    pattern: PathPattern,
    handler: Heap[dyn Handler],
}
```

### 6.4 Path Parameters

```tml
public type PathParams {
    params: Map[String, String],
}

extend PathParams {
    /// Get parameter by name
    public func get(this, name: ref str) -> Maybe[ref str]

    /// Get parameter, parsing as type
    public func parse[T: FromStr](this, name: ref str) -> Maybe[Outcome[T, T.Err]]
}

// Path patterns:
// "/users/:id"           - named parameter
// "/files/*path"         - wildcard (captures rest)
// "/api/v{version:\\d+}" - with regex constraint
```

### 6.5 Middleware

```tml
public behaviorMiddleware {
    func call(this, request: Request, next: Next) -> Response
    effects: [io.network.http.server]
}

public type Next {
    handler: Heap[dyn Handler],
}

extend Next {
    public func call(this, request: Request) -> Response
}

// Common middleware
module middleware

/// Logging middleware
public type Logger { level: LogLevel }

/// CORS middleware
public type Cors {
    allow_origins: List[String],
    allow_methods: List[Method],
    allow_headers: List[String],
    max_age: Maybe[Duration],
}

/// Compression middleware
public type Compression { level: CompressionLevel }

/// Rate limiting middleware
public type RateLimit {
    requests_per_second: U32,
    burst: U32,
}
```

## 7. Multipart Form Data

```tml
module multipart

public type Multipart {
    parts: List[Part],
    boundary: String,
}

public type Part {
    name: String,
    filename: Maybe[String],
    content_type: Maybe[Mime],
    data: List[U8],
}

extend Multipart {
    /// Create new multipart form
    public func new() -> This

    /// Add text field
    public func text(this, name: ref str, value: ref str) -> This

    /// Add file
    public func file(this, name: ref str, filename: ref str, data: ref [U8], content_type: Mime) -> This

    /// Parse from request
    public func from_request(request: ref Request) -> Outcome[This, MultipartError]
    effects: [io.network.http]
}

extend Part {
    /// Get data as string
    public func text(this) -> Outcome[String, Utf8Error]

    /// Get data as bytes
    public func bytes(this) -> ref [U8]

    /// Save to file
    public func save(this, path: ref Path) -> Outcome[Unit, IoError]
    effects: [io.file.write]
}
```

## 8. Examples

### 8.1 Simple HTTP Client

```tml
module http_example
caps: [io.network.http]

import std.http.{Client, Request}

public func main() -> Outcome[Unit, Error] {
    let client = Client.new()

    // Simple GET
    let response = client.get("https://api.example.com/users")!
    println("Status: " + response.status().to_string())
    println("Body: " + response.text()!)

    // POST with JSON
    let user = User { name: "Alice", email: "alice@example.com" }
    let response = client.execute(
        Request.post("https://api.example.com/users")!
            .json(ref user)!
            .header("Authorization", "Bearer token123")
            .build()
    )!

    if response.is_success() {
        let created: User = response.json()!
        println("Created user: " + created.name)
    }

    return Success(unit)
}
```

### 8.2 REST API Server

```tml
module api_server
caps: [io.network.http.server]

import std.http.{Server, Router, Request, Response, StatusCode}
import std.http.middleware.{Logger, Cors}

public func main() -> Outcome[Unit, Error] {
    let router = Router.new()
        .middleware(Logger.new())
        .middleware(Cors.allow_all())
        .get("/", home)
        .get("/users", list_users)
        .get("/users/:id", get_user)
        .post("/users", create_user)
        .delete("/users/:id", delete_user)
        .not_found(not_found_handler)

    println("Server running on http://localhost:8080")
    Server.bind("0.0.0.0:8080")!
        .router(router)!

    return Success(unit)
}

func home(req: Request) -> Response {
    return Response.ok()
        .body("Welcome to the API!")
}

func list_users(req: Request) -> Response {
    let users = get_all_users()
    return Response.ok()
        .json(ref users)
        .unwrap()
}

func get_user(req: Request) -> Response {
    let id: U64 = req.params().parse("id").unwrap().unwrap()

    when find_user(id) {
        Just(user) -> Response.ok().json(ref user).unwrap(),
        None -> Response.status(StatusCode.NOT_FOUND)
            .body("User not found"),
    }
}

func create_user(req: Request) -> Response {
    when req.json[CreateUserRequest]() {
        Success(data) -> {
            let user = save_user(data)
            Response.status(StatusCode.CREATED)
                .json(ref user)
                .unwrap()
        },
        Failure(_) -> Response.status(StatusCode.BAD_REQUEST)
            .body("Invalid JSON"),
    }
}

func delete_user(req: Request) -> Response {
    let id: U64 = req.params().parse("id").unwrap().unwrap()
    remove_user(id)
    Response.status(StatusCode.NO_CONTENT).build()
}

func not_found_handler(req: Request) -> Response {
    Response.status(StatusCode.NOT_FOUND)
        .json(&ErrorResponse { error: "Not found" })
        .unwrap()
}
```

### 8.3 File Upload

```tml
module upload_server
caps: [io.network.http.server, io.file.write]

import std.http.{Server, Request, Response, StatusCode}
import std.http.multipart.Multipart
import std.fs

func upload_handler(req: Request) -> Response {
    when Multipart.from_request(&req) {
        Success(form) -> {
            loop part in form.parts {
                when part.filename {
                    Just(filename) -> {
                        let path = Path.new("uploads").join(filename)
                        part.save(&path).ok()
                        println("Saved: " + filename)
                    },
                    None -> unit,
                }
            }
            Response.ok().body("Upload complete")
        },
        Failure(e) -> Response.status(StatusCode.BAD_REQUEST)
            .body("Invalid multipart form"),
    }
}
```

---

*Previous: [06-TLS.md](./06-TLS.md)*
*Next: [08-COMPRESS.md](./08-COMPRESS.md) — Compression Algorithms*
