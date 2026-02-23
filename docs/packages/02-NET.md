# std::net — Networking

## 1. Overview

The `std::net` package provides low-level networking primitives: TCP streams, UDP sockets, DNS resolution, IP addresses, and socket address handling. All networking goes through the OS via FFI to system calls.

```tml
use std::net::tcp::{TcpListener, TcpStream}
use std::net::udp::UdpSocket
use std::net::dns
use std::net::ip::{IpAddr, Ipv4Addr, Ipv6Addr}
use std::net::{SocketAddr, SocketAddrV4, SocketAddrV6}
```

**Implementation status**: Fully implemented and tested. 30 test files, 350+ tests passing.

## 2. Module Structure

```
std::net
├── ip          # Ipv4Addr, Ipv6Addr, IpAddr
├── socket      # SocketAddr, SocketAddrV4, SocketAddrV6
├── parser      # Address parsing (parse_ipv4, parse_ipv6, parse_socket_addr)
├── tcp         # TcpListener, TcpStream, TcpBuilder
├── udp         # UdpSocket, UdpBuilder
├── dns         # DNS resolution (lookup, reverse, lookup_all)
├── tls         # TLS/SSL (see 06-TLS.md)
├── error       # NetError, NetErrorKind
├── sys         # RawSocket, low-level socket operations
├── async_tcp   # Async TCP (future — not yet implemented)
└── async_udp   # Async UDP (future — not yet implemented)
```

## 3. IP Addresses

### 3.1 Ipv4Addr

```tml
use std::net::ip::Ipv4Addr

// Construction
let localhost: Ipv4Addr = Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8)
let any: Ipv4Addr = Ipv4Addr::UNSPECIFIED()
let broadcast: Ipv4Addr = Ipv4Addr::BROADCAST()

// Classification
localhost.is_loopback()       // true
localhost.is_private()        // false
localhost.is_multicast()      // false
localhost.is_broadcast()      // false
localhost.is_unspecified()    // false
localhost.is_link_local()     // false
localhost.is_documentation()  // false

// Accessors
let octs: [U8; 4] = localhost.octets()
let bits: U32 = localhost.to_bits()
let from_bits: Ipv4Addr = Ipv4Addr::from_bits(0x7F000001 as U32)

// Formatting
let s: Str = localhost.to_string()  // "127.0.0.1"

// Parsing (via parser module)
use std::net::parser
let parsed: Outcome[Ipv4Addr, AddrParseError] = parser::parse_ipv4("192.168.1.1")
```

### 3.2 Ipv6Addr

```tml
use std::net::ip::Ipv6Addr

// Construction
let localhost: Ipv6Addr = Ipv6Addr::new(
    0 as U16, 0 as U16, 0 as U16, 0 as U16,
    0 as U16, 0 as U16, 0 as U16, 1 as U16
)
let any: Ipv6Addr = Ipv6Addr::UNSPECIFIED()
let loopback: Ipv6Addr = Ipv6Addr::LOCALHOST()

// Classification
loopback.is_loopback()     // true
loopback.is_multicast()    // false
loopback.is_unspecified()  // false

// Accessors
let segs: [U16; 8] = loopback.segments()

// Formatting
let s: Str = loopback.to_string()  // "::1"
```

### 3.3 IpAddr (union)

```tml
use std::net::ip::{IpAddr, Ipv4Addr, Ipv6Addr}

pub type IpAddr = V4(Ipv4Addr) | V6(Ipv6Addr)

let addr: IpAddr = IpAddr::V4(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8))

// Pattern matching
when addr {
    IpAddr::V4(v4) => print("IPv4: " + v4.to_string() + "\n"),
    IpAddr::V6(v6) => print("IPv6: " + v6.to_string() + "\n"),
}

// Classification
addr.is_loopback()     // true
addr.is_ipv4()         // true
addr.is_ipv6()         // false
```

### 3.4 SocketAddr

```tml
use std::net::{SocketAddr, SocketAddrV4, SocketAddrV6}
use std::net::ip::{Ipv4Addr, Ipv6Addr}

// IPv4 socket address
let v4: SocketAddrV4 = SocketAddrV4::new(
    Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8),
    8080 as U16
)
let addr: SocketAddr = SocketAddr::V4(v4)

// IPv6 socket address
let v6: SocketAddrV6 = SocketAddrV6::new(
    Ipv6Addr::LOCALHOST(),
    8080 as U16,
    0 as U32,  // flowinfo
    0 as U32   // scope_id
)

// Accessors
let port: U16 = v4.port()
let ip: Ipv4Addr = v4.ip()
```

## 4. TCP

### 4.1 TcpStream

