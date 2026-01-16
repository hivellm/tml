# std::net — Networking

## 1. Overview

The `std::net` package provides low-level networking primitives: TCP streams, UDP sockets, and IP address handling.

```tml
use std::net
use std::net.{TcpStream, TcpListener, UdpSocket}
```

## 2. Capabilities

```tml
caps: [io::network]           // Full network access
caps: [io::network.tcp]       // TCP only
caps: [io::network.udp]       // UDP only
caps: [io::network.connect]   // Outbound connections only
caps: [io::network.listen]    // Accept incoming connections
```

## 3. IP Addresses

### 3.1 IpAddr

```tml
pub type IpAddr = V4(Ipv4Addr) | V6(Ipv6Addr)

extend IpAddr {
    pub func parse(s: ref str) -> Outcome[This, AddrParseError]
    pub func is_loopback(this) -> Bool
    pub func is_multicast(this) -> Bool
    pub func is_unspecified(this) -> Bool
    pub func is_ipv4(this) -> Bool
    pub func is_ipv6(this) -> Bool
}
```

### 3.2 Ipv4Addr

```tml
pub type Ipv4Addr {
    octets: [U8; 4],
}

extend Ipv4Addr {
    public const LOCALHOST: This = This { octets: [127, 0, 0, 1] }
    public const UNSPECIFIED: This = This { octets: [0, 0, 0, 0] }
    public const BROADCAST: This = This { octets: [255, 255, 255, 255] }

    pub func new(a: U8, b: U8, c: U8, d: U8) -> This
    pub func parse(s: ref str) -> Outcome[This, AddrParseError]
    pub func octets(this) -> [U8; 4]
    pub func is_loopback(this) -> Bool
    pub func is_private(this) -> Bool
    pub func is_multicast(this) -> Bool
    pub func is_broadcast(this) -> Bool
    pub func is_unspecified(this) -> Bool
    pub func to_ipv6_compatible(this) -> Ipv6Addr
    pub func to_ipv6_mapped(this) -> Ipv6Addr
}
```

### 3.3 Ipv6Addr

```tml
pub type Ipv6Addr {
    segments: [U16; 8],
}

extend Ipv6Addr {
    public const LOCALHOST: This = This { segments: [0, 0, 0, 0, 0, 0, 0, 1] }
    public const UNSPECIFIED: This = This { segments: [0, 0, 0, 0, 0, 0, 0, 0] }

    pub func new(a: U16, b: U16, c: U16, d: U16, e: U16, f: U16, g: U16, h: U16) -> This
    pub func parse(s: ref str) -> Outcome[This, AddrParseError]
    pub func segments(this) -> [U16; 8]
    pub func is_loopback(this) -> Bool
    pub func is_multicast(this) -> Bool
    pub func is_unspecified(this) -> Bool
    pub func to_ipv4(this) -> Maybe[Ipv4Addr]
}
```

### 3.4 SocketAddr

```tml
pub type SocketAddr = V4(SocketAddrV4) | V6(SocketAddrV6)

extend SocketAddr {
    pub func new(ip: IpAddr, port: U16) -> This
    pub func parse(s: ref str) -> Outcome[This, AddrParseError]
    pub func ip(this) -> IpAddr
    pub func port(this) -> U16
    pub func set_ip(this, ip: IpAddr)
    pub func set_port(this, port: U16)
}

pub type SocketAddrV4 {
    ip: Ipv4Addr,
    port: U16,
}

pub type SocketAddrV6 {
    ip: Ipv6Addr,
    port: U16,
    flowinfo: U32,
    scope_id: U32,
}
```

## 4. TCP

### 4.1 TcpStream

```tml
pub type TcpStream {
    handle: RawSocket,
}

extend TcpStream {
    /// Connect to remote address
    pub func connect(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io::network.tcp, io::network.connect]

    /// Connect with timeout
    pub func connect_timeout(addr: ref SocketAddr, timeout: Duration) -> Outcome[This, IoError]
    effects: [io::network.tcp, io::network.connect]

    /// Get local address
    pub func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Get peer address
    pub func peer_addr(this) -> Outcome[SocketAddr, IoError]

    /// Shutdown read, write, or both
    pub func shutdown(this, how: Shutdown) -> Outcome[Unit, IoError]

    /// Set read timeout
    pub func set_read_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set write timeout
    pub func set_write_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Get read timeout
    pub func read_timeout(this) -> Outcome[Maybe[Duration], IoError]

    /// Get write timeout
    pub func write_timeout(this) -> Outcome[Maybe[Duration], IoError]

    /// Set TCP_NODELAY (disable Nagle's algorithm)
    pub func set_nodelay(this, nodelay: Bool) -> Outcome[Unit, IoError]

    /// Get TCP_NODELAY
    pub func nodelay(this) -> Outcome[Bool, IoError]

    /// Set TTL
    pub func set_ttl(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Get TTL
    pub func ttl(this) -> Outcome[U32, IoError]

    /// Clone as new handle
    pub func try_clone(this) -> Outcome[This, IoError]

    /// Take read half
    pub func take_read(this) -> Outcome[ReadHalf, IoError]

    /// Take write half
    pub func take_write(this) -> Outcome[WriteHalf, IoError]

    /// Peek at incoming data without consuming
    pub func peek(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tcp]
}

extend TcpStream with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tcp]
}

extend TcpStream with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tcp]

    func flush(this) -> Outcome[Unit, IoError]
}

pub type Shutdown = Read | Write | Both
```

