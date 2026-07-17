# Tasks: HTTP Framework — cmmv-server Parity + 500K req/s

**Status**: COMPLETE (March 2026) — All phases done
**Reference**: https://github.com/cmmvio/cmmv-server

## Phase 0: Foundation Types

- [x] 0.1 IncomingMessage already has: method, url, http_version, headers, body, socket_fd
- [x] 0.2 IncomingMessage getters: query(), path(), hostname(), ip(), protocol(), secure(), fresh(), stale(), xhr()
- [x] 0.3 IncomingMessage.get_header(name) — header by name (case-insensitive)
- [x] 0.4 IncomingMessage.is_type(type) — content-type check
- [x] 0.5 IncomingMessage.accepts(types) — Accept negotiation
- [x] 0.6 ServerResponse already has: status_code, headers, finished, headers_sent, socket_fd
- [x] 0.7 ServerResponse.status(code) — status setter
- [x] 0.8 ServerResponse.send(body) — detect content-type, set Content-Length, end
- [x] 0.9 ServerResponse.json(data) — set application/json, end
- [x] 0.10 ServerResponse.html(body) — set text/html, end
- [x] 0.11 ServerResponse.redirect(url) and redirect_with(status, url)
- [x] 0.12 ServerResponse.set_header(name, value) / get_header(name) — header manipulation
- [x] 0.13 ServerResponse.type_set(mime) — Content-Type setter
- [x] 0.14 ServerResponse.write(chunk) + end() — streaming support (pre-existing)
- [x] 0.15 ServerResponse.write_head(status, message) — raw header write (pre-existing)
- [x] 0.16 ServerResponse.vary(field) — Vary header
- [x] 0.17 Already covered: functions.tml (methods, status_codes), content_type.tml (MIME), body_parser.tml

## Phase 1: Hook System + Router

- [x] 1.1 Hook system in app.tml — onRequest, preHandler, onSend, onResponse, onError (5 hooks)
- [x] 1.2 Hook.add(name, fn) — generic dispatcher to typed add_hook_* methods
- [x] 1.3 Hook runners: app_run_hooks (2-param) + app_run_error_hooks (3-param) in app.tml
- [x] 1.4 Body detection: app_is_bodyless_method() skips body parse for GET/HEAD/TRACE/OPTIONS
- [x] 1.5 Content-type parser registry — add_content_type_parser(type, fn) in App
- [x] 1.6 Router wired to Application (radix tree inline lookup + insert from router.tml)
- [x] 1.7 Handler signature: func(IncomingMessage, ServerResponse) instead of func(AppContext) -> Str

## Phase 2: Application + Server

- [x] 2.1 Rewrite `app.tml` — App type: router, hooks (5), settings, middleware, body limit
- [x] 2.2 app.get/post/put/delete/patch/head/options/all (all HTTP methods via router)
- [x] 2.3 app.use_middleware(mw) — middleware stack (path prefix support TODO)
- [x] 2.4 App settings: set_trust_proxy(), set_etag(), set_x_powered_by() as typed methods
- [x] 2.5 app.add_hook_on_request/pre_handler/on_send/on_response/on_error — lifecycle hooks
- [x] 2.6 app.set_error_handler(fn) — custom error handler
- [x] 2.7 app.add_content_type_parser(type, fn) — implemented with 16-slot registry
- [x] 2.8 app.listen(port) — thread pool + event loop (listen_async)
- [x] 2.9 app_shutdown() — graceful shutdown via global shared ptr + OFF_SHUTDOWN flag
- [x] 2.10 Server options: read/write/idle timeout, bodyLimit

## Phase 3: Error Handling + Serialization

- [x] 3.1 Error handler via app_run_error_hooks + set_error_handler (in app.tml, no separate file needed)
- [x] 3.2 Error serialization: app_error_response(status, msg) → {"statusCode":N,"error":"...","message":"..."}
- [x] 3.3 Hook-based error pipeline: 404 → onError hooks → error handler → default JSON response
- [x] 3.4 Error headers — handlers set headers directly via ServerResponse, error_handler has full access

## Phase 4: Performance (Thread Pool + Event Loop)

- [x] 4.1 Per-worker pre-allocated params buffer (128 bytes, reused across requests)
- [x] 4.2 Non-blocking I/O via std::net::eventloop (pre-existing, both listen modes work)
- [x] 4.3 Response writes — vectored_io.tml send_response_parts for header+body, ServerResponse.serialize for direct
- [x] 4.4 HTTP version detection: app_is_http11() — HTTP/1.0 closes connection by default
- [x] 4.5 Pre-computed status lines: fast_status_line() for 16 common codes
- [x] 4.6 Inline I64-to-ASCII: fast_i64_to_str() for Content-Length (no format system)

## Phase 5: IOCP + 500K Target — DONE (in http-production-server)

- [x] 5.1 C runtime: tml_iocp_* functions — implemented in iocp.c (10 functions)
- [x] 5.2 TML: iocp_worker.tml with app_listen_iocp
- [x] 5.3 IOCP-based server backend — App.listen_iocp() integrated
- [x] 5.4 Pre-posted AcceptEx buffers — tml_iocp_accept + accept pool
- [x] 5.5 Connection memory — arena.tml per-worker allocator + Bytes ref-counted buffers

## Done (prior work, preserved)

- [x] Thread pool with mutex+condvar ring buffer (50K req/s)
- [x] EventLoop abstraction (std::net::eventloop)
- [x] Radix-tree router (router.tml)
- [x] Pattern matching dispatch with path params
- [x] Zero-copy HTTP parsing
- [x] Compiler: GlobalModuleCache hash validation

## Done (cmmv-server rewrite, 2026-03-17)

- [x] Handler signature: func(IncomingMessage, ServerResponse) via I64 fn ptrs
- [x] Remove AppContext — replaced with IncomingMessage + ServerResponse
- [x] Hook system: onRequest, preHandler, onSend, onResponse, onError (5 lifecycle hooks)
- [x] Custom error handler (set_error_handler)
- [x] Body limit enforcement (413 Payload Too Large)
- [x] App.options() and App.all() methods
- [x] app_get_param() helper for fast-path param access
- [x] Shared state layout expanded for hooks (SHARED_SIZE 240→336)
- [x] Sample server updated (samples/http-server/server.tml)
- [x] IncomingMessage.cookie(name) — reads Cookie header
- [x] ServerResponse.set_cookie(Cookie), cookie(name, val), clear_cookie(name)
- [x] Sample expanded with middleware, hooks, settings, HTML/text handlers