```tml
use std::net::tcp::TcpStream
use std::net::{SocketAddr, SocketAddrV4}
use std::net::ip::Ipv4Addr

// Connect to a remote server
let addr: SocketAddr = SocketAddr::V4(
    SocketAddrV4::new(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8), 8080 as U16)
)
let stream: TcpStream = TcpStream::connect(addr).unwrap()

// Read and write
var buf: [U8; 1024] = [0 as U8; 1024]
let n: I64 = stream.read(mut ref buf).unwrap()
let written: I64 = stream.write(ref buf).unwrap()

// Socket options
stream.set_nodelay(true).unwrap()
stream.set_keepalive(true).unwrap()
stream.set_ttl(64).unwrap()
stream.set_nonblocking(false).unwrap()

// Address info
let local: SocketAddr = stream.local_addr().unwrap()
let peer: SocketAddr = stream.peer_addr().unwrap()

// Access raw socket for TLS
let raw: RawSocket = stream.into_raw_socket()
```

### 4.2 TcpListener

```tml
use std::net::tcp::TcpListener
use std::net::{SocketAddr, SocketAddrV4}
use std::net::ip::Ipv4Addr

// Bind to port 0 (OS picks port)
let bind_addr: SocketAddr = SocketAddr::V4(
    SocketAddrV4::new(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8), 0 as U16)
)
let listener: TcpListener = TcpListener::bind(bind_addr).unwrap()

// Get assigned port
let local: SocketAddr = listener.local_addr().unwrap()

// Accept connections (blocking)
let result: (TcpStream, SocketAddr) = listener.accept().unwrap()
let client_stream: TcpStream = result.0
let client_addr: SocketAddr = result.1
```

### 4.3 TcpBuilder

```tml
use std::net::tcp::TcpBuilder

// Builder pattern for custom TCP options
let builder: TcpBuilder = TcpBuilder::new()
    .nodelay(true)
    .keepalive(true)
    .reuse_addr(true)
    .backlog(256)

// Use builder to create listener or stream
let listener: TcpListener = builder.bind(addr).unwrap()
let stream: TcpStream = builder.connect(addr).unwrap()
```

## 5. UDP

```tml
use std::net::udp::UdpSocket
use std::net::{SocketAddr, SocketAddrV4}
use std::net::ip::Ipv4Addr

// Bind to local address
let bind_addr: SocketAddr = SocketAddr::V4(
    SocketAddrV4::new(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8), 0 as U16)
)
let socket: UdpSocket = UdpSocket::bind(bind_addr).unwrap()

// Send to specific address
let target: SocketAddr = SocketAddr::V4(
    SocketAddrV4::new(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8), 9000 as U16)
)
let data: [U8; 5] = [72 as U8, 101 as U8, 108 as U8, 108 as U8, 111 as U8]  // "Hello"
socket.send_to(ref data, target).unwrap()

// Receive with sender address
var buf: [U8; 1024] = [0 as U8; 1024]
let result: (I64, SocketAddr) = socket.recv_from(mut ref buf).unwrap()
let bytes_read: I64 = result.0
let sender: SocketAddr = result.1

// Connected mode (send/recv without addresses)
socket.connect(target).unwrap()
socket.send(ref data).unwrap()
let n: I64 = socket.recv(mut ref buf).unwrap()

// Socket options
socket.set_broadcast(true).unwrap()
socket.set_ttl(128).unwrap()
socket.set_nonblocking(false).unwrap()
```

## 6. DNS Resolution

DNS resolution uses the OS resolver via FFI to `getaddrinfo`/`getnameinfo`.

### 6.1 Basic Lookup

```tml
use std::net::dns
use std::net::ip::{Ipv4Addr, Ipv6Addr, IpAddr}

// Resolve hostname to first IPv4 address
let addr: Ipv4Addr = dns::lookup("google.com").unwrap()
print("Google IPv4: " + addr.to_string() + "\n")

// Resolve to first IPv6 address
let v6: Ipv6Addr = dns::lookup6("google.com").unwrap()

// Resolve to either (prefers IPv4)
let ip: IpAddr = dns::lookup_ip("google.com").unwrap()
```

### 6.2 Multi-Result Lookup

```tml
use std::net::dns::{lookup_all, LookupResult}

// Get all addresses (up to 16 IPv4 + 16 IPv6)
let result: LookupResult = dns::lookup_all("google.com", 16).unwrap()

let total: I32 = result.count()
let v4_count: I32 = result.ipv4_count()
let v6_count: I32 = result.ipv6_count()

// Access individual addresses
when result.first() {
    Just(ip) => print("First: " + ip.to_string() + "\n"),
    Nothing => print("No results\n"),
}

when result.get_v4(0) {
    Just(v4) => print("First IPv4: " + v4.to_string() + "\n"),
    Nothing => {}
}
```

### 6.3 Reverse DNS

