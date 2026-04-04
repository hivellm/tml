# TML REST API Guide

This guide explains how to build a REST API using TML's HTTP framework with NestJS-style
`@Controller`, `@Get`/`@Post`/`@Put`/`@Delete` decorators, and the pointer-based DI system
from `std::di`.

## Complete Example

See `samples/api/main.tml` for a runnable CRUD API covering all patterns in this guide.

```bash
tml run samples/api/main.tml
# Starts on http://localhost:3000
```

---

## Handler Signature

Every HTTP handler is a free function with this exact signature:

```tml
func handler_name(req: IncomingMessage, res: Response) -> Str {
    return res.json("{\"ok\":true}")
}
```

- `req: IncomingMessage` — the incoming HTTP request (method, URL, headers, body)
- `res: Response` — a pre-configured response builder (status 200, CORS/Security/ETag auto-applied)
- Return type `-> Str` — the complete HTTP response string (built by `res.json()`, `res.html()`, etc.)

Handlers must be **free functions** (not methods). They are passed to the App as function
pointers cast to `I64`:

```tml
app.get("/api/users", list_users as I64)
```

---

## Route Parameter Extraction

Use `app_get_param(req, "name")` to read path parameters:

```tml
use std::http::server::dispatch::app_get_param

@Get("/api/users/:id")
func get_user(req: IncomingMessage, res: Response) -> Str {
    let id = app_get_param(req, "id")
    return res.json(`{"id":{id}}` as Str)
}
```

Parameters are declared in the route path with a `:` prefix (e.g., `/:id`, `/:category/:slug`).
`app_get_param` returns an empty string `""` when the named parameter is absent.

---

## Response Builder

`Response` provides a fluent builder API for composing HTTP responses.

### Content type methods (terminal — return `Str`)

| Method | Content-Type | Example |
|--------|-------------|---------|
| `res.json(data)` | `application/json; charset=utf-8` | `res.json("{\"ok\":true}")` |
| `res.html(data)` | `text/html; charset=utf-8` | `res.html("<h1>Hello</h1>")` |
| `res.text(data)` | `text/plain; charset=utf-8` | `res.text("plain text")` |

### Chaining methods (return `Response` for further chaining)

| Method | Effect |
|--------|--------|
| `res.status(code)` | Create a new response with the given HTTP status code |
| `res.set_status(code)` | Same as `status()` but called on an existing `Response` value |
| `res.header(name, value)` | Add a custom HTTP header |
| `res.no_cors()` | Disable automatic CORS headers |

### Examples

```tml
// 200 JSON
return res.json("{\"users\":[]}")

// 201 Created
return res.status(201).json("{\"id\":42}")

// 404 Not Found
return res.status(404).json("{\"error\":\"not found\"}")

// 400 with custom header
return res.status(400).header("X-Error-Code", "INVALID_INPUT").json("{\"error\":\"bad input\"}")

// HTML page
return res.html("<html><body><h1>Hello</h1></body></html>")
```

Response includes these headers automatically:
- `Content-Type` and `Content-Length`
- CORS headers (`Access-Control-Allow-Origin`, etc.)
- Security headers (`X-Frame-Options`, `X-Content-Type-Options`, etc.)
- `ETag` (when body is non-empty)
- `Cache-Control: no-store` (for dynamic content)
- `Connection: keep-alive`

---

## Template Literals

TML template literals let you embed variables inline. They return `Text`, which must be
cast to `Str` when passing to response methods:

```tml
let id = app_get_param(req, "id")
let name = "alice"
return res.json(`{"id":{id},"name":"{name}"}` as Str)
```

The cast `` `...` as Str`` is required because `res.json()` takes `Str`.

---

## @Controller Decorator

`@Controller("/prefix")` marks a type as a route group with a URL prefix. It serves two roles:

1. **Documentation** — clearly documents which URL prefix the type owns
2. **Behavior hook** — the type implements `impl Controller for MyController` to register routes

```tml
use std::http::app::controller::Controller

@Controller("/api/users")
type UserController {
    user_svc_ptr: I64   // injected service pointer
}

impl Controller for UserController {
    func prefix(this) -> Str { return "/api/users" }

    func register(this, app: mut ref App) {
        app.get("/api/users", list_users as I64)
        app.get("/api/users/:id", get_user as I64)
        app.post("/api/users", create_user as I64)
        app.put("/api/users/:id", update_user as I64)
        app.delete("/api/users/:id", delete_user as I64)
    }
}
```

The `Controller` behavior requires two methods:
- `prefix(this) -> Str` — the base URL prefix (used for introspection)
- `register(this, app: mut ref App)` — registers all routes with the `App`

Call `register` explicitly in `main()`:

