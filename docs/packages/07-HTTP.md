# std::http -- HTTP Client, Server, Middleware, and Protocol Types

## 1. Overview

The `std::http` module provides a complete HTTP/1.1 stack: client functionality, server
listener and request handling, an Express.js-compatible middleware ecosystem, URL routing,
and shared protocol types (methods, status codes, headers, cookies, error types). Shared
protocol types are kept separate from client/server logic so they can be used by both.

```tml
use std::http                              // all re-exports
use std::http::{HttpClient, Request, Response}
use std::http::{HttpServer, ServerResponse, IncomingMessage}
use std::http::{Method, Status, Headers}
use std::http::{Router, RouteMatch}
use std::http::{Cookie, HttpError, HttpErrorKind}
use std::http::{Connection, ConnectionInfo}
use std::http::{HttpVersion}
use std::http::encoding::{compress, decompress, accepted_encodings}
use std::http::cors::{CorsOptions, cors_headers, preflight_headers}
use std::http::compression::{CompressionOptions, compress_response}
use std::http::security::{SecurityOptions, security_headers}
use std::http::body_parser::{ParsedBody, parse_body, form_get, url_decode}
use std::http::etag::{generate, is_not_modified}
use std::http::stream::{SseEvent, sse_headers}
use std::http::range::{ByteRange, parse_range, content_range}
use std::http::cache_control::{CacheOptions, cache_control_header}
use std::http::rate_limit::{RateLimiter, RateLimitOptions}
use std::http::static_server::{StaticOptions, resolve_path, static_file_headers}
```

## 2. Implementation Status

| Module | File | Status | Description |
|--------|------|--------|-------------|
| `method` | `method.tml` | IMPLEMENTED | HTTP request methods (GET, POST, PUT, etc.) |
| `status` | `status.tml` | IMPLEMENTED | Full IANA status code registry (100-511) |
| `version` | `version.tml` | IMPLEMENTED | HTTP protocol versions (1.0, 1.1, 2, 3) |
| `headers` | `headers.tml` | IMPLEMENTED | Case-insensitive header map (linear array) |
| `error` | `error.tml` | IMPLEMENTED | HttpError and HttpErrorKind types |
| `cookie` | `cookie.tml` | IMPLEMENTED | Cookie builder and parser (RFC 6265) |
| `encoding` | `encoding.tml` | IMPLEMENTED | Content-Encoding compress/decompress (gzip, deflate, br, zstd) |
| `connection` | `connection.tml` | IMPLEMENTED | DNS + TCP + optional TLS connection |
| `request` | `request.tml` | IMPLEMENTED | Request builder with URL parsing and wire serialization |
| `response` | `response.tml` | IMPLEMENTED | Response parser from HTTP/1.1 wire format |
| `client` | `client.tml` | IMPLEMENTED | HTTP client: send requests, receive responses |
| `router` | `router.tml` | IMPLEMENTED | Radix tree router with parametric and wildcard support |
| `multipart` | `multipart.tml` | IMPLEMENTED | Multipart/form-data builder (RFC 2046) |
| `chunked` | `chunked.tml` | IMPLEMENTED | Chunked transfer encoding (RFC 7230) |
| `functions` | `functions.tml` | IMPLEMENTED | Utility functions: header validation, method/status lists |
| `content_type` | `content_type.tml` | IMPLEMENTED | Content-Type parsing, wildcards, Accept negotiation |
| `body_parser` | `body_parser.tml` | IMPLEMENTED | Request body parsing (JSON, form, text, raw) |
| Server | `server.tml` | IMPLEMENTED | HttpServer: bind, accept, send response |
| Server response | `server_response.tml` | IMPLEMENTED | ServerResponse: build and serialize HTTP responses |
| Incoming message | `incoming.tml` | IMPLEMENTED | IncomingMessage: parsed server-side request |
| Agent | `agent.tml` | IMPLEMENTED | Connection pooling configuration (Node.js Agent API) |
| CORS | `cors.tml` | IMPLEMENTED | Cross-Origin Resource Sharing header generation |
| Compression | `compression.tml` | IMPLEMENTED | Automatic response compression middleware |
| Security | `security.tml` | IMPLEMENTED | Security header generation (helmet-equivalent) |
| ETag | `etag.tml` | IMPLEMENTED | ETag generation and conditional request handling |
| Stream/SSE | `stream.tml` | IMPLEMENTED | Server-Sent Events and streaming HTTP helpers |
| Range | `range.tml` | IMPLEMENTED | Range request parsing and Content-Range generation |
| Cache-Control | `cache_control.tml` | IMPLEMENTED | Cache-Control header generation |
| Rate limiting | `rate_limit.tml` | IMPLEMENTED | In-memory token bucket rate limiter |
| Static server | `static_server.tml` | IMPLEMENTED | Static file serving utilities |
| HTTP/2 | -- | NOT YET IMPLEMENTED | HTTP/2 framing, multiplexing, HPACK |
| WebSocket | -- | NOT YET IMPLEMENTED | WebSocket upgrade and frame protocol |

## 3. Module Structure

```
std::http
  +-- method           Shared: HTTP request methods
  +-- status           Shared: HTTP status codes (full IANA)
  +-- version          Shared: HTTP protocol versions
  +-- headers          Shared: Case-insensitive header map
  +-- error            Shared: HttpError, HttpErrorKind
  +-- encoding         Shared: Content-Encoding compress/decompress
  +-- cookie           Shared: Cookie parse/serialize (RFC 6265)
  +-- chunked          Shared: Chunked transfer encoding
  +-- functions        Shared: Header validation, method/status lists
  +-- content_type     Shared: Content-Type parsing and negotiation
  +-- body_parser      Shared: Request body parsing
  +-- connection       Client: DNS + TCP + TLS connection
  +-- request          Client: Request builder and serializer
  +-- response         Client: Response parser
  +-- client           Client: HttpClient (send/receive)
  +-- agent            Client: Connection pool configuration
  +-- incoming         Server: IncomingMessage (parsed request)
  +-- server_response  Server: ServerResponse (response builder)
  +-- server           Server: HttpServer (bind/accept/send)
  +-- router           Server: Radix tree URL router
  +-- multipart        Shared: Multipart/form-data builder
  +-- cors             Middleware: CORS header generation
  +-- compression      Middleware: Response compression
  +-- security         Middleware: Security header generation
  +-- etag             Middleware: ETag generation and validation
  +-- stream           Middleware: SSE and streaming helpers
  +-- range            Middleware: Range request support
  +-- cache_control    Middleware: Cache-Control header generation
  +-- rate_limit       Middleware: In-memory token bucket rate limiter
  +-- static_server    Middleware: Static file serving utilities
```

Re-exports from `std::http::mod`:

```tml
pub use std::http::method::Method
pub use std::http::status::Status
pub use std::http::version::HttpVersion
pub use std::http::headers::Headers
pub use std::http::error::{HttpError, HttpErrorKind}
pub use std::http::cookie::Cookie
pub use std::http::router::{Router, RouteMatch}
pub use std::http::encoding::{compress, decompress, accepted_encodings}
pub use std::http::connection::{Connection, ConnectionInfo}
pub use std::http::request::Request
pub use std::http::response::Response
pub use std::http::client::HttpClient
pub use std::http::incoming::{IncomingMessage, parse_request}
pub use std::http::server_response::ServerResponse
pub use std::http::server::HttpServer
pub use std::http::agent::{Agent, AgentOptions}
pub use std::http::chunked::{encode_chunk, encode_final_chunk, encode_body_chunked, decode_chunked, ChunkedResult}
pub use std::http::multipart::{MultipartBuilder, MultipartPart}
pub use std::http::content_type::{parse_content_type, matches_type, negotiate}
pub use std::http::body_parser::{ParsedBody, parse_body, form_get, url_decode}
pub use std::http::cors::{CorsOptions, cors_headers, preflight_headers, is_preflight}
pub use std::http::compression::{CompressionOptions, select_encoding, should_compress, compress_response}
pub use std::http::security::{SecurityOptions, security_headers}
pub use std::http::etag::{generate, generate_weak, is_not_modified}
pub use std::http::stream::{SseEvent, sse_headers, sse_ping, sse_comment, streaming_headers, ndjson_headers, ndjson_line}
pub use std::http::range::{ByteRange, parse_range, content_range, content_range_unsatisfied, is_range_satisfiable}
pub use std::http::cache_control::{CacheOptions, cache_control_header}
pub use std::http::rate_limit::{RateLimitOptions, RateLimitResult, RateLimiter}
pub use std::http::static_server::{StaticOptions, resolve_path, get_extension, mime_for_extension, static_file_headers}
```

## 4. HttpClient

The `HttpClient` type sends HTTP requests and returns parsed responses. It manages
connection setup (DNS, TCP, TLS), request serialization, and response parsing internally.

```tml
pub type HttpClient {
    user_agent: Str,
}
```

### 4.1 Constructors

```tml
impl HttpClient {
    /// Creates a client with default User-Agent "tml/1.0".
    pub func new() -> HttpClient

    /// Creates a client with a custom User-Agent string.
    pub func with_user_agent(ua: Str) -> HttpClient
}
```

### 4.2 Convenience Methods

Each convenience method builds a `Request` internally with `User-Agent`, `Connection: close`,
and `Accept: */*` headers, then calls `send()`.

```tml
impl HttpClient {
    /// GET request.
    pub func get(this, url: Str) -> Outcome[Response, HttpError]

    /// POST request with string body.
    pub func post(this, url: Str, body: Str) -> Outcome[Response, HttpError]

    /// POST request with JSON body (auto-sets Content-Type: application/json).
    pub func post_json(this, url: Str, json_body: Str) -> Outcome[Response, HttpError]

    /// PUT request with string body.
    pub func put(this, url: Str, body: Str) -> Outcome[Response, HttpError]

    /// DELETE request.
    pub func delete(this, url: Str) -> Outcome[Response, HttpError]

    /// HEAD request (response has no body).
    pub func head(this, url: Str) -> Outcome[Response, HttpError]
}
```

### 4.3 Core Send

```tml
impl HttpClient {
    /// Sends a Request and returns the parsed Response.
    ///
    /// 1. Extracts host/port/scheme from the request URL.
    /// 2. Opens a Connection (DNS + TCP + optional TLS).
    /// 3. Serializes the request to HTTP/1.1 wire format.
    /// 4. Reads the full response (up to 8 MB).
    /// 5. Parses the raw response into a Response struct.
    pub func send(this, req: Request) -> Outcome[Response, HttpError]
}
```

### 4.4 Usage Example

```tml
use std::http::{HttpClient, Request}

let client = HttpClient::new()

// Simple GET
let result = client.get("https://api.example.com/users")
when result {
    Ok(resp) => {
        print("Status: {resp.status().code()}\n")
        print("Body: {resp.text()}\n")
    },
    Err(e) => print("Error: {e.msg()}\n"),
}

// POST with JSON body
let result = client.post_json("https://api.example.com/users", "{\"name\":\"Alice\"}")
when result {
    Ok(resp) => print("Created: {resp.text()}\n"),
    Err(e) => print("Error: {e.msg()}\n"),
}

// Custom request via builder
let req = Request::post("https://api.example.com/data")
    .header("Authorization", "Bearer token123")
    .header("X-Custom", "value")
    .json("{\"key\":\"value\"}")
let result = client.send(req)
```

## 5. Request

The `Request` type is a builder for HTTP requests. It parses the URL on construction and
stores components internally. Builder methods return `Request` for chaining.

```tml
pub type Request {
    req_method: Method,
    req_url: Str,
    url_scheme: I64,
    url_host: I64,
    url_port: I64,
    url_path: I64,
    url_query: I64,
    req_headers: Headers,
    req_body: Str,
    req_version: HttpVersion,
    timeout_ms: I64,
}
```

### 5.1 Constructors

```tml
impl Request {
    pub func new(method: Method, url: Str) -> Request
    pub func get(url: Str) -> Request
    pub func post(url: Str) -> Request
    pub func put(url: Str) -> Request
    pub func delete(url: Str) -> Request
    pub func patch(url: Str) -> Request
    pub func head(url: Str) -> Request
    pub func options(url: Str) -> Request
}
```

### 5.2 Builder Methods

```tml
impl Request {
    pub func header(this, key: Str, value: Str) -> Request
    pub func body(this, data: Str) -> Request
    pub func json(this, data: Str) -> Request         // auto-sets Content-Type: application/json
    pub func form(this, data: Str) -> Request         // auto-sets Content-Type: application/x-www-form-urlencoded
    pub func version(this, v: HttpVersion) -> Request
    pub func timeout(this, ms: I64) -> Request
}
```

### 5.3 Accessors

```tml
impl Request {
    pub func method(this) -> Method
    pub func url(this) -> Str
    pub func headers(this) -> ref Headers
    pub func body_data(this) -> Str
    pub func http_version(this) -> HttpVersion
    pub func get_timeout(this) -> I64
    pub func scheme(this) -> Str        // "http" or "https"
    pub func host(this) -> Str
    pub func port(this) -> I64          // -1 if not specified
    pub func path(this) -> Str
    pub func query(this) -> Str
    pub func path_and_query(this) -> Str
    pub func host_header(this) -> Str
    pub func has_body(this) -> Bool
    pub func is_idempotent(this) -> Bool
    pub func is_safe(this) -> Bool
    pub func is_secure(this) -> Bool
    pub func content_type(this) -> Str
    pub func content_length(this) -> I64
    pub func serialize(this) -> Str     // HTTP/1.1 wire format
    pub func destroy(this)
}
```

## 6. Response

Parses HTTP/1.1 responses from raw wire format into structured status, headers, and body.

```tml
pub type Response {
    status_code: Status,
    headers: Headers,
    body_data: Str,
    http_version: HttpVersion,
}
```

### 6.1 Constructors