```tml
use std::net::dns
use std::net::ip::{IpAddr, Ipv4Addr}

// Reverse lookup: IP → hostname
let ip: IpAddr = IpAddr::V4(Ipv4Addr::new(8 as U8, 8 as U8, 8 as U8, 8 as U8))
when dns::reverse(ip) {
    Ok(hostname) => print("Hostname: " + hostname + "\n"),
    Err(e) => print("Reverse DNS failed: " + e.message() + "\n"),
}
```

### 6.4 Error Handling

```tml
use std::net::dns::{DnsError, DnsErrorKind}

when dns::lookup("nonexistent.invalid") {
    Ok(addr) => print("Resolved: " + addr.to_string() + "\n"),
    Err(e) => {
        let kind: DnsErrorKind = e.kind()
        if kind.is_not_found() {
            print("Hostname not found\n")
        } else {
            print("DNS error: " + e.message() + "\n")
        }
    }
}
```

## 7. Address Parsing

The `std::net::parser` module provides string-to-address conversion.

```tml
use std::net::parser

// IPv4
let v4: Ipv4Addr = parser::parse_ipv4("192.168.1.1").unwrap()

// IPv6 (full and compressed)
let v6: Ipv6Addr = parser::parse_ipv6("2001:db8::1").unwrap()
let v6_full: Ipv6Addr = parser::parse_ipv6("fe80:0000:0000:0000:0000:0000:0000:0001").unwrap()

// Either IPv4 or IPv6
let ip: IpAddr = parser::parse_ip("127.0.0.1").unwrap()

// Socket addresses
let sock_v4: SocketAddrV4 = parser::parse_socket_addr_v4("127.0.0.1:8080").unwrap()
let sock_v6: SocketAddrV6 = parser::parse_socket_addr_v6("[::1]:443").unwrap()
let sock: SocketAddr = parser::parse_socket_addr("192.168.1.1:80").unwrap()
```

## 8. Error Types

### 8.1 NetError

```tml
use std::net::error::{NetError, NetErrorKind}

// NetError is used by TCP, UDP, and socket operations
pub type NetError {
    error_kind: NetErrorKind,
}

// NetErrorKind covers all socket error conditions
// Common kinds: ConnectionRefused, ConnectionReset, NotConnected,
//               WouldBlock, TimedOut, AddrInUse, AddrNotAvailable
```

### 8.2 AddrParseError

```tml
use std::net::parser::AddrParseError

pub type AddrParseErrorKind = Empty | InvalidIpv4 | InvalidIpv6 | InvalidPort | InvalidSocketAddr
```

### 8.3 DnsError

```tml
use std::net::dns::{DnsError, DnsErrorKind}

// DnsErrorKind: NotFound, NoData, ServerFailure, BadName,
//               BadFamily, Timeout, Refused, NoMemory, Other
```

## 9. Working Examples

### 9.1 TCP Echo (Self-Contained with Threads)

This example shows a complete TCP echo server and client using threads:

```tml
use std::net::tcp::{TcpListener, TcpStream}
use std::net::{SocketAddr, SocketAddrV4}
use std::net::ip::Ipv4Addr
use std::thread

// Server: bind to port 0, accept one connection, echo data back
let bind_addr: SocketAddr = SocketAddr::V4(
    SocketAddrV4::new(Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8), 0 as U16)
)
let listener: TcpListener = TcpListener::bind(bind_addr).unwrap()
let local: SocketAddr = listener.local_addr().unwrap()

// Spawn server thread
thread::spawn(do() {
    let result: (TcpStream, SocketAddr) = listener.accept().unwrap()
    let client: TcpStream = result.0
    var buf: [U8; 256] = [0 as U8; 256]
    let n: I64 = client.read(mut ref buf).unwrap()
    client.write(ref buf).unwrap()
})

// Client: connect, send, receive echo
let stream: TcpStream = TcpStream::connect(local).unwrap()
let msg: [U8; 5] = [72 as U8, 101 as U8, 108 as U8, 108 as U8, 111 as U8]
stream.write(ref msg).unwrap()
var reply: [U8; 256] = [0 as U8; 256]
let n: I64 = stream.read(mut ref reply).unwrap()
// reply[0..5] == "Hello"
```

### 9.2 DNS Lookup + TCP Connect

This example resolves a hostname via DNS and connects via TCP:

```tml
use std::net::dns
use std::net::tcp::TcpStream
use std::net::{SocketAddr, SocketAddrV4}

// Resolve google.com
let ip: Ipv4Addr = dns::lookup("google.com").unwrap()
let addr: SocketAddr = SocketAddr::V4(SocketAddrV4::new(ip, 80 as U16))

// Connect via TCP
let stream: TcpStream = TcpStream::connect(addr).unwrap()
print("Connected to Google on port 80\n")

// Send HTTP request
let request: Str = "GET / HTTP/1.0\r\nHost: google.com\r\n\r\n"
var req_bytes: [U8; 128] = [0 as U8; 128]
// ... write request bytes, read response
```