```tml
let ctrl = UserController { user_svc_ptr: &user_svc as I64 }
ctrl.register(ref app)
```

### @Get / @Post / @Put / @Delete / @Patch

These decorators on free functions document the intended HTTP verb and path. They do not
auto-register routes — you still pass the handler to `app.get()` etc. in your controller's
`register()` method. This may be auto-wired by a future compiler feature.

```tml
@Get("/api/users")
func list_users(req: IncomingMessage, res: Response) -> Str {
    return res.json("[{\"id\":1}]")
}

// Register explicitly in the controller
impl Controller for UserController {
    func register(this, app: mut ref App) {
        app.get("/api/users", list_users as I64)
    }
}
```

---

## Request Body

Read the request body as a `Str` via `req.body()`:

```tml
@Post("/api/users")
func create_user(req: IncomingMessage, res: Response) -> Str {
    let body = req.body()
    if str::len(body) == 0 {
        return res.status(400).json("{\"error\":\"body required\"}")
    }
    // Parse body, create resource...
    return res.status(201).json("{\"created\":true}")
}
```

The default body size limit is 1 MB. Override with `app.set_body_limit(bytes)`.

---

## DI Integration

TML uses pointer-based dependency injection — no IoC container, no reflection, no proxies.
Services are stack-allocated and wired via raw `I64` pointers.

### Service pattern

```tml
@Service
type UserService {
    db_ptr: I64   // pointer to a database connection
}

impl UserService {
    pub func new(db_ptr: I64) -> UserService {
        return UserService { db_ptr: db_ptr }
    }

    pub func find_all(this) -> Str {
        return "[{\"id\":1}]"
    }
}
```

### Controller with injected service

```tml
@Controller("/api/users")
type UserController {
    user_svc_ptr: I64   // raw pointer to UserService
}
```

### Wiring in main()

```tml
pub func main() -> I32 {
    // 1. Create services on the stack
    var user_svc = UserService::new(0)

    // 2. Wire controller — take address, cast to I64
    let ctrl = UserController { user_svc_ptr: &user_svc as I64 }

    // 3. Register routes
    var app: App = App::new()
    ctrl.register(ref app)

    // 4. Start server
    app.listen(3000)
    return 0
}
```

To call a service method from a handler, dereference the pointer in the handler body. Since
handlers are free functions, services are typically accessed via module-level state or passed
through a shared registry.

### Config via InjectedConfig

```tml
use std::di::config::InjectedConfig

var config = InjectedConfig::new()
config.set("port", "3000")
config.set("db", ":memory:")

let port = config.get_i64_or("port", 3000)
let db   = config.get_or("db", ":memory:")
```

---

## Full Application Structure

```tml
use core::str
use std::http::app::App
use std::http::app::controller::Controller
use std::http::server::dispatch::app_get_param
use std::http::server::incoming::IncomingMessage
use std::http::framework::response_builder::Response
use std::di::config::InjectedConfig

// --- Handlers (free functions) ---

@Get("/api/items")
func list_items(req: IncomingMessage, res: Response) -> Str {
    return res.json("[{\"id\":1}]")
}

@Get("/api/items/:id")
func get_item(req: IncomingMessage, res: Response) -> Str {
    let id = app_get_param(req, "id")
    return res.json(`{"id":{id}}` as Str)
}

// --- Controller ---

@Controller("/api/items")
type ItemController {}

impl Controller for ItemController {
    func prefix(this) -> Str { return "/api/items" }
    func register(this, app: mut ref App) {
        app.get("/api/items", list_items as I64)
        app.get("/api/items/:id", get_item as I64)
    }
}

// --- Bootstrap ---

pub func main() -> I32 {
    var config = InjectedConfig::new()
    config.set("port", "3000")

    let ctrl = ItemController {}
    var app: App = App::new()
    ctrl.register(ref app)
    app.listen(3000)
    return 0
}
```

---

## Supported HTTP Methods

| App method | HTTP verb |
|------------|-----------|
| `app.get(path, handler as I64)` | GET |
| `app.post(path, handler as I64)` | POST |
| `app.put(path, handler as I64)` | PUT |
| `app.delete(path, handler as I64)` | DELETE |
| `app.patch(path, handler as I64)` | PATCH |
| `app.head(path, handler as I64)` | HEAD |
| `app.options(path, handler as I64)` | OPTIONS |
| `app.all(path, handler as I64)` | All methods |

## Server Startup Options

```tml
app.listen(3000)         // thread-pool server (default, highest throughput)
app.listen_async(3000)   // event-loop server (more concurrent connections)
app.listen_iocp(3000)    // Windows IOCP server (zero-blocking, highest perf on Windows)
```
