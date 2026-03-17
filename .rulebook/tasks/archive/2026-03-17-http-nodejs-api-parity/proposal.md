# Proposal: HTTP Module — Node.js API Parity

## Status: IN_PROGRESS

## Why

TML's HTTP module has a solid client-side implementation (HttpClient, Request, Response, Headers, etc.) but is completely missing the **server side** — which is the core of Node.js's `http` module. This task implements full HTTP server functionality and enhances the client to match Node.js API coverage.

## Current State

### Already Implemented (✅)
- `Method` — all 9 HTTP methods with utility methods
- `Status` — full IANA status code registry (1xx-5xx)
- `HttpVersion` — 1.0, 1.1, 2, 3
- `Headers` — case-insensitive header map with linear scan
- `HttpError` / `HttpErrorKind` — 16 error categories
- `Cookie` — RFC 6265 parse/serialize
- `Request` — builder pattern, URL parsing, serialization
- `Response` — wire format parser, status/header/body accessors
- `HttpClient` — GET/POST/PUT/DELETE/HEAD with TLS
- `Connection` — TCP + TLS layer
- `Router` — radix tree with params/wildcards
- `MultipartBuilder` — RFC 2046 multipart form-data
- `encoding` — gzip/deflate/br/zstd compress/decompress

### Missing (❌) — Node.js API Parity
- **HttpServer** — listen, accept, dispatch, close
- **ServerResponse** — writeHead, write, end, headers
- **IncomingMessage** — parsed request on server side
- **Agent** — connection pooling, keep-alive, socket reuse
- **Chunked transfer encoding** — read and write
- **Module-level API** — createServer, METHODS, STATUS_CODES, validateHeaderName/Value
- **Client enhancements** — redirect following, Agent integration, keep-alive

## Node.js HTTP API → TML Mapping

### Module-Level Functions
| Node.js | TML | Status |
|---------|-----|--------|
| `http.createServer(handler)` | `http::create_server(handler_id)` | ❌ |
| `http.request(url, options)` | `HttpClient::send(req)` | ✅ |
| `http.get(url)` | `HttpClient::get(url)` | ✅ |
| `http.METHODS` | `http::METHODS` (array of Str) | ❌ |
| `http.STATUS_CODES` | `http::STATUS_CODES` (HashMap) | ❌ |
| `http.validateHeaderName(name)` | `http::validate_header_name(name)` | ❌ |
| `http.validateHeaderValue(name, value)` | `http::validate_header_value(name, value)` | ❌ |
| `http.maxHeaderSize` | `http::MAX_HEADER_SIZE` | ❌ |
| `http.globalAgent` | `http::global_agent()` | ❌ |

### Class: http.Server → HttpServer
| Node.js | TML | Status |
|---------|-----|--------|
| `server.listen(port)` | `server.listen(port)` | ❌ |
| `server.close()` | `server.close()` | ❌ |
| `server.closeAllConnections()` | `server.close_all_connections()` | ❌ |
| `server.closeIdleConnections()` | `server.close_idle_connections()` | ❌ |
| `server.setTimeout(ms)` | `server.set_timeout(ms)` | ❌ |
| `server.listening` | `server.is_listening()` | ❌ |
| `server.headersTimeout` | `server.headers_timeout` | ❌ |
| `server.keepAliveTimeout` | `server.keep_alive_timeout` | ❌ |
| `server.requestTimeout` | `server.request_timeout` | ❌ |
| `server.maxHeadersCount` | `server.max_headers_count` | ❌ |
| `server.maxRequestsPerSocket` | `server.max_requests_per_socket` | ❌ |