### 4.2 TcpListener

```tml
pub type TcpListener {
    handle: RawSocket,
}

extend TcpListener {
    /// Bind to address and listen
    pub func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io::network.tcp, io::network.listen]

    /// Accept incoming connection
    pub func accept(this) -> Outcome[(TcpStream, SocketAddr), IoError]
    effects: [io::network.tcp, io::network.listen]

    /// Get local address
    pub func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Set non-blocking mode
    pub func set_nonblocking(this, nonblocking: Bool) -> Outcome[Unit, IoError]

    /// Clone as new handle
    pub func try_clone(this) -> Outcome[This, IoError]

    /// Iterator over incoming connections
    pub func incoming(this) -> Incoming
}

pub type Incoming {
    listener: ref TcpListener,
}

extend Incoming with Iterator {
    type Item = Outcome[TcpStream, IoError]

    func next(this) -> Maybe[Outcome[TcpStream, IoError]]
    effects: [io::network.tcp]
}
```

## 5. UDP

### 5.1 UdpSocket

```tml
pub type UdpSocket {
    handle: RawSocket,
}

extend UdpSocket {
    /// Bind to local address
    pub func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io::network.udp]

    /// Connect to remote (for send/recv instead of send_to/recv_from)
    pub func connect(this, addr: impl ToSocketAddrs) -> Outcome[Unit, IoError]
    effects: [io::network.udp]

    /// Send data to connected peer
    pub func send(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.udp]

    /// Receive data from connected peer
    pub func recv(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.udp]

    /// Send data to specific address
    pub func send_to(this, buf: ref [U8], addr: impl ToSocketAddrs) -> Outcome[U64, IoError]
    effects: [io::network.udp]

    /// Receive data and sender address
    pub func recv_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    effects: [io::network.udp]

    /// Peek at incoming data
    pub func peek(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.udp]

    /// Peek with sender address
    pub func peek_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    effects: [io::network.udp]

    /// Get local address
    pub func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Get peer address (if connected)
    pub func peer_addr(this) -> Outcome[SocketAddr, IoError]

    /// Set read timeout
    pub func set_read_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set write timeout
    pub func set_write_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set broadcast permission
    pub func set_broadcast(this, broadcast: Bool) -> Outcome[Unit, IoError]

    /// Get broadcast permission
    pub func broadcast(this) -> Outcome[Bool, IoError]

    /// Set TTL
    pub func set_ttl(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Get TTL
    pub func ttl(this) -> Outcome[U32, IoError]

    /// Set multicast TTL
    pub func set_multicast_ttl_v4(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Join multicast group
    pub func join_multicast_v4(this, multiaddr: ref Ipv4Addr, interface: ref Ipv4Addr) -> Outcome[Unit, IoError]

    /// Leave multicast group
    pub func leave_multicast_v4(this, multiaddr: ref Ipv4Addr, interface: ref Ipv4Addr) -> Outcome[Unit, IoError]

    /// Set non-blocking mode
    pub func set_nonblocking(this, nonblocking: Bool) -> Outcome[Unit, IoError]

    /// Clone as new handle
    pub func try_clone(this) -> Outcome[This, IoError]
}
```

## 6. DNS Resolution

### 6.1 ToSocketAddrs Trait

```tml
pub behaviorToSocketAddrs {
    type Iter: Iterator[Item = SocketAddr]

    func to_socket_addrs(this) -> Outcome[This.Iter, IoError]
    effects: [io::network]
}

// Implementations
extend SocketAddr with ToSocketAddrs { ... }
extend (IpAddr, U16) with ToSocketAddrs { ... }
extend (ref str, U16) with ToSocketAddrs { ... }  // DNS lookup
extend ref str with ToSocketAddrs { ... }         // "host:port" format
extend String with ToSocketAddrs { ... }
```

### 6.2 Lookup Functions

```tml
/// Resolve hostname to IP addresses
pub func lookup_host(host: ref str) -> Outcome[LookupHost, IoError]
effects: [io::network]

pub type LookupHost {
    inner: List[IpAddr],
    pos: U64,
}

extend LookupHost with Iterator {
    type Item = IpAddr
}

// Example
let addrs = lookup_host("example.com")!
loop addr in addrs {
    println(addr.to_string())
}
```