```tml
impl Response {
    pub func new(status: Status, headers: Headers, body: Str) -> Response
    pub func parse(raw: Str) -> Outcome[Response, HttpError]
}
```

### 6.2 Accessors

```tml
impl Response {
    pub func status(this) -> Status
    pub func headers(this) -> ref Headers
    pub func text(this) -> Str
    pub func version(this) -> HttpVersion
    pub func content_length(this) -> I64
    pub func content_type(this) -> Str
    pub func is_ok(this) -> Bool           // 2xx
    pub func is_redirect(this) -> Bool     // 3xx
    pub func is_client_error(this) -> Bool // 4xx
    pub func is_server_error(this) -> Bool // 5xx
    pub func destroy(this)
}
```

## 7. HttpServer

The `HttpServer` type listens for incoming TCP connections and delivers
request/response pairs. Corresponds to Node.js `http.Server`.

```tml
pub type HttpServer {
    listener: I64,
    is_listening: Bool,
    port: I64,
    host: Str,
    headers_timeout: I64,
    keep_alive_timeout: I64,
    request_timeout: I64,
    max_headers_count: I64,
    max_requests_per_socket: I64,
    timeout: I64,
}
```

### 7.1 Lifecycle

```tml
impl HttpServer {
    /// Creates a new server with default settings.
    pub func new() -> HttpServer

    /// Binds to a TCP port and starts accepting connections.
    pub func listen(mut this, port: I64) -> Outcome[Unit, HttpError]

    /// Returns true if the server is currently bound and listening.
    pub func listening(this) -> Bool

    /// Accepts a single connection. Returns the parsed request and a response builder.
    pub func accept(this) -> Outcome[(IncomingMessage, ServerResponse), HttpError]

    /// Sends a completed ServerResponse to the client and closes the socket.
    pub func send_response(this, resp: ref ServerResponse) -> Outcome[Unit, HttpError]

    /// Closes the listening socket.
    pub func close(mut this)
}
```

### 7.2 Configuration

```tml
impl HttpServer {
    pub func set_timeout(mut this, ms: I64)
    pub func set_headers_timeout(mut this, ms: I64)
    pub func set_keep_alive_timeout(mut this, ms: I64)
    pub func set_request_timeout(mut this, ms: I64)
    pub func set_max_headers_count(mut this, count: I64)
    pub func set_max_requests_per_socket(mut this, count: I64)

    pub func get_port(this) -> I64
    pub func get_host(this) -> Str
    pub func get_headers_timeout(this) -> I64
    pub func get_keep_alive_timeout(this) -> I64
    pub func get_request_timeout(this) -> I64
    pub func get_max_headers_count(this) -> I64
    pub func get_timeout(this) -> I64
}
```

### 7.3 Usage Example

```tml
use std::http::{HttpServer, IncomingMessage, ServerResponse}

func main() {
    var server = HttpServer::new()
    let bind_result = server.listen(8080)
    when bind_result {
        Err(e) => {
            print("Failed to bind: {e.msg()}\n")
            return
        },
        Ok(_) => print("Server listening on port 8080\n"),
    }

    // Accept one request
    let accept_result = server.accept()
    when accept_result {
        Err(e) => print("Accept error: {e.msg()}\n"),
        Ok(pair) => {
            let req = pair.0
            var resp = pair.1

            print("Received: {req.method()} {req.url()}\n")

            resp.set_header("Content-Type", "application/json")
            resp.end("{\"status\":\"ok\"}")

            let send_result = server.send_response(ref resp)
            when send_result {
                Err(e) => print("Send error: {e.msg()}\n"),
                Ok(_) => print("Response sent\n"),
            }
        },
    }

    server.close()
}
```

## 8. IncomingMessage

Represents an HTTP request received by the server. Corresponds to Node.js
`http.IncomingMessage` (server-side).

```tml
pub type IncomingMessage {
    req_method: Str,
    req_url: Str,
    req_headers_ptr: I64,
    raw_header_count: I64,
    req_body: Str,
    req_version: Str,
    is_complete: Bool,
    socket_fd: I64,
}
```

### 8.1 Accessors

```tml
impl IncomingMessage {
    pub func method(this) -> Str          // "GET", "POST", etc.
    pub func url(this) -> Str             // full URL path with query string
    pub func path(this) -> Str            // URL path without query string
    pub func query(this) -> Str           // query string without "?"
    pub func body(this) -> Str
    pub func http_version(this) -> Str    // "HTTP/1.1"
    pub func complete(this) -> Bool
    pub func socket(this) -> I64

    // Header access (case-insensitive, pass lowercase key)
    pub func get_header(this, name: Str) -> Str
    pub func has_header(this, name: Str) -> Bool
    pub func header_count(this) -> I64

    // Convenience header accessors
    pub func host(this) -> Str
    pub func content_type(this) -> Str
    pub func content_length(this) -> I64   // -1 if not set
    pub func connection(this) -> Str
    pub func user_agent(this) -> Str
    pub func accept(this) -> Str
    pub func authorization(this) -> Str
    pub func is_keep_alive(this) -> Bool
    pub func is_chunked(this) -> Bool

    pub func destroy(this)
}

/// Parses an HTTP/1.1 request from raw wire format.
/// Expects: "GET /path HTTP/1.1\r\nHeader: Value\r\n\r\nbody"
pub func parse_request(raw: Str) -> Outcome[IncomingMessage, HttpError]
```

## 9. ServerResponse

Builds an HTTP response on the server side. Corresponds to Node.js
`http.ServerResponse`.

```tml
pub type ServerResponse {
    resp_status_code: I64,
    resp_status_message: Str,
    resp_headers_ptr: I64,
    body_chunks: I64,
    body_chunk_count: I64,
    body_total_len: I64,
    headers_sent: Bool,
    finished: Bool,
    send_date: Bool,
    socket_fd: I64,
    req_method: Str,
}
```

### 9.1 Methods

```tml
impl ServerResponse {
    pub func new() -> ServerResponse
    pub func for_request(fd: I64, method: Str) -> ServerResponse

    // Status
    pub func set_status(mut this, code: I64)
    pub func status_code(this) -> I64
    pub func set_status_message(mut this, message: Str)
    pub func status_message(this) -> Str

    // Headers
    pub func set_header(this, name: Str, value: Str)
    pub func get_header(this, name: Str) -> Str
    pub func has_header(this, name: Str) -> Bool
    pub func is_headers_sent(this) -> Bool
    pub func set_send_date(mut this, send: Bool)

    // Write head
    pub func write_head(mut this, code: I64, message: Str)
    pub func write_head_simple(mut this, code: I64)
    pub func flush_headers(mut this)

    // Body
    pub func write(mut this, chunk: Str)
    pub func end(mut this, data: Str)
    pub func end_empty(mut this)
    pub func is_finished(this) -> Bool

    // Informational responses
    pub func write_continue() -> Str
    pub func write_processing() -> Str
    pub func write_early_hints(link: Str) -> Str

    // Wire serialization
    pub func serialize(this) -> Str

    pub func destroy(this)
}
```

### 9.2 Usage Example