### Class: http.ServerResponse → ServerResponse
| Node.js | TML | Status |
|---------|-----|--------|
| `response.writeHead(code, headers)` | `response.write_head(code, headers)` | ❌ |
| `response.write(chunk)` | `response.write(chunk)` | ❌ |
| `response.end(data)` | `response.end(data)` | ❌ |
| `response.setHeader(name, value)` | `response.set_header(name, value)` | ❌ |
| `response.getHeader(name)` | `response.get_header(name)` | ❌ |
| `response.getHeaderNames()` | `response.get_header_names()` | ❌ |
| `response.getHeaders()` | `response.get_headers()` | ❌ |
| `response.hasHeader(name)` | `response.has_header(name)` | ❌ |
| `response.removeHeader(name)` | `response.remove_header(name)` | ❌ |
| `response.flushHeaders()` | `response.flush_headers()` | ❌ |
| `response.addTrailers(headers)` | `response.add_trailers(headers)` | ❌ |
| `response.writeContinue()` | `response.write_continue()` | ❌ |
| `response.writeProcessing()` | `response.write_processing()` | ❌ |
| `response.writeEarlyHints(hints)` | `response.write_early_hints(hints)` | ❌ |
| `response.statusCode` | `response.status_code` | ❌ |
| `response.statusMessage` | `response.status_message` | ❌ |
| `response.headersSent` | `response.headers_sent` | ❌ |
| `response.sendDate` | `response.send_date` | ❌ |
| `response.socket` | `response.socket()` | ❌ |
| `response.req` | `response.request()` | ❌ |

### Class: http.IncomingMessage → IncomingMessage
| Node.js | TML | Status |
|---------|-----|--------|
| `message.method` | `message.method()` | ❌ |
| `message.url` | `message.url()` | ❌ |
| `message.headers` | `message.headers()` | ❌ |
| `message.rawHeaders` | `message.raw_headers()` | ❌ |
| `message.httpVersion` | `message.http_version()` | ❌ |
| `message.statusCode` | `message.status_code()` | ❌ |
| `message.statusMessage` | `message.status_message()` | ❌ |
| `message.complete` | `message.is_complete()` | ❌ |
| `message.trailers` | `message.trailers()` | ❌ |
| `message.socket` | `message.socket()` | ❌ |
| `message.destroy()` | `message.destroy()` | ❌ |
| `message.setTimeout(ms)` | `message.set_timeout(ms)` | ❌ |

### Class: http.Agent → Agent
| Node.js | TML | Status |
|---------|-----|--------|
| `new Agent(options)` | `Agent::new(options)` | ❌ |
| `agent.createConnection(opts)` | `agent.create_connection(opts)` | ❌ |
| `agent.keepSocketAlive(socket)` | `agent.keep_socket_alive(fd)` | ❌ |
| `agent.reuseSocket(socket, req)` | `agent.reuse_socket(fd, req)` | ❌ |
| `agent.destroy()` | `agent.destroy()` | ❌ |
| `agent.getName(options)` | `agent.get_name(opts)` | ❌ |
| `agent.maxSockets` | `agent.max_sockets` | ❌ |
| `agent.maxTotalSockets` | `agent.max_total_sockets` | ❌ |
| `agent.maxFreeSockets` | `agent.max_free_sockets` | ❌ |
| `agent.freeSockets` | `agent.free_sockets()` | ❌ |
| `agent.sockets` | `agent.sockets()` | ❌ |
| `agent.requests` | `agent.requests()` | ❌ |

### Chunked Transfer Encoding
| Feature | Status |
|---------|--------|
| Chunked encoding writer | ❌ |
| Chunked encoding reader/parser | ❌ |
| Trailer headers support | ❌ |

### Client Enhancements
| Feature | Current | Target | Status |
|---------|---------|--------|--------|
| Redirect following | ❌ | Auto-follow 3xx up to N redirects | ❌ |
| Keep-alive | ❌ | Reuse connections via Agent | ❌ |
| Connection pooling | ❌ | Agent manages pool | ❌ |
| Request timeout | Stored but unused | Actually enforced | ❌ |

## Architecture

```
Application Code
     │
     ├── HttpClient (enhanced with Agent)
     │      ↓
     │   Agent (connection pool)
     │      ↓
     │   Connection (TCP + TLS)
     │
     └── HttpServer
            ↓
         ┌─── accept loop (TcpListener)
         │       ↓
         │    parse_request() → IncomingMessage
         │       ↓
         │    ServerResponse (write_head, write, end)
         │       ↓
         │    serialize → Connection → client
         └───────────────────────────────────
```

## Impact
- New files: `server.tml`, `server_response.tml`, `incoming.tml`, `agent.tml`, `chunked.tml`, `functions.tml`
- Modified: `mod.tml`, `client.tml` (Agent integration)
- Tests: ~15 new test files
- Breaking: NO (all additive)
