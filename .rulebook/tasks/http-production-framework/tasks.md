# Tasks: HTTP Framework — cmmv-server Parity + 500K req/s

**Status**: IN_PROGRESS (March 2026)
**Reference**: https://github.com/cmmvio/cmmv-server

## Phase 0: Foundation Types

- [ ] 0.1 Create `server/request.tml` — Request type with: method, url, http_version, headers, params, body, raw fd
- [ ] 0.2 Request lazy getters: query(), path(), hostname(), ip(), protocol(), secure(), fresh(), stale(), xhr()
- [ ] 0.3 Request.get(name) — header by name (case-insensitive)
- [ ] 0.4 Request.is(type) — content-type check
- [ ] 0.5 Request.accepts(types) — Accept negotiation
- [ ] 0.6 Create `server/response.tml` — Response type with: status_code, headers, sent, header_sent, raw fd
- [ ] 0.7 Response.status(code) — chainable status setter
- [ ] 0.8 Response.send(body) — detect content-type, set Content-Length, write to socket
- [ ] 0.9 Response.json(data) — set application/json, serialize, send
- [ ] 0.10 Response.html(body) — set text/html, send
- [ ] 0.11 Response.redirect(url) and redirect_with(status, url)
- [ ] 0.12 Response.set(name, value) / get(name) — header manipulation
- [ ] 0.13 Response.type_set(mime) — Content-Type setter
- [ ] 0.14 Response.write(chunk) + end() — streaming support
- [ ] 0.15 Response.write_head(status, headers) — raw header write
- [ ] 0.16 Response.vary(field) — Vary header
- [ ] 0.17 Create `server/constants.tml` — HTTP methods list, status codes map, MIME types

## Phase 1: Hook System + Router

- [ ] 1.1 Create `server/hooks.tml` — Hooks type with: onRequest, preParsing, preHandler, onSend, onResponse, onError
- [ ] 1.2 Hook.add(name, fn), Hook.validate(name, fn)
- [ ] 1.3 Hook runners: onRequestHookRunner, preHandlerHookRunner, onSendHookRunner, onResponseHookRunner
- [ ] 1.4 Create `server/handle_request.tml` — body detection (bodyless: GET/HEAD/TRACE, bodywith: POST/PUT/PATCH/DELETE/OPTIONS)
- [ ] 1.5 Content-type parser registry (addContentTypeParser, contentTypeParser)
- [ ] 1.6 Wire router.tml to Application (find-my-way integration)
- [ ] 1.7 Handler signature: func(Request, Response) instead of func(AppContext) -> Str

## Phase 2: Application + Server

- [ ] 2.1 Rewrite `server/app.tml` — Application type: router, hooks, settings, cache, middlewares
- [ ] 2.2 app.get/post/put/delete/patch/head/options (all HTTP methods via router)
- [ ] 2.3 app.use(middleware) — middleware stack with path prefix support
- [ ] 2.4 app.set(key, value) / app.get(key) — settings (etag, trust proxy, view engine, etc.)
- [ ] 2.5 app.addHook(name, fn) — lifecycle hook registration
- [ ] 2.6 app.setErrorHandler(fn) — custom error handler chain
- [ ] 2.7 app.addContentTypeParser(type, fn) — content-type parser registration
- [ ] 2.8 app.listen(port) — create server, bind, accept loop
- [ ] 2.9 app.close() — graceful shutdown with connection draining
- [ ] 2.10 Server options: connectionTimeout, keepAliveTimeout, requestTimeout, bodyLimit, maxHeaderSize

## Phase 3: Error Handling + Serialization

- [ ] 3.1 Create `server/error_handler.tml` — buildErrorHandler, rootErrorHandler, fallbackErrorHandler
- [ ] 3.2 Error serialization (JSON format: {error, code, message, statusCode})
- [ ] 3.3 Hook-based error pipeline: handler error → onError hook → error handler chain → fallback
- [ ] 3.4 Set error headers from error.status/error.statusCode

## Phase 4: Performance (Thread Pool + Event Loop)

- [ ] 4.1 Thread pool with per-worker pre-allocated buffers (response, params, headers)
- [ ] 4.2 Non-blocking I/O via std::net::eventloop
- [ ] 4.3 Response writes directly to socket (no intermediate Str allocation)
- [ ] 4.4 HTTP version detection from request line (1.0, 1.1, 2)
- [ ] 4.5 Pre-computed status lines for common codes
- [ ] 4.6 Inline I64-to-ASCII for Content-Length

## Phase 5: IOCP + 500K Target

- [ ] 5.1 C runtime: tml_iocp_* functions (CreateIoCompletionPort, AcceptEx, WSARecv, WSASend)
- [ ] 5.2 TML: std::net::iocp module
- [ ] 5.3 IOCP-based server backend for app.listen()
- [ ] 5.4 Pre-posted AcceptEx + WSARecv buffers
- [ ] 5.5 Connection memory pool (slab allocator)

## Done (prior work, preserved)

- [x] Thread pool with mutex+condvar ring buffer (50K req/s)
- [x] EventLoop abstraction (std::net::eventloop)
- [x] Radix-tree router (router.tml)
- [x] Pattern matching dispatch with path params
- [x] Zero-copy HTTP parsing
- [x] Compiler: GlobalModuleCache hash validation