```tml
var resp = ServerResponse::new()
resp.set_status(200)
resp.set_header("Content-Type", "text/plain")
resp.end("Hello, world!")

// resp.serialize() produces full HTTP/1.1 wire format:
// "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, world!"
```

## 10. Method

```tml
pub type Method {
    GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, TRACE, CONNECT,
}

impl Method {
    pub func to_string(this) -> Str
    pub func from_string(s: Str) -> Method
    pub func has_body(this) -> Bool
    pub func is_idempotent(this) -> Bool
    pub func is_safe(this) -> Bool
}
```

## 11. Status

Full IANA HTTP status code registry (100-511).

```tml
pub type Status {
    code_value: I32,
}

impl Status {
    pub func new(code: I32) -> Status
    pub func code(this) -> I32
    pub func reason(this) -> Str

    pub func is_informational(this) -> Bool     // 1xx
    pub func is_success(this) -> Bool           // 2xx
    pub func is_redirect(this) -> Bool          // 3xx
    pub func is_client_error(this) -> Bool      // 4xx
    pub func is_server_error(this) -> Bool      // 5xx
    pub func is_redirect_with_body(this) -> Bool // 307, 308
    pub func is_empty(this) -> Bool             // 204, 205, 304
    pub func is_retry(this) -> Bool             // 502, 503, 504
}
```

Named constructors: `Status::OK()` (200), `Status::NOT_FOUND()` (404),
`Status::INTERNAL_SERVER_ERROR()` (500), and the full IANA set from 100 to 511.

## 12. Headers

A case-insensitive HTTP header map. Uses parallel arrays with linear scan — intentionally
not a HashMap because HTTP messages typically have fewer than 30 headers.

```tml
pub type Headers {
    handle: *Unit
}

impl Headers {
    pub func new() -> Headers
    pub func set(this, key: Str, value: Str)
    pub func get(this, key: Str) -> Str
    pub func has(this, key: Str) -> Bool
    pub func remove(this, key: Str)
    pub func append(this, key: Str, value: Str)  // comma-separated per RFC 7230
    pub func serialize(this) -> Str              // "Key: Value\r\n" wire format
    pub func len(this) -> I64
    pub func is_empty(this) -> Bool
    pub func destroy(this)

    // Convenience accessors
    pub func content_length(this) -> I64
    pub func content_type(this) -> Str
    pub func content_encoding(this) -> Str
    pub func transfer_encoding(this) -> Str
    pub func is_chunked(this) -> Bool
    pub func host(this) -> Str
    pub func connection(this) -> Str
    pub func is_keep_alive(this) -> Bool
    pub func is_close(this) -> Bool
    pub func accept(this) -> Str
    pub func accept_encoding(this) -> Str
    pub func authorization(this) -> Str
    pub func location(this) -> Str
}
```

## 13. Router

A high-performance URL router using a compressed radix tree (one tree per HTTP method).
Priority order: static > parametric > wildcard.

```tml
pub type Router {
    methods: HashMap[Str, I64]
}

pub type RouteMatch {
    found: Bool,
    handler_id: I64,
    param_names: List[Str],
    param_values: List[Str],
}

impl Router {
    pub func new() -> Router
    pub func on(this, method: Str, path: Str, handler_id: I64)
    pub func find(this, method: Str, path: Str) -> RouteMatch
    pub func destroy(this)
}

impl RouteMatch {
    pub func get_param(this, name: Str) -> Str
    pub func param_count(this) -> I64
    pub func destroy(this)
}
```

Route pattern syntax:

| Pattern | Example Path | Parameters |
|---------|-------------|------------|
| `/users` | `/users` | none |
| `/users/:id` | `/users/42` | `id = "42"` |
| `/users/:id/posts` | `/users/42/posts` | `id = "42"` |
| `/files/*filepath` | `/files/css/main.css` | `filepath = "css/main.css"` |

```tml
use std::http::router::{Router, RouteMatch}

let router = Router::new()
router.on("GET", "/", 1)
router.on("GET", "/users/:id", 2)
router.on("POST", "/users", 3)
router.on("GET", "/files/*filepath", 4)

let m = router.find("GET", "/users/42")
// m.found == true, m.handler_id == 2, m.get_param("id") == "42"
router.destroy()
```

## 14. HttpVersion

```tml
pub type HttpVersion {
    HTTP_1_0, HTTP_1_1, HTTP_2, HTTP_3,
}

impl HttpVersion {
    pub func to_string(this) -> Str
    pub func from_string(s: Str) -> HttpVersion
    pub func is_h2_or_later(this) -> Bool
}
```

## 15. Cookie

HTTP cookie builder and parser per RFC 6265.

```tml
pub type Cookie {
    name: Str,
    value: Str,
    path: Str,
    domain: Str,
    max_age: I64,
    http_only: Bool,
    secure: Bool,
    same_site: Str,
}

impl Cookie {
    pub func new(name: Str, value: Str) -> Cookie
    pub func with_path(this, path: Str) -> Cookie
    pub func with_domain(this, domain: Str) -> Cookie
    pub func with_max_age(this, seconds: I64) -> Cookie
    pub func with_http_only(this) -> Cookie
    pub func with_secure(this) -> Cookie
    pub func with_same_site(this, policy: Str) -> Cookie
    pub func to_set_cookie(this) -> Str
}

pub func parse_cookie(header: Str, name: Str) -> Str
```

## 16. HttpError and HttpErrorKind

```tml
pub type HttpErrorKind { value: I32 }
pub type HttpError { error_kind: HttpErrorKind, message: Str }
pub type HttpResult[T] = Outcome[T, HttpError]

impl HttpErrorKind {
    pub func InvalidUrl() -> HttpErrorKind          // 1
    pub func DnsFailure() -> HttpErrorKind          // 2
    pub func ConnectionFailed() -> HttpErrorKind    // 3
    pub func TlsError() -> HttpErrorKind            // 4
    pub func Timeout() -> HttpErrorKind             // 5
    pub func InvalidResponse() -> HttpErrorKind     // 6
    pub func TooManyRedirects() -> HttpErrorKind    // 7
    pub func BodyTooLarge() -> HttpErrorKind        // 8
    pub func EncodingError() -> HttpErrorKind       // 9
    pub func ProtocolError() -> HttpErrorKind       // 10
    pub func AlreadySent() -> HttpErrorKind         // 11
    pub func InvalidStatusCode() -> HttpErrorKind   // 12
    pub func InvalidHeader() -> HttpErrorKind       // 13
    pub func InvalidPayload() -> HttpErrorKind      // 14
    pub func RouteError() -> HttpErrorKind          // 15
    pub func HookError() -> HttpErrorKind           // 16
}

impl HttpError {
    pub func new(kind: HttpErrorKind, msg: Str) -> HttpError
    pub func kind(this) -> HttpErrorKind
    pub func msg(this) -> Str
    pub func invalid_url(msg: Str) -> HttpError
    pub func dns_failure(msg: Str) -> HttpError
    pub func connection_failed(msg: Str) -> HttpError
    pub func tls_error(msg: Str) -> HttpError
    pub func timeout(msg: Str) -> HttpError
    pub func invalid_response(msg: Str) -> HttpError
    pub func too_many_redirects(msg: Str) -> HttpError
    pub func body_too_large(msg: Str) -> HttpError
    pub func encoding_error(msg: Str) -> HttpError
    pub func protocol_error(msg: Str) -> HttpError
}
```

