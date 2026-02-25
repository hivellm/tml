# std::http -- HTTP Client, Routing, and Protocol Types

## 1. Overview

The `std::http` module provides HTTP/1.1 client functionality, URL routing, and shared
protocol types (methods, status codes, headers, cookies, error types). The module is
organized so that shared protocol types are separate from client/server logic and can
be reused by both.

```tml
use std::http                              // all re-exports
use std::http::{HttpClient, Request, Response}
use std::http::{Method, Status, Headers}
use std::http::{Router, RouteMatch}
use std::http::{Cookie, HttpError, HttpErrorKind}
use std::http::{Connection, ConnectionInfo}
use std::http::{HttpVersion}
use std::http::encoding::{compress, decompress, accepted_encodings}
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
| Server | -- | NOT YET IMPLEMENTED | HTTP server, listeners, connection handling |
| Middleware | -- | NOT YET IMPLEMENTED | Middleware chain, CORS, logging, rate limiting |
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
  +-- connection       Client: DNS + TCP + TLS connection
  +-- request          Client: Request builder and serializer
  +-- response         Client: Response parser
  +-- client           Client: HttpClient (send/receive)
  +-- router           Server: Radix tree URL router
  +-- multipart        Shared: Multipart/form-data builder
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
    url_scheme: I64,     // parsed URL components stored as I64
    url_host: I64,       // to escape @allocates tracking
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
    /// Creates a request with the given method and URL.
    pub func new(method: Method, url: Str) -> Request

    /// Convenience constructors:
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

All builder methods return `Request` for fluent chaining.

```tml
impl Request {
    /// Sets a request header (case-insensitive key).
    pub func header(this, key: Str, value: Str) -> Request

    /// Sets the body and auto-sets Content-Length.
    pub func body(this, data: Str) -> Request

    /// Sets the body as JSON (auto-sets Content-Type: application/json).
    pub func json(this, data: Str) -> Request

    /// Sets the body as form data (auto-sets Content-Type: application/x-www-form-urlencoded).
    pub func form(this, data: Str) -> Request

    /// Sets the HTTP version (default: HTTP/1.1).
    pub func version(this, v: HttpVersion) -> Request

    /// Sets the request timeout in milliseconds.
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

    // URL component accessors
    pub func scheme(this) -> Str        // "http" or "https"
    pub func host(this) -> Str          // e.g. "example.com"
    pub func port(this) -> I64          // -1 if not specified
    pub func path(this) -> Str          // e.g. "/api/users"
    pub func query(this) -> Str         // query string without "?"
    pub func path_and_query(this) -> Str
    pub func host_header(this) -> Str   // host with port if non-default

    // Method helpers
    pub func has_body(this) -> Bool
    pub func is_idempotent(this) -> Bool
    pub func is_safe(this) -> Bool
    pub func is_secure(this) -> Bool

    // Header convenience
    pub func content_type(this) -> Str
    pub func content_length(this) -> I64

    // Serialization
    pub func serialize(this) -> Str     // HTTP/1.1 wire format

    // Cleanup
    pub func destroy(this)
}
```

### 5.4 Builder Example

```tml
let req = Request::post("https://api.example.com/items")
    .header("Authorization", "Bearer my-token")
    .header("Accept", "application/json")
    .json("{\"name\":\"widget\",\"price\":9.99}")
    .timeout(5000)

// req.method()       -> Method::POST
// req.host()         -> "api.example.com"
// req.path()         -> "/items"
// req.content_type() -> "application/json"
// req.serialize()    -> full HTTP/1.1 wire format string
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
    /// Creates a response manually (for testing or server-side construction).
    pub func new(status: Status, headers: Headers, body: Str) -> Response

    /// Parses an HTTP/1.1 response from raw wire format.
    /// Expects: "HTTP/1.1 200 OK\r\nHeader: Value\r\n\r\nbody"
    pub func parse(raw: Str) -> Outcome[Response, HttpError]
}
```

### 6.2 Accessors

```tml
impl Response {
    pub func status(this) -> Status
    pub func headers(this) -> ref Headers
    pub func text(this) -> Str          // body as a string (returns a copy)
    pub func version(this) -> HttpVersion
    pub func content_length(this) -> I64
    pub func content_type(this) -> Str

