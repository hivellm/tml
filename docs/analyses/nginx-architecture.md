# NGINX Architecture: Deep Dive Analysis

**Date**: 2026-03-19
**Scope**: Event-driven web server design

## Executive Summary

NGINX is one of the worlds fastest HTTP servers, handling millions of concurrent connections.

## 1. Event Loop Model

NGINX uses process-per-core model with async I/O:
- One master process manages workers
- One worker per CPU core  
- Each worker handles thousands of connections via epoll/kqueue
- Capacity: worker_processes × worker_connections

## 2. HTTP Parser

Uses hand-coded state machine that scans buffer without copying:
- Read bytes into single buffer
- State machine marks token boundaries (offsets only)
- Return pointers to original buffer
- No memory allocations for tokens

Performance: ~1-5 µs request line, ~10-50 µs headers, <100 µs total

## 3. Memory Management

### Pool Allocator
Per-request memory pool avoids malloc/free overhead:
- Zero fragmentation
- Better cache locality  
- Automatic cleanup handlers
- Memory per request: ~8-12 KB (vs 2-5 MB for threads)

### Slab Allocator
For shared memory (caches):
- Divides into pages (4KB each)
- Each page into slots (8-1024 bytes)
- O(1) allocation/deallocation

### Buffer Structure
A single buffer can represent:
- Memory data (in-memory)
- File (sendfile, zero-copy)
- Shared memory (readonly references)

## 4. Request Processing Phases

11 pluggable phases: POST_READ, SERVER_REWRITE, FIND_CONFIG, REWRITE, POST_REWRITE, PREACCESS, ACCESS, POST_ACCESS, PRECONTENT, CONTENT, LOG

## 5. Connection Lifecycle

Accept -> Register READ -> epoll_wait()
Read -> Parse -> 11-phase processing -> Generate response
Register WRITE -> Send response
Keep-alive? -> YES: Reset and re-register READ; NO: Close

Timeouts: client_header_timeout (60s), client_body_timeout (60s), send_timeout (60s), keepalive_timeout (75s)

## 6. Buffer Management and Zero-Copy

### Sendfile Integration
Traditional: read(fd, buffer) + write(socket, buffer) = 2 copies, 2 syscalls
Sendfile: sendfile(socket, file) = 0 userspace copies, 1 syscall

Performance: Large files at disk speed, zero CPU overhead, no copies

### Buffer Reuse
Temporary buffers recycled after read to avoid allocations

## 7. Load Balancing

### Upstream Algorithms
- Round-robin (weight-based)
- Least connections
- IP hash (session affinity)
- Random (nginx 1.15+)

### Keepalive Pool
Persistent connections reused across client requests

## 8. Performance Characteristics

Throughput:
- Static HTML (2-core): 20-30K req/sec
- Static HTML (16-core): 150-300K req/sec  
- JSON: 15-25K req/sec
- Reverse proxy: 10-15K req/sec

Memory per connection:
- HTTP: ~6-8 KB
- HTTPS: ~15-25 KB
- 100K connections = 1.2 GB (vs 200 GB for threads)

Latency (percentiles):
- p50: 0.1-0.5 ms
- p90: 1-2 ms
- p99: 5-20 ms

## 9. Design Decisions

- Event-driven (epoll/kqueue) not threads: 10-100x fewer processes
- Zero-copy sendfile: 2-3x higher throughput
- Per-request memory pools: single free() instead of thousands
- Modular 11-phase system: extensible without core changes
- Process-per-core: simpler synchronization, no shared state

## 10. Weaknesses

- Dynamic content: not application server, requires backend
- Configuration reload: requires restart, drops keep-alive connections
- Regex locations: O(n) matching
- Limited streaming: buffers responses by default
- Module ABI: must recompile per version
- Limited debugging: no built-in debugger

## 11. Comparisons

NGINX vs Apache: Event vs thread (10KB vs 2-5MB mem), C10K (yes vs no), 30K vs 5K req/sec
NGINX vs HAProxy: Web server vs load balancer, reload vs hot reload
NGINX vs Caddy: C vs Go, manual vs automatic HTTPS, no hot reload vs yes

## 12. Real-World: 1M Requests/sec

Hardware: 16-core Xeon, 256GB RAM, 100 Gbps network
Config: worker_processes 32, worker_connections 50000, sendfile on
Performance: 1M req/sec, 31K per worker, 1-5ms latency, 150-200MB memory
Bottlenecks: NIC interrupts, kernel TCP stack, network cable, backend servers

## 13. Conclusion

NGINX succeeds through:
1. Event-driven I/O: 100K+ connections per process
2. Zero-copy design: sendfile + buffer chains
3. Memory efficiency: pools + management
4. Modularity: 11-phase extensible pipeline
5. Proven at scale: millions req/sec at Cloudflare, Netflix, Dropbox

For TML HTTP server: event-driven I/O, zero-copy sendfile, memory pools, modular request processing

## References

- https://nginx.org/en/docs/
- https://man7.org/linux/man-pages/man7/epoll.7.html
- https://www.techempower.com/benchmarks/
- http://www.kegel.com/c10k.html