## 17. Connection

Low-level connection layer handling DNS, TCP, and optional TLS. Used internally by
`HttpClient` but also available directly.

```tml
pub type Connection {
    tcp: TcpStream,
    ssl_ptr: *Unit,
    is_tls: Bool,
    fd: I64,
    info: ConnectionInfo,
}

pub type ConnectionInfo {
    tls_version: Str,
    cipher: Str,
    peer_verified: Bool,
}

impl Connection {
    pub func open(host: Str, port: I64, is_https: Bool) -> Outcome[Connection, HttpError]
    pub func write_str(this, data: Str) -> Outcome[I64, HttpError]
    pub func read(this, buf: mut ref [U8]) -> Outcome[I64, HttpError]
    pub func connection_info(this) -> ConnectionInfo
    pub func is_encrypted(this) -> Bool
    pub func get_fd(this) -> I64
    pub func close(this)
}
```

## 18. Content-Encoding

The `std::http::encoding` module dispatches compression and decompression to the
appropriate algorithm. Supported encodings: `gzip`, `x-gzip`, `deflate`, `br`, `zstd`.

```tml
pub func decompress(encoding: Str, data: Str) -> Outcome[Str, HttpError]
pub func compress(encoding: Str, data: Str) -> Outcome[Str, HttpError]
pub func accepted_encodings() -> Str   // "gzip, deflate, br, zstd"
```

## 19. Multipart

Builds multipart/form-data request bodies per RFC 2046.

```tml
pub type MultipartBuilder {
    boundary: Str,
    parts: List[MultipartPart],
}

impl MultipartBuilder {
    pub func new() -> MultipartBuilder
    pub func with_boundary(boundary: Str) -> MultipartBuilder
    pub func add_field(this, name: Str, value: Str)
    pub func add_file(this, name: Str, filename: Str, content_type: Str, data: Str)
    pub func content_type(this) -> Str
    pub func build(this) -> Str
    pub func destroy(this)
}
```

```tml
let mp = MultipartBuilder::new()
mp.add_field("username", "alice")
mp.add_file("avatar", "photo.jpg", "image/jpeg", file_data)

let req = Request::post("https://example.com/upload")
    .header("Content-Type", mp.content_type())
    .body(mp.build())
mp.destroy()
```

## 20. Agent (Connection Pooling)

The `Agent` type corresponds to Node.js `http.Agent`. It stores configuration for
connection pooling behavior. Corresponds to the Node.js API.

```tml
pub type AgentOptions {
    keep_alive: Bool,
    keep_alive_msecs: I64,
    max_sockets: I64,
    max_total_sockets: I64,
    max_free_sockets: I64,
    scheduling: Str,       // "lifo" or "fifo"
    timeout: I64,
    default_port: I64,
    protocol: Str,         // "http:" or "https:"
}

impl AgentOptions {
    pub func new() -> AgentOptions                       // default: no keep-alive, max 256 free sockets
    pub func with_keep_alive(this, enabled: Bool) -> AgentOptions
    pub func with_max_sockets(this, n: I64) -> AgentOptions
}
```

## 21. Middleware Modules

The following modules implement the Express.js middleware ecosystem. They are stateless
helpers that produce or interpret HTTP headers — they do not use a framework pipeline
abstraction. Apply them in a request handler loop by calling the relevant functions.

### 21.1 CORS (`std::http::cors`)

Generates Cross-Origin Resource Sharing headers per the Fetch specification.
Inspired by Express.js `cors` middleware.

```tml
pub type CorsOptions {
    allow_origin: Str,        // "*" or specific origin
    allow_methods: Str,       // "GET,HEAD,PUT,PATCH,POST,DELETE"
    allow_headers: Str,
    expose_headers: Str,
    allow_credentials: Bool,
    max_age: I64,             // preflight cache in seconds
}

impl CorsOptions {
    pub func new() -> CorsOptions                        // allow all origins, common methods
    pub func with_origin(this, origin: Str) -> CorsOptions
    pub func with_methods(this, methods: Str) -> CorsOptions
    pub func with_headers(this, headers: Str) -> CorsOptions
    pub func with_expose_headers(this, headers: Str) -> CorsOptions
    pub func with_credentials(this) -> CorsOptions
    pub func with_max_age(this, seconds: I64) -> CorsOptions
}

/// Generates CORS response headers as a header block string.
pub func cors_headers(opts: ref CorsOptions, request_origin: Str) -> Str

/// Generates preflight (OPTIONS) response headers.
pub func preflight_headers(opts: ref CorsOptions, request_origin: Str, request_method: Str, request_headers: Str) -> Str

/// Returns true if the request is a CORS preflight.
pub func is_preflight(method: Str, has_origin: Bool, has_request_method: Bool) -> Bool
```

```tml
use std::http::cors::{CorsOptions, cors_headers, preflight_headers, is_preflight}

let cors = CorsOptions::new()
    .with_origin("https://example.com")
    .with_credentials()
    .with_max_age(86400)

// In your request handler:
let origin = req.get_header("origin")
let method = req.method()

if is_preflight(method, str::len(origin) > 0, req.has_header("access-control-request-method")) {
    // Respond to preflight
    var resp = ServerResponse::new()
    resp.set_status(204)
    let hdrs = preflight_headers(ref cors, origin, req.get_header("access-control-request-method"), req.get_header("access-control-request-headers"))
    // Apply hdrs to resp...
    resp.end_empty()
} else {
    let hdrs = cors_headers(ref cors, origin)
    // Apply hdrs to resp...
}
```

### 21.2 Compression (`std::http::compression`)

Selects and applies the best compression algorithm based on the client's
`Accept-Encoding` header. Inspired by Express.js `compression` middleware.

```tml
pub const MIN_COMPRESS_SIZE: I64 = 1024   // minimum body size to compress

pub type CompressionOptions {
    min_size: I64,        // minimum body size (default: 1024 bytes)
    level: I64,           // compression level 1-9 (0 = default)
    prefer_order: Str,    // "br,gzip,deflate,zstd"
    filter_types: Str,    // MIME types to compress
}

impl CompressionOptions {
    pub func new() -> CompressionOptions
    pub func with_min_size(this, size: I64) -> CompressionOptions
    pub func with_level(this, lvl: I64) -> CompressionOptions
}

/// Selects the best compression encoding from Accept-Encoding.
/// Returns "" if no suitable encoding is found.
pub func select_encoding(accept_encoding: Str, opts: ref CompressionOptions) -> Str

/// Returns true if the content type and body size meet compression criteria.
pub func should_compress(content_type: Str, body_len: I64, opts: ref CompressionOptions) -> Bool

/// Compresses the body using the best supported encoding from Accept-Encoding.
/// Returns (compressed_body, encoding_name). encoding_name is "" if uncompressed.
pub func compress_response(body: Str, accept_encoding: Str, content_type: Str) -> (Str, Str)
```

