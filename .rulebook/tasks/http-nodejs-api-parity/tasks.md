# Tasks: HTTP Module — Node.js API Parity

**Status**: In Progress (0%)
**Priority**: High

## Phase 1: Server Foundation

- [ ] 1.1 Implement `incoming.tml` — IncomingMessage type (parsed HTTP request from wire)
- [ ] 1.2 Implement request parsing: wire format → IncomingMessage (method, url, headers, body)
- [ ] 1.3 Implement `server_response.tml` — ServerResponse type
- [ ] 1.4 Implement ServerResponse.write_head(status_code, headers)
- [ ] 1.5 Implement ServerResponse.set_header/get_header/has_header/remove_header
- [ ] 1.6 Implement ServerResponse.write(chunk) — buffer body data
- [ ] 1.7 Implement ServerResponse.end(data) — flush response to wire
- [ ] 1.8 Implement response serialization (ServerResponse → HTTP/1.1 wire format)
- [ ] 1.9 Implement ServerResponse properties: status_code, status_message, headers_sent, send_date
- [ ] 1.10 Implement ServerResponse.flush_headers()
- [ ] 1.11 Implement ServerResponse.write_continue() (100 Continue)
- [ ] 1.12 Implement ServerResponse.write_processing() (102 Processing)
- [ ] 1.13 Implement ServerResponse.write_early_hints(hints) (103 Early Hints)
- [ ] 1.14 Implement ServerResponse.add_trailers(headers)
- [ ] 1.15 Write tests for IncomingMessage parsing
- [ ] 1.16 Write tests for ServerResponse serialization

## Phase 2: HTTP Server

- [ ] 2.1 Implement `server.tml` — HttpServer type with TcpListener
- [ ] 2.2 Implement HttpServer.listen(port) — bind and accept loop
- [ ] 2.3 Implement HttpServer.close() — stop accepting connections
- [ ] 2.4 Implement HttpServer.close_all_connections()
- [ ] 2.5 Implement HttpServer.close_idle_connections()
- [ ] 2.6 Implement HttpServer.set_timeout(ms)
- [ ] 2.7 Implement server properties: listening, headers_timeout, keep_alive_timeout, request_timeout, max_headers_count, max_requests_per_socket
- [ ] 2.8 Implement request dispatch: accept → parse → create IncomingMessage + ServerResponse → call handler
- [ ] 2.9 Implement keep-alive connection handling on server side
- [ ] 2.10 Write tests for HttpServer (listening, request handling, properties)

## Phase 3: Chunked Transfer Encoding

- [ ] 3.1 Implement `chunked.tml` — ChunkedWriter (encode body as chunks)
- [ ] 3.2 Implement ChunkedReader (decode chunked response body)
- [ ] 3.3 Implement trailer headers support in chunked encoding
- [ ] 3.4 Integrate chunked encoding in ServerResponse (auto-chunk when no Content-Length)
- [ ] 3.5 Integrate chunked decoding in Response parser
- [ ] 3.6 Write tests for chunked encoding/decoding

## Phase 4: Connection Agent

- [ ] 4.1 Implement `agent.tml` — Agent type with connection pool
- [ ] 4.2 Implement Agent.create_connection(host, port, is_tls) — returns pooled or new Connection
- [ ] 4.3 Implement Agent.keep_socket_alive(fd) — return socket to pool
- [ ] 4.4 Implement Agent.reuse_socket(fd, request) — attach pooled socket to request
- [ ] 4.5 Implement Agent.destroy() — close all pooled sockets
- [ ] 4.6 Implement Agent.get_name(host, port, is_tls) — pool key
- [ ] 4.7 Implement Agent options: max_sockets, max_total_sockets, max_free_sockets, keep_alive, keep_alive_msecs, scheduling
- [ ] 4.8 Implement global_agent() singleton
- [ ] 4.9 Write tests for Agent (pooling, limits, reuse)

## Phase 5: Module-Level API

- [ ] 5.1 Implement `functions.tml` — module-level functions
- [ ] 5.2 Implement METHODS constant (list of all method strings)
- [ ] 5.3 Implement STATUS_CODES map (code → reason phrase)
- [ ] 5.4 Implement validate_header_name(name) → Outcome[Unit, HttpError]
- [ ] 5.5 Implement validate_header_value(name, value) → Outcome[Unit, HttpError]
- [ ] 5.6 Implement MAX_HEADER_SIZE constant (16KB default)
- [ ] 5.7 Implement create_server(handler_id) → HttpServer convenience function
- [ ] 5.8 Write tests for all module-level functions

## Phase 6: Client Enhancements

- [ ] 6.1 Integrate Agent into HttpClient (connection pooling)
- [ ] 6.2 Implement redirect following (auto-follow 3xx, configurable max)
- [ ] 6.3 Implement request timeout enforcement
- [ ] 6.4 Implement keep-alive on client side
- [ ] 6.5 Write tests for enhanced client (redirects, pooling, timeouts)

## Phase 7: Integration & mod.tml

- [ ] 7.1 Update mod.tml with new modules and re-exports
- [ ] 7.2 Run full http test suite
- [ ] 7.3 Fix any failures
- [ ] 7.4 Verify all existing tests still pass
