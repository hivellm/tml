# std::http — HTTP Client and Server

## 1. Overview

The \x60std::http` package provides HTTP/1.1 and HTTP/2 client and server functionality.

```tml
use std::http
use std::http.{Client, Request, Response, Server}
```

## 2. Capabilities

```tml
caps: [io::network.http]           // Full HTTP access
caps: [io::network.http.client]    // Client only
caps: [io::network.http.server]    // Server only
```

## 3. HTTP Client

### 3.1 Client

```tml
pub type Client {
    config: ClientConfig,
    pool: ConnectionPool,
}

extend Client {
    /// Create with default configuration
    pub func new() -> This

    /// Create with custom configuration
    pub func with_config(config: ClientConfig) -> This

    /// Execute request
    pub func execute(this, request: Request) -> Outcome[Response, HttpError]
    effects: [io::network.http]

    /// GET request
    pub func get(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io::network.http]
    {
        return this.execute(Request.get(url)!)
    }

    /// POST request
    pub func post(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io::network.http]
    {
        return this.execute(Request.post(url)!.body(body))
    }

    /// PUT request
    pub func put(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io::network.http]

    /// DELETE request
    pub func delete(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io::network.http]

    /// PATCH request
    pub func patch(this, url: ref str, body: impl Into[Body]) -> Outcome[Response, HttpError]
    effects: [io::network.http]

    /// HEAD request
    pub func head(this, url: ref str) -> Outcome[Response, HttpError]
    effects: [io::network.http]
}
```

### 3.2 ClientConfig

```tml
pub type ClientConfig {
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
    pub func new() -> This {
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

    pub func timeout(this, dur: Duration) -> This
    pub func connect_timeout(this, dur: Duration) -> This
    pub func no_redirects(this) -> This
    pub func max_redirects(this, n: U32) -> This
    pub func user_agent(this, ua: ref str) -> This
    pub func default_header(this, name: ref str, value: ref str) -> This
    pub func proxy(this, proxy: Proxy) -> This
    pub func https_only(this) -> This
    pub func http2_only(this) -> This
}
```

### 3.3 RequestBuilder

```tml
pub type RequestBuilder {
    method: Method,
    url: Url,
    headers: HeaderMap,
    body: Maybe[Body],
    timeout: Maybe[Duration>,
}

extend RequestBuilder {
    pub func new(method: Method, url: ref str) -> Outcome[This, UrlError]

    /// Add header
    pub func header(this, name: ref str, value: ref str) -> This

    /// Add multiple headers
    pub func headers(this, headers: HeaderMap) -> This

    /// Set body
    pub func body(this, body: impl Into[Body]) -> This

    /// Set JSON body
    pub func json[T: Serialize](this, value: ref T) -> Outcome[This, JsonError]

    /// Set form body
    pub func form[T: Serialize](this, value: ref T) -> Outcome[This, FormError]

    /// Set request timeout
    pub func timeout(this, dur: Duration) -> This

    /// Set bearer token
    pub func bearer_auth(this, token: ref str) -> This

    /// Set basic auth
    pub func basic_auth(this, username: ref str, password: Maybe[ref str]) -> This

    /// Build request
    pub func build(this) -> Request
}

extend Request {
    pub func get(url: ref str) -> Outcome[RequestBuilder, UrlError]
    pub func post(url: ref str) -> Outcome[RequestBuilder, UrlError]
    pub func put(url: ref str) -> Outcome[RequestBuilder, UrlError]
    pub func delete(url: ref str) -> Outcome[RequestBuilder, UrlError]
    pub func patch(url: ref str) -> Outcome[RequestBuilder, UrlError]
    pub func head(url: ref str) -> Outcome[RequestBuilder, UrlError]
}
```

## 4. Request and Response

### 4.1 Request

```tml
pub type Request {
    method: Method,
    url: Url,
    headers: HeaderMap,
    body: Maybe[Body],
}

extend Request {
    pub func method(this) -> ref Method
    pub func url(this) -> ref Url
    pub func headers(this) -> ref HeaderMap
    pub func body(this) -> Maybe[ref Body]
    pub func body_mut(this) -> Maybe[mut ref Body]
}

pub type Method = Get | Post | Put | Delete | Patch | Head | Options | Connect | Trace

extend Method {
    pub func as_str(this) -> ref str
    pub func from_str(s: ref str) -> Outcome[This, InvalidMethod]
    pub func is_safe(this) -> Bool
    pub func is_idempotent(this) -> Bool
}
```

### 4.2 Response

```tml
pub type Response {
    status: StatusCode,
    headers: HeaderMap,
    body: Body,
    version: Version,
}

extend Response {
    /// Get status code
    pub func status(this) -> StatusCode

    /// Check if successful (2xx)
    pub func is_Ok(this) -> Bool {
        return this.status.is_Ok()
    }

    /// Check if redirect (3xx)
    pub func is_redirect(this) -> Bool {
        return this.status.is_redirect()
    }

    /// Check if client error (4xx)
    pub func is_client_error(this) -> Bool {
        return this.status.is_client_error()
    }

    /// Check if server error (5xx)
    pub func is_server_error(this) -> Bool {
        return this.status.is_server_error()
    }

    /// Get headers
    pub func headers(this) -> ref HeaderMap

    /// Get header value
    pub func header(this, name: ref str) -> Maybe[ref str] {
        return this.headers.get(name)
    }

    /// Get content type
    pub func content_type(this) -> Maybe[Mime>

    /// Get content length
    pub func content_length(this) -> Maybe[U64]

    /// Read body as bytes
    pub func bytes(this) -> Outcome[List[U8], HttpError]
    effects: [io::network.http]

    /// Read body as string
    pub func text(this) -> Outcome[String, HttpError]
    effects: [io::network.http]

    /// Parse body as JSON
    pub func json[T: Deserialize](this) -> Outcome[T, HttpError]
    effects: [io::network.http]

    /// Get body as reader
    pub func body(this) -> ref Body

    /// Get mutable body
    pub func body_mut(this) -> mut ref Body
}
```

### 4.3 StatusCode

```tml
pub type StatusCode {
    code: U16,
}

extend StatusCode {
    // Common status codes
    pub const OK: This = This { code: 200 }
    pub const CREATED: This = This { code: 201 }
    pub const ACCEPTED: This = This { code: 202 }
    pub const NO_CONTENT: This = This { code: 204 }
    pub const MOVED_PERMANENTLY: This = This { code: 301 }
    pub const FOUND: This = This { code: 302 }
    pub const NOT_MODIFIED: This = This { code: 304 }
    pub const BAD_REQUEST: This = This { code: 400 }
    pub const UNAUTHORIZED: This = This { code: 401 }
    pub const FORBIDDEN: This = This { code: 403 }
    pub const NOT_FOUND: This = This { code: 404 }
    pub const METHOD_NOT_ALLOWED: This = This { code: 405 }
    pub const CONFLICT: This = This { code: 409 }
    pub const GONE: This = This { code: 410 }
    pub const UNPROCESSABLE_ENTITY: This = This { code: 422 }
    pub const TOO_MANY_REQUESTS: This = This { code: 429 }
    pub const INTERNAL_SERVER_ERROR: This = This { code: 500 }
    pub const BAD_GATEWAY: This = This { code: 502 }
    pub const SERVICE_UNAVAILABLE: This = This { code: 503 }
    pub const GATEWAY_TIMEOUT: This = This { code: 504 }

    pub func from_u16(code: U16) -> Maybe[This]
    pub func as_u16(this) -> U16
    pub func reason_phrase(this) -> ref str
    pub func is_informational(this) -> Bool { this.code >= 100 and this.code < 200 }
    pub func is_Ok(this) -> Bool { this.code >= 200 and this.code < 300 }
    pub func is_redirect(this) -> Bool { this.code >= 300 and this.code < 400 }
    pub func is_client_error(this) -> Bool { this.code >= 400 and this.code < 500 }
    pub func is_server_error(this) -> Bool { this.code >= 500 and this.code < 600 }
}
```

### 4.4 Headers

```tml
pub type HeaderMap {
    inner: Map[HeaderName, List[HeaderValue]],
}

extend HeaderMap {
    pub func new() -> This

    /// Insert header (replaces existing)
    pub func insert(this, name: impl Into[HeaderName], value: impl Into[HeaderValue])

    /// Append header (adds to existing)
    pub func append(this, name: impl Into[HeaderName], value: impl Into[HeaderValue])

    /// Get first value for header
    pub func get(this, name: ref str) -> Maybe[ref str]

    /// Get all values for header
    pub func get_all(this, name: ref str) -> ref [HeaderValue]

    /// Check if header exists
    pub func contains(this, name: ref str) -> Bool

    /// Remove header
    pub func remove(this, name: ref str) -> Maybe[HeaderValue]

    /// Iterate over all headers
    pub func iter(this) -> HeaderIter
}

// Common headers
pub const CONTENT_TYPE: ref str = "Content-Type"
pub const CONTENT_LENGTH: ref str = "Content-Length"
pub const AUTHORIZATION: ref str = "Authorization"
pub const ACCEPT: ref str = "Accept"
pub const USER_AGENT: ref str = "User-Agent"
pub const HOST: ref str = "Host"
pub const COOKIE: ref str = "Cookie"
pub const SET_COOKIE: ref str = "Set-Cookie"
pub const LOCATION: ref str = "Location"
pub const CACHE_CONTROL: ref str = "Cache-Control"
```

### 4.5 Body

```tml
pub type Body {
    kind: BodyKind,
}

type BodyKind =
    | Empty
    | Bytes(List[U8])
    | Stream(Heap[dyn Read>)

extend Body {
    pub func empty() -> This
    pub func from_bytes(bytes: impl Into[List[U8]>) -> This
    pub func from_string(s: impl Into[String>) -> This
    pub func from_reader(reader: impl Read + 'static) -> This

    /// Check if body is empty
    pub func is_empty(this) -> Bool

    /// Get known length (if available)
    pub func len(this) -> Maybe[U64]
}

// Conversions
extend ref str with Into[Body] { ... }
extend String with Into[Body] { ... }
extend ref [U8] with Into[Body] { ... }
extend List[U8] with Into[Body] { ... }
```

## 5. URL Handling

```tml
pub type Url {
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
    pub func parse(s: ref str) -> Outcome[This, UrlError]

    pub func scheme(this) -> ref str
    pub func host(this) -> ref str
    pub func port(this) -> Maybe[U16]
    pub func port_or_default(this) -> U16
    pub func path(this) -> ref str
    pub func query(this) -> Maybe[ref str>
    pub func query_pairs(this) -> QueryPairs
    pub func fragment(this) -> Maybe[ref str]

    pub func set_path(this, path: ref str)
    pub func set_query(this, query: Maybe[ref str])
    pub func set_fragment(this, fragment: Maybe[ref str>)

    pub func join(this, path: ref str) -> Outcome[Url, UrlError]

    pub func to_string(this) -> String
}

pub type QueryPairs { ... }
extend QueryPairs with Iterator { type Item = (String, String) }
```

## 6. HTTP Server

### 6.1 Server

```tml
pub type Server {
    config: ServerConfig,
}

extend Server {
    /// Create server bound to address
    pub func bind(addr: impl ToSocketAddrs) -> Outcome[ServerBuilder, HttpError]
    effects: [io::network.http.server]

    /// Create HTTPS server
    pub func bind_tls(
        addr: impl ToSocketAddrs,
        cert: Certificate,
        key: PrivateKey
    ) -> Outcome[ServerBuilder, HttpError]
    effects: [io::network.http.server]
}

pub type ServerBuilder {
    addr: SocketAddr,
    tls: Maybe[TlsAcceptor],
    config: ServerConfig,
}

extend ServerBuilder {
    /// Set request handler
    pub func serve[H: Handler](this, handler: H) -> Outcome[Unit, HttpError]
    effects: [io::network.http.server]

    /// Set router
    pub func router(this, router: Router) -> Outcome[Unit, HttpError>
    effects: [io::network.http.server]

    /// Set max connections
    pub func max_connections(this, n: U32) -> This

    /// Set keep-alive timeout
    pub func keep_alive(this, dur: Duration) -> This

    /// Disable keep-alive
    pub func no_keep_alive(this) -> This

    /// Enable HTTP/2
    pub func http2(this) -> This
}
```

### 6.2 Handler Trait

```tml
pub behaviorHandler {
    func handle(this, request: Request) -> Response
    effects: [io::network.http.server]
}

// Function as handler
extend func(Request) -> Response with Handler {
    func handle(this, request: Request) -> Response {
        return this(request)
    }
}

// Async handler
pub behaviorAsyncHandler {
    async func handle(this, request: Request) -> Response
    effects: [io::network.http.server]
}
```

### 6.3 Router

```tml
pub type Router {
    routes: List[Route],
    not_found: Maybe[Heap[dyn Handler]>,
}

extend Router {
    pub func new() -> This

    /// Add route
    pub func route(this, method: Method, path: ref str, handler: impl Handler + 'static) -> This

    /// GET route
    pub func get(this, path: ref str, handler: impl Handler + 'static) -> This

    /// POST route
    pub func post(this, path: ref str, handler: impl Handler + 'static) -> This

    /// PUT route
    pub func put(this, path: ref str, handler: impl Handler + 'static) -> This

    /// DELETE route
    pub func delete(this, path: ref str, handler: impl Handler + 'static) -> This

    /// Nested router
    pub func nest(this, prefix: ref str, router: Router) -> This

    /// Set 404 handler
    pub func not_found(this, handler: impl Handler + 'static) -> This

    /// Add middleware
    pub func middleware(this, mw: impl Middleware + 'static) -> This
}

pub type Route {
    method: Method,
    pattern: PathPattern,
    handler: Heap[dyn Handler],
}
```

### 6.4 Path Parameters

```tml
pub type PathParams {
    params: Map[String, String],
}

extend PathParams {
    /// Get parameter by name
    pub func get(this, name: ref str) -> Maybe[ref str]

    /// Get parameter, parsing as type
    pub func parse[T: FromStr](this, name: ref str) -> Maybe[Outcome[T, T.Err]]
}

// Path patterns:
// "/users/:id"           - named parameter
// "/files/*path"         - wildcard (captures rest)
// "/api/v{version:\\d+}" - with regex constraint
```

### 6.5 Middleware

```tml
pub behaviorMiddleware {
    func call(this, request: Request, next: Next) -> Response
    effects: [io::network.http.server]
}

pub type Next {
    handler: Heap[dyn Handler],
}

extend Next {
    pub func call(this, request: Request) -> Response
}

// Common middleware
mod middleware

/// Logging middleware
pub type Logger { level: LogLevel }

/// CORS middleware
pub type Cors {
    allow_origins: List[String],
    allow_methods: List[Method],
    allow_headers: List[String],
    max_age: Maybe[Duration],
}

/// Compression middleware
pub type Compression { level: CompressionLevel }

/// Rate limiting middleware
pub type RateLimit {
    requests_per_second: U32,
    burst: U32,
}
```

## 7. Multipart Form Data

```tml
mod multipart

pub type Multipart {
    parts: List[Part],
    boundary: String,
}

pub type Part {
    name: String,
    filename: Maybe[String],
    content_type: Maybe[Mime],
    data: List[U8],
}

extend Multipart {
    /// Create new multipart form
    pub func new() -> This

    /// Add text field
    pub func text(this, name: ref str, value: ref str) -> This

    /// Add file
    pub func file(this, name: ref str, filename: ref str, data: ref [U8], content_type: Mime) -> This

    /// Parse from request
    pub func from_request(request: ref Request) -> Outcome[This, MultipartError]
    effects: [io::network.http]
}

extend Part {
    /// Get data as string
    pub func text(this) -> Outcome[String, Utf8Error]

    /// Get data as bytes
    pub func bytes(this) -> ref [U8]

    /// Save to file
    pub func save(this, path: ref Path) -> Outcome[Unit, IoError]
    effects: [io::file.write]
}
```

## 8. Examples

### 8.1 Simple HTTP Client

```tml
mod http_example
caps: [io::network.http]

use std::http.{Client, Request}

pub func main() -> Outcome[Unit, Error] {
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

    if response.is_Ok() {
        let created: User = response.json()!
        println("Created user: " + created.name)
    }

    return Ok(unit)
}
```

### 8.2 REST API Server

```tml
mod api_server
caps: [io::network.http.server]

use std::http.{Server, Router, Request, Response, StatusCode}
use std::http.middleware.{Logger, Cors}

pub func main() -> Outcome[Unit, Error] {
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

    return Ok(unit)
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
        Ok(data) -> {
            let user = save_user(data)
            Response.status(StatusCode.CREATED)
                .json(ref user)
                .unwrap()
        },
        Err(_) -> Response.status(StatusCode.BAD_REQUEST)
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
mod upload_server
caps: [io::network.http.server, io::file.write]

use std::http.{Server, Request, Response, StatusCode}
use std::http.multipart.Multipart
use std::fs

func upload_handler(req: Request) -> Response {
    when Multipart.from_request(&req) {
        Ok(form) -> {
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
        Err(e) -> Response.status(StatusCode.BAD_REQUEST)
            .body("Invalid multipart form"),
    }
}
```

---

*Previous: [06-TLS.md](./06-TLS.md)*
*Next: [08-COMPRESS.md](./08-COMPRESS.md) — Compression Algorithms*