```tml
use std::http::compression::compress_response

// In a request handler:
let accept_enc = req.get_header("accept-encoding")
let ct = "application/json"
let body = "{\"items\":[1,2,3]}"

let result = compress_response(body, accept_enc, ct)
let compressed_body = result.0
let encoding_name = result.1

resp.set_header("Content-Type", ct)
if str::len(encoding_name) > 0 {
    resp.set_header("Content-Encoding", encoding_name)
}
resp.end(compressed_body)
```

### 21.3 Security (`std::http::security`)

Generates security-related HTTP response headers to protect against common web
vulnerabilities. Equivalent to the Node.js `helmet` middleware.

```tml
pub type SecurityOptions {
    content_security_policy: Str,
    x_content_type_options: Bool,       // nosniff
    x_frame_options: Str,               // "DENY" or "SAMEORIGIN"
    x_xss_protection: Bool,
    strict_transport_security: Bool,
    hsts_max_age: I64,
    hsts_include_subdomains: Bool,
    hsts_preload: Bool,
    referrer_policy: Str,
    x_dns_prefetch_control: Bool,
    x_download_options: Bool,
    x_permitted_cross_domain: Bool,
    cross_origin_embedder_policy: Str,
    cross_origin_opener_policy: Str,
    cross_origin_resource_policy: Str,
}

impl SecurityOptions {
    pub func new() -> SecurityOptions        // helmet defaults
    pub func minimal() -> SecurityOptions    // only essential headers
    pub func with_csp(this, policy: Str) -> SecurityOptions
    pub func with_frame_options(this, value: Str) -> SecurityOptions
    pub func with_referrer_policy(this, policy: Str) -> SecurityOptions
}

/// Generates all configured security headers as a wire-format header block.
pub func security_headers(opts: ref SecurityOptions) -> Str
```

```tml
use std::http::security::{SecurityOptions, security_headers}

let sec = SecurityOptions::new()
    .with_csp("default-src 'self'; script-src 'self' cdn.example.com")
    .with_frame_options("DENY")

// In a request handler, apply security headers to every response:
let sec_hdrs = security_headers(ref sec)
// Parse sec_hdrs and call resp.set_header() for each line...
```

### 21.4 ETag (`std::http::etag`)

ETag generation and conditional request handling per RFC 7232. Uses FNV-1a hashing for
fast, deterministic entity tags. Inspired by the Express.js `etag` package.

```tml
/// Generates a strong ETag: "\"fnv1a-length\""
pub func generate(content: Str) -> Str

/// Generates a weak ETag: "W/\"fnv1a-length\""
pub func generate_weak(content: Str) -> Str

/// Returns true if If-None-Match matches the ETag (304 should be sent).
pub func is_not_modified(if_none_match: Str, etag: Str) -> Bool

/// Returns true if If-Modified-Since matches Last-Modified (304 should be sent).
pub func is_not_modified_since(if_modified_since: Str, last_modified: Str) -> Bool
```

```tml
use std::http::etag::{generate, is_not_modified}

let body = "{\"data\":\"response\"}"
let etag = generate(body)  // e.g. "\"cbf29ce484222325-18\""

let if_none_match = req.get_header("if-none-match")
if is_not_modified(if_none_match, etag) {
    var resp = ServerResponse::new()
    resp.set_status(304)
    resp.set_header("ETag", etag)
    resp.end_empty()
} else {
    var resp = ServerResponse::new()
    resp.set_header("Content-Type", "application/json")
    resp.set_header("ETag", etag)
    resp.end(body)
}
```

### 21.5 Body Parser (`std::http::body_parser`)

Parses request bodies by Content-Type. Inspired by Express.js `body-parser`.

```tml
pub const MAX_BODY_SIZE: I64 = 1048576   // 1 MB

pub type ParsedBody {
    Json(Str),
    Form(Str),
    Text(Str),
    Raw(Str),
    Empty,
}

impl ParsedBody {
    pub func as_str(this) -> Str
    pub func is_json(this) -> Bool
    pub func is_form(this) -> Bool
    pub func is_text(this) -> Bool
    pub func is_empty(this) -> Bool
}

/// Parses the body based on Content-Type. Returns ParsedBody variant.
pub func parse_body(content_type: Str, body: Str) -> Outcome[ParsedBody, HttpError]

/// Gets a value from URL-encoded form data by key.
/// Input: "name=John&age=30", key: "age" → "30"
pub func form_get(data: Str, key: Str) -> Str

/// Decodes percent-encoded strings (URL decoding). Converts '+' to space.
pub func url_decode(s: Str) -> Str
```

```tml
use std::http::body_parser::{parse_body, form_get, url_decode}

let ct = req.content_type()
let raw_body = req.body()

let body_result = parse_body(ct, raw_body)
when body_result {
    Ok(parsed) => {
        if parsed.is_json() {
            let json = parsed.as_str()
            print("JSON body: {json}\n")
        }
        if parsed.is_form() {
            let name = form_get(parsed.as_str(), "name")
            print("Name: {url_decode(name)}\n")
        }
    },
    Err(e) => print("Body parse error: {e.msg()}\n"),
}
```

### 21.6 Content-Type (`std::http::content_type`)

Content-Type header parsing, Accept header negotiation, and MIME type matching.
Inspired by Express.js `content-type` and `accepts` packages.

```tml
/// Parses a Content-Type header into (media_type, charset).
/// Input: "text/html; charset=utf-8" → ("text/html", "utf-8")
pub func parse_content_type(header: Str) -> (Str, Str)

/// Returns true if media_type matches pattern. Supports wildcards.
/// "text/*" matches "text/html"; "*/*" matches anything.
pub func matches_type(media_type: Str, pattern: Str) -> Bool

/// Negotiates the best Content-Type from an Accept header.
/// Returns "" if no acceptable type is found.
pub func negotiate(accept: Str, offered: Str) -> Str
```

### 21.7 Server-Sent Events (`std::http::stream`)

Server-Sent Events (SSE) message formatting per the W3C EventSource specification,
and streaming HTTP response helpers.

```tml
pub type SseEvent {
    event_type: Str,
    data: Str,
    id: Str,
    retry: I64,   // reconnection time in ms (0 = omit)
}

impl SseEvent {
    pub func data(data: Str) -> SseEvent
    pub func named(event_type: Str, data: Str) -> SseEvent
    pub func with_id(this, id: Str) -> SseEvent
    pub func with_retry(this, ms: I64) -> SseEvent
    pub func serialize(this) -> Str     // wire format: "event: ...\ndata: ...\n\n"
}

/// Returns the required SSE response headers (Content-Type: text/event-stream, etc.).
pub func sse_headers() -> Str

/// Returns a keep-alive SSE comment ping.
pub func sse_ping() -> Str

/// Returns an SSE comment line.
pub func sse_comment(text: Str) -> Str

/// Returns headers for a generic streaming response.
pub func streaming_headers(content_type: Str) -> Str

/// Returns headers for NDJSON streaming (application/x-ndjson).
pub func ndjson_headers() -> Str

/// Serializes a single NDJSON line.
pub func ndjson_line(json: Str) -> Str
```