    // Status category checks
    pub func is_ok(this) -> Bool           // 2xx
    pub func is_redirect(this) -> Bool     // 3xx
    pub func is_client_error(this) -> Bool // 4xx
    pub func is_server_error(this) -> Bool // 5xx

    // Cleanup
    pub func destroy(this)
}
```

### 6.3 Usage Example

```tml
let client = HttpClient::new()
let result = client.get("https://example.com")
when result {
    Ok(resp) => {
        let status = resp.status()
        print("Code: {status.code()}\n")
        print("Reason: {status.reason()}\n")
        print("Content-Type: {resp.content_type()}\n")
        print("Body: {resp.text()}\n")

        if resp.is_ok() {
            print("Request succeeded\n")
        }
    },
    Err(e) => print("Error: {e.msg()}\n"),
}
```

## 7. Method

An HTTP request method, defined as a variant type.

```tml
pub type Method {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS,
    TRACE,
    CONNECT,
}
```

### 7.1 Methods

```tml
impl Method {
    pub func to_string(this) -> Str          // "GET", "POST", etc.
    pub func from_string(s: Str) -> Method   // parses; returns GET for unrecognized

    pub func has_body(this) -> Bool          // true for POST, PUT, PATCH
    pub func is_idempotent(this) -> Bool     // true for GET, PUT, DELETE, HEAD, OPTIONS, TRACE
    pub func is_safe(this) -> Bool           // true for GET, HEAD, OPTIONS, TRACE
}
```

## 8. Status

An HTTP response status code with the full IANA registry (100-511) and category classification.

```tml
pub type Status {
    code_value: I32,
}
```

### 8.1 Constructors

```tml
impl Status {
    pub func new(code: I32) -> Status
    pub func code(this) -> I32
}
```

### 8.2 Named Status Codes

Each returns a `Status` value:

| 1xx Informational | 2xx Success | 3xx Redirection |
|---|---|---|
| `CONTINUE()` (100) | `OK()` (200) | `MULTIPLE_CHOICES()` (300) |
| `SWITCHING_PROTOCOLS()` (101) | `CREATED()` (201) | `MOVED_PERMANENTLY()` (301) |
| `PROCESSING()` (102) | `ACCEPTED()` (202) | `FOUND()` (302) |
| `EARLY_HINTS()` (103) | `NON_AUTHORITATIVE_INFORMATION()` (203) | `SEE_OTHER()` (303) |
| | `NO_CONTENT()` (204) | `NOT_MODIFIED()` (304) |
| | `RESET_CONTENT()` (205) | `USE_PROXY()` (305) |
| | `PARTIAL_CONTENT()` (206) | `TEMPORARY_REDIRECT()` (307) |
| | `MULTI_STATUS()` (207) | `PERMANENT_REDIRECT()` (308) |
| | `ALREADY_REPORTED()` (208) | |
| | `IM_USED()` (226) | |

| 4xx Client Errors | 5xx Server Errors |
|---|---|
| `BAD_REQUEST()` (400) | `INTERNAL_SERVER_ERROR()` (500) |
| `UNAUTHORIZED()` (401) | `NOT_IMPLEMENTED()` (501) |
| `PAYMENT_REQUIRED()` (402) | `BAD_GATEWAY()` (502) |
| `FORBIDDEN()` (403) | `SERVICE_UNAVAILABLE()` (503) |
| `NOT_FOUND()` (404) | `GATEWAY_TIMEOUT()` (504) |
| `METHOD_NOT_ALLOWED()` (405) | `HTTP_VERSION_NOT_SUPPORTED()` (505) |
| `NOT_ACCEPTABLE()` (406) | `VARIANT_ALSO_NEGOTIATES()` (506) |
| `PROXY_AUTHENTICATION_REQUIRED()` (407) | `INSUFFICIENT_STORAGE()` (507) |
| `REQUEST_TIMEOUT()` (408) | `LOOP_DETECTED()` (508) |
| `CONFLICT()` (409) | `BANDWIDTH_LIMIT_EXCEEDED()` (509) |
| `GONE()` (410) | `NOT_EXTENDED()` (510) |
| `LENGTH_REQUIRED()` (411) | `NETWORK_AUTHENTICATION_REQUIRED()` (511) |
| `PRECONDITION_FAILED()` (412) | |
| `PAYLOAD_TOO_LARGE()` (413) | |
| `URI_TOO_LONG()` (414) | |
| `UNSUPPORTED_MEDIA_TYPE()` (415) | |
| `RANGE_NOT_SATISFIABLE()` (416) | |
| `EXPECTATION_FAILED()` (417) | |
| `IM_A_TEAPOT()` (418) | |
| `MISDIRECTED_REQUEST()` (421) | |
| `UNPROCESSABLE_ENTITY()` (422) | |
| `LOCKED()` (423) | |
| `FAILED_DEPENDENCY()` (424) | |
| `TOO_EARLY()` (425) | |
| `UPGRADE_REQUIRED()` (426) | |
| `PRECONDITION_REQUIRED()` (428) | |
| `TOO_MANY_REQUESTS()` (429) | |
| `REQUEST_HEADER_FIELDS_TOO_LARGE()` (431) | |
| `UNAVAILABLE_FOR_LEGAL_REASONS()` (451) | |

### 8.3 Category Checks

```tml
impl Status {
    pub func is_informational(this) -> Bool     // 1xx
    pub func is_success(this) -> Bool           // 2xx
    pub func is_redirect(this) -> Bool          // 3xx
    pub func is_client_error(this) -> Bool      // 4xx
    pub func is_server_error(this) -> Bool      // 5xx
    pub func is_redirect_with_body(this) -> Bool // 307, 308
    pub func is_empty(this) -> Bool             // 204, 205, 304
    pub func is_retry(this) -> Bool             // 502, 503, 504
    pub func reason(this) -> Str                // IANA reason phrase
}
```

## 9. Headers

A case-insensitive HTTP header map. Stores headers in parallel arrays (keys, values) with
linear scan. Keys are lowercased internally. This is intentionally NOT a HashMap -- HTTP
messages typically have fewer than 30 headers, and linear scan avoids the pointer-equality
limitation with `Str` keys.

```tml
pub type Headers {
    handle: *Unit    // opaque pointer to internal parallel arrays
}
```

### 9.1 Core Methods

```tml
impl Headers {
    pub func new() -> Headers