## 7. Unix Domain Sockets

```tml
@when(unix)
mod unix

pub type UnixStream {
    handle: RawFd,
}

extend UnixStream {
    pub func connect(path: impl AsRef[Path]) -> Outcome[This, IoError]
    effects: [io::network]

    pub func pair() -> Outcome[(This, This), IoError]
    effects: [io::network]

    pub func local_addr(this) -> Outcome[SocketAddr, IoError]
    pub func peer_addr(this) -> Outcome[SocketAddr, IoError]
    pub func shutdown(this, how: Shutdown) -> Outcome[Unit, IoError]
}

extend UnixStream with Read { ... }
extend UnixStream with Write { ... }

pub type UnixListener {
    handle: RawFd,
}

extend UnixListener {
    pub func bind(path: impl AsRef[Path]) -> Outcome[This, IoError]
    effects: [io::network]

    pub func accept(this) -> Outcome[(UnixStream, SocketAddr), IoError]
    effects: [io::network]

    pub func incoming(this) -> Incoming
}

pub type UnixDatagram {
    handle: RawFd,
}

extend UnixDatagram {
    pub func bind(path: impl AsRef[Path]) -> Outcome[This, IoError]
    pub func connect(this, path: impl AsRef[Path]) -> Outcome[Unit, IoError]
    pub func send(this, buf: ref [U8]) -> Outcome[U64, IoError]
    pub func recv(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    pub func send_to(this, buf: ref [U8], path: impl AsRef[Path]) -> Outcome[U64, IoError]
    pub func recv_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    pub func pair() -> Outcome[(This, This), IoError]
}
```

## 8. Async Support

### 8.1 Async TCP

```tml
mod async

pub type AsyncTcpStream {
    inner: TcpStream,
}

extend AsyncTcpStream {
    public async func connect(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io::network.tcp]

    public async func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tcp]

    public async func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tcp]

    public async func write_all(this, buf: ref [U8]) -> Outcome[Unit, IoError]
    effects: [io::network.tcp]
}

pub type AsyncTcpListener {
    inner: TcpListener,
}

extend AsyncTcpListener {
    public async func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io::network.tcp]

    public async func accept(this) -> Outcome[(AsyncTcpStream, SocketAddr), IoError]
    effects: [io::network.tcp]
}
```

## 9. Error Types

```tml
pub type AddrParseError {
    kind: AddrParseErrorKind,
}

pub type AddrParseErrorKind =
    | Empty
    | InvalidIpv4
    | InvalidIpv6
    | InvalidPort
    | InvalidFormat
```

## 10. Examples

### 10.1 TCP Client

```tml
mod tcp_client
caps: [io::network.tcp]

use std::net.TcpStream
use std::io.{BufReader, BufWriter, BufRead, Write}

pub func main() -> Outcome[Unit, Error] {
    // Connect to server
    let stream = TcpStream.connect("127.0.0.1:8080")!

    // Create buffered reader/writer
    let reader = BufReader.new(stream.try_clone()!)
    var writer = BufWriter.new(stream)

    // Send request
    writer.write_all(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")!
    writer.flush()!

    // Read response
    var line = String.new()
    reader.read_line(mut ref line)!
    println("Response: " + line)

    return Ok(unit)
}
```

### 10.2 TCP Server

```tml
mod tcp_server
caps: [io::network.tcp]

use std::net.{TcpListener, TcpStream}
use std::thread

pub func main() -> Outcome[Unit, Error] {
    let listener = TcpListener.bind("127.0.0.1:8080")!
    println("Listening on port 8080")

    loop (stream, addr) in listener.incoming() {
        let stream = stream!
        println("Connection from: " + addr.to_string())

        thread.spawn(do() {
            handle_client(stream).ok()
        })
    }

    return Ok(unit)
}

func handle_client(stream: TcpStream) -> Outcome[Unit, IoError] {
    var buf: [U8; 1024] = [0; 1024]
    let n = stream.read(mut ref buf)!

    let response = b"HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World!"
    stream.write_all(response)!

    return Ok(unit)
}
```

### 10.3 UDP Echo Server

```tml
mod udp_echo
caps: [io::network.udp]

use std::net.UdpSocket

pub func main() -> Outcome[Unit, Error] {
    let socket = UdpSocket.bind("127.0.0.1:9000")!
    println("UDP server on port 9000")

    var buf: [U8; 1024] = [0; 1024]

    loop {
        let (n, src) = socket.recv_from(mut ref buf)!
        println("Received " + n.to_string() + " bytes from " + src.to_string())

        // Echo back
        socket.send_to(ref buf[0 to n], src)!
    }
}
```

---

*Previous: [01-FS.md](./01-FS.md)*
*Next: [03-BUFFER.md](./03-BUFFER.md) — Buffer Operations*