```tml
use std::http::stream::{SseEvent, sse_headers}

// Start an SSE stream:
var resp = ServerResponse::new()
resp.set_status(200)
let hdrs = sse_headers()
// Apply sse_headers to resp...

// Send events:
let event = SseEvent::named("update", "{\"count\":42}")
    .with_id("evt-1")
    .with_retry(3000)
resp.write(event.serialize())

// Named event output:
// "event: update\ndata: {\"count\":42}\nid: evt-1\nretry: 3000\n\n"
```

### 21.8 Range Requests (`std::http::range`)

Range request parsing and Content-Range header generation per RFC 7233. Used for
partial content delivery (video streaming, resumable downloads).

```tml
pub type ByteRange {
    start: I64,
    end: I64,    // -1 means "to end of content"
}

impl ByteRange {
    pub func new(start: I64, end: I64) -> ByteRange
    pub func from_start(start: I64) -> ByteRange
    pub func suffix(n: I64) -> ByteRange
    pub func resolve(this, content_length: I64) -> (I64, I64)   // (actual_start, actual_end)
    pub func length(this, content_length: I64) -> I64
}

/// Parses a Range header: "bytes=0-499" or "bytes=500-" or "bytes=-500".
pub func parse_range(header: Str) -> Maybe[ByteRange]

/// Generates a Content-Range header value: "bytes 0-499/1000"
pub func content_range(start: I64, end: I64, total: I64) -> Str

/// Generates a 416 Content-Range value: "bytes */1000"
pub func content_range_unsatisfied(total: I64) -> Str

/// Returns true if the range is satisfiable given the content length.
pub func is_range_satisfiable(range: ref ByteRange, content_length: I64) -> Bool
```

```tml
use std::http::range::{parse_range, content_range, is_range_satisfiable}

let range_header = req.get_header("range")
let total_size: I64 = 102400  // total file size

when parse_range(range_header) {
    Nothing => {
        // No Range header: send full content with 200
        resp.end(full_content)
    },
    Just(range) => {
        if is_range_satisfiable(ref range, total_size) {
            let resolved = range.resolve(total_size)
            let start = resolved.0
            let end = resolved.1
            // Slice the content...
            resp.set_status(206)
            resp.set_header("Content-Range", content_range(start, end, total_size))
            resp.end(slice)
        } else {
            resp.set_status(416)
            resp.end_empty()
        }
    },
}
```

### 21.9 Cache-Control (`std::http::cache_control`)

Cache-Control, Expires, and related caching header generation per RFC 7234.
Inspired by Express.js caching middleware.

```tml
pub type CacheOptions {
    is_public: Bool,
    is_private: Bool,
    no_cache: Bool,
    no_store: Bool,
    no_transform: Bool,
    must_revalidate: Bool,
    proxy_revalidate: Bool,
    max_age: I64,                   // seconds (-1 = omit)
    s_maxage: I64,
    immutable: Bool,
    stale_while_revalidate: I64,
    stale_if_error: I64,
}

impl CacheOptions {
    pub func no_store() -> CacheOptions         // private, no-cache, no-store
    pub func short() -> CacheOptions            // public, max-age=300, must-revalidate
    pub func immutable_asset() -> CacheOptions  // public, max-age=31536000, immutable
    pub func public_max_age(seconds: I64) -> CacheOptions
    pub func private_max_age(seconds: I64) -> CacheOptions
}

/// Generates the Cache-Control header value string.
pub func cache_control_header(opts: ref CacheOptions) -> Str
```

```tml
use std::http::cache_control::{CacheOptions, cache_control_header}

// For API responses:
let cc = cache_control_header(ref CacheOptions::no_store())
// "private, no-cache, no-store"

// For fingerprinted static assets:
let cc = cache_control_header(ref CacheOptions::immutable_asset())
// "public, max-age=31536000, immutable"

resp.set_header("Cache-Control", cc)
```

### 21.10 Rate Limiting (`std::http::rate_limit`)

In-memory token bucket rate limiter. Inspired by Express.js `express-rate-limit`.

```tml
pub type RateLimitOptions {
    window_ms: I64,
    max_requests: I64,
}

impl RateLimitOptions {
    pub func new() -> RateLimitOptions            // default: 100 req / 15 min
    pub func with_window(this, ms: I64) -> RateLimitOptions
    pub func with_max(this, max: I64) -> RateLimitOptions
}

pub type RateLimitResult {
    allowed: Bool,
    remaining: I64,
    limit: I64,
    reset_at: I64,
}

impl RateLimitResult {
    /// Returns rate limit response headers as a wire-format string.
    pub func headers(this) -> Str
}

pub type RateLimiter {
    handle: I64,
}

impl RateLimiter {
    pub func new(opts: RateLimitOptions) -> RateLimiter
    pub func check(this, key: Str, now_ms: I64) -> RateLimitResult
    pub func destroy(this)
}
```

```tml
use std::http::rate_limit::{RateLimiter, RateLimitOptions}

let opts = RateLimitOptions::new()
    .with_window(60000)   // 1 minute
    .with_max(60)         // 60 requests per minute

let limiter = RateLimiter::new(opts)

// In a request handler:
let client_ip = req.get_header("x-forwarded-for")
let result = limiter.check(client_ip, now_ms())

if not result.allowed {
    var resp = ServerResponse::new()
    resp.set_status(429)
    let rl_hdrs = result.headers()
    // Apply rl_hdrs...
    resp.end("Too Many Requests")
}
// Otherwise continue handling the request
```

### 21.11 Static File Server (`std::http::static_server`)

Static file serving utilities with MIME detection, ETag generation, and
Cache-Control. Inspired by Express.js `serve-static` and `send`.

```tml
pub type StaticOptions {
    root: Str,
    index_file: Str,
    dot_files: Str,         // "ignore", "allow", "deny"
    etag_enabled: Bool,
    last_modified: Bool,
    max_age: I64,
    immutable: Bool,
    extensions: Str,        // fallback extensions: "html,htm"
}

impl StaticOptions {
    pub func new(root: Str) -> StaticOptions
    pub func with_index(this, index: Str) -> StaticOptions
    pub func with_max_age(this, seconds: I64) -> StaticOptions
    pub func with_immutable(this) -> StaticOptions
    pub func with_extensions(this, exts: Str) -> StaticOptions
}

/// Resolves a URL path to a filesystem path under root.
/// Returns "" if the path is invalid (directory traversal attempt).
pub func resolve_path(root: Str, url_path: Str) -> Str

/// Extracts the file extension from a path.
pub func get_extension(path: Str) -> Str

/// Returns the MIME type for a file extension.
pub func mime_for_extension(ext: Str) -> Str

/// Generates response headers for a static file (ETag, Cache-Control, Content-Type, etc.).
pub func static_file_headers(file_path: Str, content: Str, opts: ref StaticOptions) -> Str
```