### 9.3 UDP Datagram Exchange

```tml
use std::net::udp::UdpSocket
use std::net::{SocketAddr, SocketAddrV4}
use std::net::ip::Ipv4Addr

let lo: Ipv4Addr = Ipv4Addr::new(127 as U8, 0 as U8, 0 as U8, 1 as U8)

// Create two sockets
let sock_a: UdpSocket = UdpSocket::bind(
    SocketAddr::V4(SocketAddrV4::new(lo, 0 as U16))
).unwrap()
let sock_b: UdpSocket = UdpSocket::bind(
    SocketAddr::V4(SocketAddrV4::new(lo, 0 as U16))
).unwrap()

let addr_b: SocketAddr = sock_b.local_addr().unwrap()

// A sends to B
let data: [U8; 3] = [65 as U8, 66 as U8, 67 as U8]  // "ABC"
sock_a.send_to(ref data, addr_b).unwrap()

// B receives
var buf: [U8; 64] = [0 as U8; 64]
let result: (I64, SocketAddr) = sock_b.recv_from(mut ref buf).unwrap()
// result.0 == 3, buf[0..3] == "ABC"
```

## 10. Test Coverage

| Test File | Tests | What It Validates |
|-----------|-------|-------------------|
| `ip.test.tml` | IPv4/IPv6 construction | `new()`, `LOCALHOST()`, `UNSPECIFIED()`, `BROADCAST()` |
| `ip_v4_classify.test.tml` | IPv4 classification | `is_loopback()`, `is_private()`, `is_multicast()`, etc. |
| `ip_v4_bits.test.tml` | IPv4 bit manipulation | `to_bits()`, `from_bits()`, octet extraction |
| `ip_v4_extra.test.tml` | Additional IPv4 tests | Edge cases, special addresses |
| `ip_v4_traits.test.tml` | IPv4 Display/Hash | String formatting, equality |
| `ip_v6_extra.test.tml` | IPv6 operations | Segment access, classification |
| `ip_v6_traits.test.tml` | IPv6 Display/Hash | String formatting, equality |
| `ip_eq_fmt.test.tml` | IP equality/format | Cross-type comparisons |
| `ip_addr_extra.test.tml` | IpAddr union | V4/V6 dispatch, classification |
| `parser_ipv4.test.tml` | IPv4 parsing | Valid/invalid strings, edge cases |
| `parser_ipv6.test.tml` | IPv6 parsing | Full/compressed, `::` handling |
| `parser_socket.test.tml` | Socket parsing | `ip:port`, `[ipv6]:port` formats |
| `net_socket.test.tml` | Socket construction | SocketAddr, SocketAddrV4, SocketAddrV6 |
| `socket_v6.test.tml` | IPv6 sockets | SocketAddrV6 with flowinfo/scope_id |
| `tcp_extra.test.tml` | TCP builder/options | TcpBuilder pattern, socket options |
| `tcp_echo.test.tml` | TCP echo (threaded) | Full lifecycle: bind→listen→accept→connect→write→read |
| `tcp_nonblock.test.tml` | TCP non-blocking | Non-blocking mode, WouldBlock handling |
| `udp.test.tml` | UDP basics | Bind, socket options, local_addr |
| `udp_extra.test.tml` | UDP options | TTL, broadcast, buffer sizes |
| `udp_echo.test.tml` | UDP echo | send_to/recv_from datagram exchange |
| `dns_realworld.test.tml` | DNS real-world | `lookup("google.com")`, `lookup_all`, reverse |
| `dns_ipv6.test.tml` | DNS IPv6 | `lookup6()`, IPv6 resolution |
| `net_dns_error.test.tml` | DNS errors | Error kinds, `from_eai()`, messages |
| `net_error_extra.test.tml` | Net errors | NetErrorKind classification |
| `sys_socket_options.test.tml` | Raw socket options | TTL, buffer sizes, keepalive |
| `net_tls.test.tml` | TLS context | TLS context creation, options |
| `tls_google.test.tml` | TLS to Google | Real HTTPS connection, version/cipher |
| `tls_verify.test.tml` | TLS version pinning | Force TLS 1.2, verify negotiation |
| `tls_cloudflare.test.tml` | TLS 1.3 | Force TLS 1.3 to Cloudflare |
| `tls_cert_verify.test.tml` | Certificate verify | `TlsVerifyMode::Peer()`, X509 validation |

---

*Previous: [01-FS.md](./01-FS.md)*
*Next: [03-BUFFER.md](./03-BUFFER.md) — Buffer Operations*
