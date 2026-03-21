# Proposal: HTTP Framework — Production-Ready (cmmv-server parity)

## Status: IN_PROGRESS

## Reference Implementation

Based on [cmmv-server](https://github.com/cmmvio/cmmv-server) — a TypeScript rewrite of Express with Fastify-level performance.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  Application                     │
│  settings, engines, cache, content-type parsers  │
├─────────────┬───────────────┬───────────────────┤
│   Router    │    Hooks      │  Error Handler    │
│ (find-my-way│ (15 lifecycle │ (chain fallback)  │
│  radix tree)│  hooks)       │                   │
├─────────────┴───────────────┴───────────────────┤
│              Handle Request                      │
│  body detection → content-type → hooks → handler │
├─────────────────────────────────────────────────┤
│          Request          │       Response       │
│ method, url, headers,     │ status, send, json,  │
│ query, params, ip, host,  │ redirect, type,      │
│ protocol, cookies, body,  │ headers, streaming,  │
│ fresh/stale, accepts      │ etag, vary, sendFile │
├─────────────────────────────────────────────────┤
│              Server (HTTP/1.1 + HTTP/2)          │
│ listen, close, keepAlive, timeout, maxRequests   │
└─────────────────────────────────────────────────┘
```

## Module Structure

```
lib/std/src/http/
├── server/
│   ├── app.tml           — Application (settings, use, listen, routes)
│   ├── request.tml       — Request type (lazy getters, headers, query, params)
│   ├── response.tml      — Response type (send, json, redirect, status, headers)
│   ├── router.tml        — Router (find-my-way radix tree) [existing]
│   ├── hooks.tml         — Hook system (15 lifecycle hooks)
│   ├── handle_request.tml — Request dispatch (body detection, content-type, hooks)
│   ├── error_handler.tml — Error handling chain
│   └── constants.tml     — HTTP methods, status codes, MIME types
├── incoming.tml          — IncomingMessage [existing]
├── server_response.tml   — ServerResponse [existing]
└── ...
```

## Request Type (from cmmv-server)

```tml
pub type Request {
    raw: I64,           // raw socket fd
    method: Str,        // GET, POST, etc.
    url: Str,           // /users/42?foo=bar
    http_version: Str,  // "1.1" or "2"
    headers: Headers,   // parsed headers map
    params: I64,        // route params (from Router)
    body: Str,          // parsed body
    app: I64,           // back-reference to Application
}

impl Request {
    pub func query(this) -> Str          // lazy: parse from url
    pub func path(this) -> Str           // lazy: url without query
    pub func hostname(this) -> Str       // lazy: from Host header
    pub func ip(this) -> Str             // lazy: from socket or X-Forwarded-For
    pub func protocol(this) -> Str       // "http" or "https"
    pub func secure(this) -> Bool        // protocol == "https"
    pub func fresh(this) -> Bool         // ETag/Last-Modified check
    pub func stale(this) -> Bool         // !fresh
    pub func xhr(this) -> Bool           // X-Requested-With == XMLHttpRequest
    pub func get(this, name: Str) -> Str // header by name
    pub func is(this, types: Str) -> Bool // content-type check
    pub func accepts(this, types: Str) -> Str // Accept negotiation
}
```

## Response Type (from cmmv-server)

```tml
pub type Response {
    raw: I64,           // raw socket fd
    status_code: I64,   // default 200
    headers: Headers,   // response headers
    sent: Bool,         // already sent?
    header_sent: Bool,  // headers written to wire?
    app: I64,           // back-reference
    request: I64,       // back-reference to Request
}

impl Response {
    pub func status(mut this, code: I64) -> ref Response   // chainable
    pub func send(this, body: Str) -> Unit                  // send body + end
    pub func json(this, data: Str) -> Unit                  // send JSON
    pub func html(this, body: Str) -> Unit                  // send HTML
    pub func redirect(this, url: Str) -> Unit               // 302 redirect
    pub func redirect_with(this, status: I64, url: Str)     // redirect with status
    pub func set(mut this, name: Str, value: Str)           // set header
    pub func get(this, name: Str) -> Str                    // get header
    pub func type_set(mut this, mime: Str)                   // Content-Type
    pub func write(this, chunk: Str) -> Unit                 // streaming write
    pub func end(this) -> Unit                               // finish response
    pub func write_head(this, status: I64, headers: Str)    // raw writeHead
    pub func send_file(this, path: Str)                      // static file
    pub func vary(mut this, field: Str)                      // Vary header
}
```

## Hook Lifecycle (from cmmv-server)

```
Client Request
     │
     ▼
 onRequest ──── (can short-circuit with error)
     │
     ▼
 preParsing ─── (body parsing, content-type detection)
     │
     ▼
 preValidation
     │
     ▼
 preHandler ─── (auth, rate-limit, etc.)
     │
     ▼
  Handler ────── (user code: req, res => ...)
     │
     ▼
 preSerialization
     │
     ▼
  onSend ─────── (modify payload before sending)
     │
     ▼
 onResponse ──── (logging, metrics, cleanup)
     │
     ▼
Client Response
```

## Handler Signature

cmmv-server style (NOT the current ctx.json() pattern):

```tml
app.get("/users/:id", do(req: Request, res: Response) {
    let id: Str = req.params.get("id")
    res.json("{\"id\":\"" + id + "\"}")
})
```

## What's Different from Current app.tml

| Current | New (cmmv-style) |
|---------|-----------------|
| `AppContext` = flat struct | `Request` + `Response` = separate objects |
| `ctx.json(200, body)` returns Str | `res.status(200).json(body)` writes to socket |
| Hardcoded HTTP/1.1 | Version detection from request |
| No headers parsing | Full headers map |
| No query string parsing | Lazy query parsing |
| 1 middleware type | 15 hook lifecycle |
| Handler returns Str | Handler receives (req, res), res writes to socket |
| String concat for response | Buffer-based response builder |