```tml
use std::http::static_server::{StaticOptions, resolve_path, mime_for_extension, static_file_headers}

let opts = StaticOptions::new("/var/www/public")
    .with_max_age(3600)
    .with_extensions("html,htm")

// In a request handler:
let url_path = req.path()
let file_path = resolve_path("/var/www/public", url_path)

if str::len(file_path) == 0 {
    // Directory traversal attempt or invalid path
    resp.set_status(403)
    resp.end_empty()
} else {
    // Read file_content from disk...
    let ext = get_extension(file_path)
    let mime = mime_for_extension(ext)
    let file_hdrs = static_file_headers(file_path, file_content, ref opts)
    // Apply file_hdrs to resp, set Content-Type: mime, send file_content
}
```

## 22. Complete Server Example

This example demonstrates a minimal HTTP API server using the complete server-side stack.

```tml
use std::http::{HttpServer, IncomingMessage, ServerResponse}
use std::http::router::{Router, RouteMatch}
use std::http::body_parser::{parse_body, form_get}
use std::http::cors::{CorsOptions, cors_headers}
use std::http::security::{SecurityOptions, security_headers}
use std::http::etag::{generate, is_not_modified}

func main() {
    var server = HttpServer::new()
    let bind_result = server.listen(3000)
    when bind_result {
        Err(e) => {
            print("Failed to start: {e.msg()}\n")
            return
        },
        Ok(_) => print("API server listening on port 3000\n"),
    }

    let cors = CorsOptions::new().with_origin("https://app.example.com")
    let sec = SecurityOptions::new()

    let router = Router::new()
    router.on("GET", "/api/users", 1)
    router.on("GET", "/api/users/:id", 2)
    router.on("POST", "/api/users", 3)

    loop (true) {
        let accept_result = server.accept()
        when accept_result {
            Err(e) => {
                print("Accept error: {e.msg()}\n")
                break
            },
            Ok(pair) => {
                let req = pair.0
                var resp = pair.1

                // Apply security headers
                // security_headers(ref sec) returns a header block string

                // Route the request
                let m = router.find(req.method(), req.path())

                if not m.found {
                    resp.set_status(404)
                    resp.set_header("Content-Type", "application/json")
                    resp.end("{\"error\":\"not found\"}")
                } else {
                    when m.handler_id {
                        1 => {
                            let body = "[{\"id\":1,\"name\":\"Alice\"}]"
                            let etag = generate(body)
                            let inm = req.get_header("if-none-match")
                            if is_not_modified(inm, etag) {
                                resp.set_status(304)
                                resp.set_header("ETag", etag)
                                resp.end_empty()
                            } else {
                                resp.set_header("Content-Type", "application/json")
                                resp.set_header("ETag", etag)
                                resp.end(body)
                            }
                        },
                        2 => {
                            let id = m.get_param("id")
                            resp.set_header("Content-Type", "application/json")
                            resp.end("{\"id\":{id},\"name\":\"Alice\"}")
                        },
                        3 => {
                            let parsed_result = parse_body(req.content_type(), req.body())
                            when parsed_result {
                                Ok(parsed) => {
                                    resp.set_status(201)
                                    resp.set_header("Content-Type", "application/json")
                                    resp.end("{\"created\":true}")
                                },
                                Err(e) => {
                                    resp.set_status(400)
                                    resp.end("{\"error\":\"invalid body\"}")
                                },
                            }
                        },
                        _ => {
                            resp.set_status(500)
                            resp.end_empty()
                        },
                    }
                }

                server.send_response(ref resp)
                m.destroy()
            },
        }
    }

    router.destroy()
    server.close()
}
```

## 23. Complete Client Examples

### 23.1 Simple GET Request

```tml
use std::http::{HttpClient, Response, HttpError}

func main() {
    let client = HttpClient::new()
    let result = client.get("https://api.example.com/users")
    when result {
        Ok(resp) => {
            print("Status: {resp.status().code()} {resp.status().reason()}\n")
            print("Content-Type: {resp.content_type()}\n")
            print("Body: {resp.text()}\n")
        },
        Err(e) => print("Request failed: {e.msg()}\n"),
    }
}
```

### 23.2 POST Request with JSON Body

```tml
use std::http::{HttpClient, Response, HttpError}

func main() {
    let client = HttpClient::new()
    let json = "{\"name\":\"Alice\",\"email\":\"alice@example.com\"}"
    let result = client.post_json("https://api.example.com/users", json)
    when result {
        Ok(resp) => {
            if resp.status().code() == 201 {
                print("User created: {resp.text()}\n")
            }
        },
        Err(e) => print("Failed: {e.msg()}\n"),
    }
}
```

### 23.3 Custom Request with Auth

```tml
use std::http::{HttpClient, Request, Response, HttpError}

func fetch_with_auth(client: HttpClient, url: Str, token: Str) -> Outcome[Response, HttpError] {
    let req = Request::get(url)
        .header("Authorization", "Bearer {token}")
        .header("Accept", "application/json")
        .timeout(5000)
    return client.send(req)
}

func main() {
    let client = HttpClient::with_user_agent("my-app/2.0")
    let result = fetch_with_auth(client, "https://api.example.com/me", "my-secret-token")
    when result {
        Ok(resp) => {
            when resp.status().code() {
                200 => print("Profile: {resp.text()}\n"),
                401 => print("Authentication failed\n"),
                403 => print("Access denied\n"),
                _ => print("Status: {resp.status().code()}\n"),
            }
        },
        Err(e) => print("Error: {e.msg()}\n"),
    }
}
```

### 23.4 Inspecting Response Headers

```tml
use std::http::HttpClient

func main() {
    let client = HttpClient::new()
    let result = client.head("https://example.com/large-file.zip")
    when result {
        Ok(resp) => {
            let headers = resp.headers()
            let size = resp.content_length()
            print("Content-Type: {resp.content_type()}\n")
            if size >= 0 {
                print("Content-Length: {size} bytes\n")
            }
            if headers.has("x-ratelimit-remaining") {
                print("Rate limit remaining: {headers.get(\"x-ratelimit-remaining\")}\n")
            }
        },
        Err(e) => print("HEAD request failed: {e.msg()}\n"),
    }
}
```

## 24. [NOT YET IMPLEMENTED] HTTP/2

> **This section describes planned functionality that is not yet implemented.**

HTTP/2 support will include binary framing, multiplexed streams, HPACK header compression,
server push, and flow control. The `HttpVersion::HTTP_2` variant already exists but the
client currently communicates using HTTP/1.1 wire format only.

## 25. [NOT YET IMPLEMENTED] WebSocket

> **This section describes planned functionality that is not yet implemented.**

WebSocket support will include the HTTP Upgrade handshake, frame encoding/decoding
(text, binary, ping, pong, close), and an event-driven message API.

---

*Previous: [06-TLS.md](./06-TLS.md)*
*Next: [08-COMPRESS.md](./08-COMPRESS.md) -- Compression Algorithms*
