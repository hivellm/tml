# Tasks: HTTP Module — Node.js API Parity

**Status**: COMPLETE (March 2026, commit 1655f1ee)

## Phase 1: Server Foundation

- [x] 1.1 Implement `incoming.tml` — IncomingMessage type (parsed HTTP request from wire)
- [x] 1.2 Implement request parsing: wire format → IncomingMessage (method, url, headers, body)
- [x] 1.3 Implement `server_response.tml` — ServerResponse type
- [x] 1.4 Implement ServerResponse.write_head(status_code, headers)
- [x] 1.5 Implement ServerResponse.set_header/get_header/has_header/remove_header
- [x] 1.6 Implement ServerResponse.write(chunk) — buffer body data
- [x] 1.7 Implement ServerResponse.end(data) — flush response to wire
- [x] 1.8 Implement response serialization (ServerResponse → HTTP/1.1 wire format)
- [x] 1.9 Implement ServerResponse properties: status_code, status_message, headers_sent, send_date
- [x] 1.10 Implement ServerResponse.flush_headers()
- [x] 1.11 Implement ServerResponse.write_continue() (100 Continue)
- [x] 1.12 Implement ServerResponse.write_processing() (102 Processing)
- [x] 1.13 Implement ServerResponse.write_early_hints(hints) (103 Early Hints)
- [x] 1.14 Implement ServerResponse.add_trailers(headers)
- [x] 1.15 Write tests for IncomingMessage parsing
- [x] 1.16 Write tests for ServerResponse serialization

## Phase 2: HTTP Server

- [x] 2.1 Implement `server.tml` — HttpServer type with TcpListener
- [x] 2.2 Implement HttpServer.listen(port) — bind and accept loop
- [x] 2.3 Implement HttpServer.close() — stop accepting connections
- [x] 2.4 Implement HttpServer.close_all_connections()
- [x] 2.5 Implement HttpServer.close_idle_connections()
- [x] 2.6 Implement HttpServer.set_timeout(ms)
- [x] 2.7 Implement server properties: listening, headers_timeout, keep_alive_timeout, request_timeout, max_headers_count, max_requests_per_socket
- [x] 2.8 Implement request dispatch: accept → parse → create IncomingMessage + ServerResponse → call handler
- [x] 2.9 Implement keep-alive connection handling on server side
- [x] 2.10 Write tests for HttpServer (listening, request handling, properties)

## Phase 3: Chunked Transfer Encoding

- [x] 3.1 Implement `chunked.tml` — ChunkedWriter (encode body as chunks)
- [x] 3.2 Implement ChunkedReader (decode chunked response body)
- [x] 3.3 Implement trailer headers support in chunked encoding
- [x] 3.4 Integrate chunked encoding in ServerResponse (auto-chunk when no Content-Length)
- [x] 3.5 Integrate chunked decoding in Response parser
- [x] 3.6 Write tests for chunked encoding/decoding

## Phase 4: Connection Agent

- [x] 4.1 Implement `agent.tml` — Agent type with connection pool
- [x] 4.2 Implement Agent.create_connection(host, port, is_tls) — returns pooled or new Connection
- [x] 4.3 Implement Agent.keep_socket_alive(fd) — return socket to pool
- [x] 4.4 Implement Agent.reuse_socket(fd, request) — attach pooled socket to request
- [x] 4.5 Implement Agent.destroy() — close all pooled sockets
- [x] 4.6 Implement Agent.get_name(host, port, is_tls) — pool key
- [x] 4.7 Implement Agent options: max_sockets, max_total_sockets, max_free_sockets, keep_alive, keep_alive_msecs, scheduling
- [x] 4.8 Implement global_agent() singleton
- [x] 4.9 Write tests for Agent (pooling, limits, reuse)

## Phase 5: Module-Level API

- [x] 5.1 Implement `functions.tml` — module-level functions
- [x] 5.2 Implement METHODS constant (list of all method strings)
- [x] 5.3 Implement STATUS_CODES map (code → reason phrase)
- [x] 5.4 Implement validate_header_name(name) → Outcome[Unit, HttpError]
- [x] 5.5 Implement validate_header_value(name, value) → Outcome[Unit, HttpError]
- [x] 5.6 Implement MAX_HEADER_SIZE constant (16KB default)
- [x] 5.7 Implement create_server(handler_id) → HttpServer convenience function
- [x] 5.8 Write tests for all module-level functions

## Phase 6: Client Enhancements

- [x] 6.1 Integrate Agent into HttpClient (connection pooling)
- [x] 6.2 Implement redirect following (auto-follow 3xx, configurable max)
- [x] 6.3 Implement request timeout enforcement
- [x] 6.4 Implement keep-alive on client side
- [x] 6.5 Write tests for enhanced client (redirects, pooling, timeouts)

## Phase 7: Integration & mod.tml

- [x] 7.1 Update mod.tml with new modules and re-exports
- [x] 7.2 Run full http test suite
- [x] 7.3 Fix any failures
- [x] 7.4 Verify all existing tests still pass

## Phase 8: Middleware Ecosystem (added March 2026)

- [x] 8.1 Implement router with params and wildcards
- [x] 8.2 Implement CORS middleware
- [x] 8.3 Implement gzip/deflate compression middleware
- [x] 8.4 Implement security headers middleware (HSTS, CSP, X-Frame-Options)
- [x] 8.5 Implement ETag + If-None-Match conditional cache middleware
- [x] 8.6 Implement body_parser (JSON, form-urlencoded, raw)
- [x] 8.7 Implement content_type enforcement middleware
- [x] 8.8 Implement SSE (Server-Sent Events) stream middleware
- [x] 8.9 Implement HTTP Range requests middleware (206 Partial Content)
- [x] 8.10 Implement cache_control header builder middleware
- [x] 8.11 Implement token-bucket rate limiter per IP
- [x] 8.12 Implement static file server (MIME, ETag, Range, index.html)
- [x] 8.13 Implement content-encoding pipeline (gzip, deflate, identity)
- [x] 8.14 Implement multipart/form-data parser
- [x] 8.15 Update spec: docs/specs/20-STDLIB.md section 16 (HTTP Module)
- [x] 8.16 Update spec: docs/specs/INDEX.md to cover all 37 spec files
- [x] 8.17 Update spec: docs/specs/INDEX.md implementation status table (March 2026)