    /// Sets a header, replacing any existing value for that key.
    pub func set(this, key: Str, value: Str)

    /// Gets the value for a header key, or "" if not present.
    pub func get(this, key: Str) -> Str

    /// Returns true if the header exists.
    pub func has(this, key: Str) -> Bool

    /// Removes a header by key.
    pub func remove(this, key: Str)

    /// Appends a value (comma-separated per RFC 7230). If key does not
    /// exist, behaves like set().
    pub func append(this, key: Str, value: Str)

    /// Serializes all headers to "Key: Value\r\n" wire format.
    pub func serialize(this) -> Str

    pub func len(this) -> I64
    pub func is_empty(this) -> Bool
    pub func destroy(this)
}
```

### 9.2 Convenience Accessors

```tml
impl Headers {
    pub func content_length(this) -> I64    // -1 if not set
    pub func content_type(this) -> Str      // "" if not set
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

### 9.3 Usage Example

```tml
let h = Headers::new()
h.set("Content-Type", "application/json")   // stored as "content-type"
h.set("Authorization", "Bearer abc123")

let ct = h.get("content-type")    // "application/json"
let ct2 = h.get("Content-Type")   // same -- case-insensitive

h.append("Accept", "text/html")
h.append("Accept", "application/json")
let accept = h.get("accept")      // "text/html, application/json"

h.remove("Authorization")
let has_auth = h.has("Authorization")  // false

h.destroy()
```

## 10. HttpVersion

HTTP protocol version, defined as a variant type.

```tml
pub type HttpVersion {
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2,
    HTTP_3,
}
```

```tml
impl HttpVersion {
    pub func to_string(this) -> Str              // "HTTP/1.0", "HTTP/1.1", etc.
    pub func from_string(s: Str) -> HttpVersion  // parses; defaults to HTTP_1_1
    pub func is_h2_or_later(this) -> Bool        // true for HTTP_2, HTTP_3
}
```

## 11. Router

A high-performance URL router using a compressed radix tree (one tree per HTTP method).
Inspired by `find-my-way`. Supports static routes, path parameters (`:param`), and
wildcard catch-all routes (`*path`).

Priority: static > parametric > wildcard.

```tml
pub type Router {
    methods: HashMap[Str, I64]    // method name -> radix tree root pointer
}

pub type RouteMatch {
    found: Bool,
    handler_id: I64,
    param_names: List[Str],
    param_values: List[Str],
}
```

### 11.1 Router Methods

```tml
impl Router {
    /// Creates a new empty router.
    pub func new() -> Router

    /// Registers a route handler.
    /// method: HTTP method string ("GET", "POST", etc.)
    /// path: URL pattern with optional parameters
    /// handler_id: integer ID for the handler
    pub func on(this, method: Str, path: Str, handler_id: I64)

    /// Looks up a route by method and path. Returns a RouteMatch.
    pub func find(this, method: Str, path: Str) -> RouteMatch

    /// Frees all internal radix tree nodes.
    pub func destroy(this)
}
```

### 11.2 RouteMatch Methods

```tml
impl RouteMatch {
    /// Gets a parameter value by name. Returns "" if not found.
    pub func get_param(this, name: Str) -> Str

    /// Returns the number of extracted parameters.
    pub func param_count(this) -> I64

    pub func destroy(this)
}
```

### 11.3 Route Patterns

| Pattern | Example Path | Matches | Parameters |
|---------|-------------|---------|------------|
| `/users` | `/users` | Exact static match | none |
| `/users/:id` | `/users/42` | Parametric segment | `id = "42"` |
| `/users/:id/posts` | `/users/42/posts` | Mixed static + param | `id = "42"` |
| `/files/*filepath` | `/files/css/style.css` | Wildcard catch-all | `filepath = "css/style.css"` |
| `/` | `/` | Root route | none |

### 11.4 Usage Example

```tml
use std::http::router::{Router, RouteMatch}

let router = Router::new()
router.on("GET", "/", 1)
router.on("GET", "/users", 2)
router.on("GET", "/users/:id", 3)
router.on("GET", "/users/:id/posts", 4)
router.on("POST", "/users", 5)
router.on("GET", "/files/*filepath", 6)

// Static match
let m = router.find("GET", "/users")
// m.found == true, m.handler_id == 2

// Parametric match
let m = router.find("GET", "/users/42/posts")
// m.found == true, m.handler_id == 4
// m.get_param("id") == "42"

// Wildcard match
let m = router.find("GET", "/files/css/style.css")
// m.found == true, m.handler_id == 6
// m.get_param("filepath") == "css/style.css"

// No match
let m = router.find("DELETE", "/users/42")
// m.found == false

router.destroy()
```

## 12. Cookie

HTTP cookie builder and parser per RFC 6265.

```tml
pub type Cookie {
    name: Str,
    value: Str,
    path: Str,         // default: "/"
    domain: Str,       // default: ""
    max_age: I64,      // default: -1 (session)
    http_only: Bool,   // default: false
    secure: Bool,      // default: false
    same_site: Str,    // default: ""
}
```

### 12.1 Constructor and Builder

```tml
impl Cookie {
    /// Creates a cookie with name and value (defaults for all attributes).
    pub func new(name: Str, value: Str) -> Cookie

    /// Builder methods (return a new Cookie for chaining):
    pub func with_path(this, path: Str) -> Cookie
    pub func with_domain(this, domain: Str) -> Cookie
    pub func with_max_age(this, seconds: I64) -> Cookie
    pub func with_http_only(this) -> Cookie
    pub func with_secure(this) -> Cookie
    pub func with_same_site(this, policy: Str) -> Cookie

    /// Serializes to Set-Cookie header value.
    pub func to_set_cookie(this) -> Str
}
```

### 12.2 Cookie Parser

```tml
/// Parses a Cookie request header ("key=val; key2=val2") and returns
/// the value for the given cookie name, or "" if not found.
pub func parse_cookie(header: Str, name: Str) -> Str
```

### 12.3 Usage Example

```tml
use std::http::cookie::{Cookie, parse_cookie}

// Build a Set-Cookie header
let cookie = Cookie::new("session", "abc123")
    .with_path("/")
    .with_domain("example.com")
    .with_max_age(3600)
    .with_http_only()
    .with_secure()
    .with_same_site("Strict")

let header = cookie.to_set_cookie()
// "session=abc123; Path=/; Domain=example.com; Max-Age=3600; HttpOnly; Secure; SameSite=Strict"

// Parse a Cookie request header
let value = parse_cookie("session=abc123; theme=dark", "theme")
// value == "dark"
```

## 13. HttpError and HttpErrorKind

Error types for all HTTP client/server error conditions.

```tml
pub type HttpErrorKind {
    value: I32,
}

pub type HttpError {
    error_kind: HttpErrorKind,
    message: Str,
}

/// Result alias for HTTP operations.
pub type HttpResult[T] = Outcome[T, HttpError]
```

### 13.1 Error Kinds

```tml
impl HttpErrorKind {
    pub func InvalidUrl() -> HttpErrorKind          // 1 - malformed URL
    pub func DnsFailure() -> HttpErrorKind          // 2 - DNS resolution failed
    pub func ConnectionFailed() -> HttpErrorKind    // 3 - TCP connect failed
    pub func TlsError() -> HttpErrorKind            // 4 - TLS handshake/cert error
    pub func Timeout() -> HttpErrorKind             // 5 - operation timed out
    pub func InvalidResponse() -> HttpErrorKind     // 6 - response parse failed
    pub func TooManyRedirects() -> HttpErrorKind    // 7 - redirect limit exceeded
    pub func BodyTooLarge() -> HttpErrorKind        // 8 - response body too large
    pub func EncodingError() -> HttpErrorKind       // 9 - Content-Encoding error
    pub func ProtocolError() -> HttpErrorKind       // 10 - HTTP protocol violation
    pub func AlreadySent() -> HttpErrorKind         // 11 - response already sent
    pub func InvalidStatusCode() -> HttpErrorKind   // 12 - invalid status code
    pub func InvalidHeader() -> HttpErrorKind       // 13 - invalid header
    pub func InvalidPayload() -> HttpErrorKind      // 14 - invalid payload
    pub func RouteError() -> HttpErrorKind          // 15 - route handler error
    pub func HookError() -> HttpErrorKind           // 16 - hook error

    pub func raw(this) -> I32
}
```

### 13.2 HttpError Methods

```tml
impl HttpError {
    pub func new(kind: HttpErrorKind, msg: Str) -> HttpError
    pub func kind(this) -> HttpErrorKind
    pub func msg(this) -> Str

    // Convenience constructors:
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

## 14. Connection

Low-level connection layer. Handles DNS resolution, TCP connection, and optional TLS
handshake. Used internally by `HttpClient` but also available for direct use.

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
```

### 14.1 Connection Methods

```tml
impl Connection {
    /// Opens a connection. Uses TLS if is_https is true.
    pub func open(host: Str, port: I64, is_https: Bool) -> Outcome[Connection, HttpError]

    /// Writes a string to the connection (TLS or plain TCP).
    pub func write_str(this, data: Str) -> Outcome[I64, HttpError]

    /// Reads bytes into a buffer.
    pub func read(this, buf: mut ref [U8]) -> Outcome[I64, HttpError]

    pub func connection_info(this) -> ConnectionInfo
    pub func is_encrypted(this) -> Bool
    pub func get_fd(this) -> I64
    pub func close(this)
}

impl ConnectionInfo {
    pub func none() -> ConnectionInfo
    pub func get_tls_version(this) -> Str
    pub func get_cipher(this) -> Str
    pub func is_peer_verified(this) -> Bool
}
```

## 15. Content-Encoding

The `std::http::encoding` module dispatches compression and decompression to the
appropriate algorithm based on the Content-Encoding header value.

Supported encodings: `gzip`, `x-gzip`, `deflate`, `br` (Brotli), `zstd`.

```tml
/// Decompresses data based on Content-Encoding name.
pub func decompress(encoding: Str, data: Str) -> Outcome[Str, HttpError]

/// Compresses data with the specified encoding.
pub func compress(encoding: Str, data: Str) -> Outcome[Str, HttpError]

/// Returns the Accept-Encoding value for all supported encodings.
pub func accepted_encodings() -> Str   // "gzip, deflate, br, zstd"
```

## 16. Multipart

The `std::http::multipart` module builds multipart/form-data request bodies per RFC 2046.

```tml
pub type MultipartPart {
    name: Str,
    filename: Str,
    content_type: Str,
    data: Str,
}

pub type MultipartBuilder {
    boundary: Str,
    parts: List[MultipartPart],
}
```

### 16.1 MultipartBuilder Methods

```tml
impl MultipartBuilder {
    /// Creates a builder with a default boundary string.
    pub func new() -> MultipartBuilder

    /// Creates a builder with a custom boundary.
    pub func with_boundary(boundary: Str) -> MultipartBuilder

    /// Adds a text field.
    pub func add_field(this, name: Str, value: Str)

    /// Adds a file part with filename and content type.
    pub func add_file(this, name: Str, filename: Str, content_type: Str, data: Str)

    /// Returns the Content-Type header value including boundary.
    pub func content_type(this) -> Str

    /// Builds the serialized multipart body.
    pub func build(this) -> Str

    pub func destroy(this)
}
```

### 16.2 Usage Example

```tml
use std::http::multipart::MultipartBuilder

let mp = MultipartBuilder::new()
mp.add_field("username", "alice")
mp.add_file("avatar", "photo.jpg", "image/jpeg", file_data)

let body = mp.build()
let ct = mp.content_type()  // "multipart/form-data; boundary=----TMLBoundary7ma4d9abcdef"

let req = Request::post("https://example.com/upload")
    .header("Content-Type", ct)
    .body(body)

mp.destroy()
```

## 17. Complete Usage Examples

This section demonstrates end-to-end HTTP workflows using the implemented API.

### 17.1 Simple GET Request

```tml
use std::http::{HttpClient, Response, HttpError}

func main() {
    let client = HttpClient::new()

    let result = client.get("https://api.example.com/users")
    when result {
        Ok(resp) => {
            let status = resp.status()
            print("Status: {status.code()} {status.reason()}\n")
            print("Content-Type: {resp.content_type()}\n")
            print("Body: {resp.text()}\n")

            if resp.is_ok() {
                print("Request succeeded\n")
            }
            if resp.is_client_error() {
                print("Client error (4xx)\n")
            }
            if resp.is_server_error() {
                print("Server error (5xx)\n")
            }
        },
        Err(e) => {
            print("Request failed: {e.msg()}\n")
        },
    }
}
```

### 17.2 POST Request with JSON Body

```tml
use std::http::{HttpClient, Response, HttpError}

func create_user(client: HttpClient, name: Str, email: Str) -> Outcome[Response, HttpError] {
    let json = "{\"name\":\"{name}\",\"email\":\"{email}\"}"
    return client.post_json("https://api.example.com/users", json)
}

func main() {
    let client = HttpClient::new()

    let result = create_user(client, "Alice", "alice@example.com")
    when result {
        Ok(resp) => {
            if resp.status().code() == 201 {
                print("User created successfully\n")
                print("Response: {resp.text()}\n")
            } else {
                print("Unexpected status: {resp.status().code()}\n")
            }
        },
        Err(e) => print("Failed to create user: {e.msg()}\n"),
    }
}
```

### 17.3 Custom Request with Headers and Error Handling

```tml
use std::http::{HttpClient, Request, Response, HttpError}

func fetch_with_auth(client: HttpClient, url: Str, token: Str) -> Outcome[Response, HttpError] {
    let req = Request::get(url)
        .header("Authorization", "Bearer {token}")
        .header("Accept", "application/json")
        .header("X-Request-Id", "req-12345")
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
                401 => print("Authentication failed -- check your token\n"),
                403 => print("Access denied\n"),
                404 => print("Resource not found\n"),
                _ => print("Unexpected status: {resp.status().code()} {resp.status().reason()}\n"),
            }
        },
        Err(e) => print("Request error: {e.msg()}\n"),
    }
}
```

### 17.4 PUT and DELETE Requests

```tml
use std::http::{HttpClient, Request}

func main() {
    let client = HttpClient::new()

    // Update a resource with PUT
    let update_body = "{\"name\":\"Alice Updated\",\"email\":\"alice-new@example.com\"}"
    let put_result = client.put("https://api.example.com/users/42", update_body)
    when put_result {
        Ok(resp) => print("PUT status: {resp.status().code()}\n"),
        Err(e) => print("PUT failed: {e.msg()}\n"),
    }

    // Delete a resource
    let del_result = client.delete("https://api.example.com/users/42")
    when del_result {
        Ok(resp) => {
            if resp.status().code() == 204 {
                print("User deleted successfully\n")
            }
        },
        Err(e) => print("DELETE failed: {e.msg()}\n"),
    }
}
```

### 17.5 Custom POST with Form Data

```tml
use std::http::{HttpClient, Request}

func main() {
    let client = HttpClient::new()

    let req = Request::post("https://api.example.com/login")
        .form("username=alice&password=secret123")
        .header("Accept", "application/json")

    let result = client.send(req)
    when result {
        Ok(resp) => {
            if resp.is_ok() {
                print("Login succeeded\n")
                // Read the auth token from response
                print("Token: {resp.text()}\n")
            } else {
                print("Login failed: {resp.status().reason()}\n")
            }
        },
        Err(e) => print("Connection error: {e.msg()}\n"),
    }
}
```

### 17.6 Inspecting Response Headers

```tml
use std::http::{HttpClient, Headers}

func main() {
    let client = HttpClient::new()

    let result = client.head("https://example.com/large-file.zip")
    when result {
        Ok(resp) => {
            let headers = resp.headers()
            let size = resp.content_length()
            let ct = resp.content_type()

            print("Content-Type: {ct}\n")
            if size >= 0 {
                print("Content-Length: {size} bytes\n")
            }

            // Check for custom headers
            if headers.has("x-ratelimit-remaining") {
                print("Rate limit remaining: {headers.get(\"x-ratelimit-remaining\")}\n")
            }
        },
        Err(e) => print("HEAD request failed: {e.msg()}\n"),
    }
}
```

## 18. [NOT YET IMPLEMENTED] HTTP Server

> **This section describes planned functionality that is not yet implemented.**

The HTTP server will provide listener binding, connection handling, and request dispatch.

```tml
// Planned API:
pub type Server { ... }

impl Server {
    pub func bind(addr: Str) -> Outcome[Server, HttpError]
    pub func bind_tls(addr: Str, cert: Str, key: Str) -> Outcome[Server, HttpError]
    pub func serve(this, router: Router) -> Outcome[Unit, HttpError]
    pub func max_connections(this, n: I64) -> Server
    pub func keep_alive(this, ms: I64) -> Server
}
```

## 19. [NOT YET IMPLEMENTED] Middleware

> **This section describes planned functionality that is not yet implemented.**

Middleware will provide composable request/response processing layers.

```tml
// Planned API:
pub behavior Middleware {
    func call(this, request: Request, next: Next) -> Response
}

// Planned middleware types:
// - Logger: request/response logging
// - Cors: Cross-Origin Resource Sharing headers
// - Compression: automatic Content-Encoding
// - RateLimit: request rate limiting
```

## 20. [NOT YET IMPLEMENTED] HTTP/2

> **This section describes planned functionality that is not yet implemented.**

HTTP/2 support will include binary framing, multiplexed streams, HPACK header compression,
server push, and flow control. The `HttpVersion::HTTP_2` variant already exists but the
client currently communicates using HTTP/1.1 wire format only.

## 21. [NOT YET IMPLEMENTED] WebSocket

> **This section describes planned functionality that is not yet implemented.**

WebSocket support will include the HTTP Upgrade handshake, frame encoding/decoding
(text, binary, ping, pong, close), and an event-driven message API.

---

*Previous: [06-TLS.md](./06-TLS.md)*
*Next: [08-COMPRESS.md](./08-COMPRESS.md) -- Compression Algorithms*
