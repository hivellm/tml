# std.net — Networking

## 1. Overview

The `std.net` package provides low-level networking primitives: TCP streams, UDP sockets, and IP address handling.

```tml
import std.net
import std.net.{TcpStream, TcpListener, UdpSocket}
```

## 2. Capabilities

```tml
caps: [io.network]           // Full network access
caps: [io.network.tcp]       // TCP only
caps: [io.network.udp]       // UDP only
caps: [io.network.connect]   // Outbound connections only
caps: [io.network.listen]    // Accept incoming connections
```

## 3. IP Addresses

### 3.1 IpAddr

```tml
public type IpAddr = V4(Ipv4Addr) | V6(Ipv6Addr)

extend IpAddr {
    public func parse(s: ref str) -> Outcome[This, AddrParseError]
    public func is_loopback(this) -> Bool
    public func is_multicast(this) -> Bool
    public func is_unspecified(this) -> Bool
    public func is_ipv4(this) -> Bool
    public func is_ipv6(this) -> Bool
}
```

### 3.2 Ipv4Addr

```tml
public type Ipv4Addr {
    octets: [U8; 4],
}

extend Ipv4Addr {
    public const LOCALHOST: This = This { octets: [127, 0, 0, 1] }
    public const UNSPECIFIED: This = This { octets: [0, 0, 0, 0] }
    public const BROADCAST: This = This { octets: [255, 255, 255, 255] }

    public func new(a: U8, b: U8, c: U8, d: U8) -> This
    public func parse(s: ref str) -> Outcome[This, AddrParseError]
    public func octets(this) -> [U8; 4]
    public func is_loopback(this) -> Bool
    public func is_private(this) -> Bool
    public func is_multicast(this) -> Bool
    public func is_broadcast(this) -> Bool
    public func is_unspecified(this) -> Bool
    public func to_ipv6_compatible(this) -> Ipv6Addr
    public func to_ipv6_mapped(this) -> Ipv6Addr
}
```

### 3.3 Ipv6Addr

```tml
public type Ipv6Addr {
    segments: [U16; 8],
}

extend Ipv6Addr {
    public const LOCALHOST: This = This { segments: [0, 0, 0, 0, 0, 0, 0, 1] }
    public const UNSPECIFIED: This = This { segments: [0, 0, 0, 0, 0, 0, 0, 0] }

    public func new(a: U16, b: U16, c: U16, d: U16, e: U16, f: U16, g: U16, h: U16) -> This
    public func parse(s: ref str) -> Outcome[This, AddrParseError]
    public func segments(this) -> [U16; 8]
    public func is_loopback(this) -> Bool
    public func is_multicast(this) -> Bool
    public func is_unspecified(this) -> Bool
    public func to_ipv4(this) -> Maybe[Ipv4Addr]
}
```

### 3.4 SocketAddr

```tml
public type SocketAddr = V4(SocketAddrV4) | V6(SocketAddrV6)

extend SocketAddr {
    public func new(ip: IpAddr, port: U16) -> This
    public func parse(s: ref str) -> Outcome[This, AddrParseError]
    public func ip(this) -> IpAddr
    public func port(this) -> U16
    public func set_ip(this, ip: IpAddr)
    public func set_port(this, port: U16)
}

public type SocketAddrV4 {
    ip: Ipv4Addr,
    port: U16,
}

public type SocketAddrV6 {
    ip: Ipv6Addr,
    port: U16,
    flowinfo: U32,
    scope_id: U32,
}
```

## 4. TCP

### 4.1 TcpStream

```tml
public type TcpStream {
    handle: RawSocket,
}

extend TcpStream {
    /// Connect to remote address
    public func connect(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io.network.tcp, io.network.connect]

    /// Connect with timeout
    public func connect_timeout(addr: ref SocketAddr, timeout: Duration) -> Outcome[This, IoError]
    effects: [io.network.tcp, io.network.connect]

    /// Get local address
    public func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Get peer address
    public func peer_addr(this) -> Outcome[SocketAddr, IoError]

    /// Shutdown read, write, or both
    public func shutdown(this, how: Shutdown) -> Outcome[Unit, IoError]

    /// Set read timeout
    public func set_read_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set write timeout
    public func set_write_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Get read timeout
    public func read_timeout(this) -> Outcome[Maybe[Duration], IoError]

    /// Get write timeout
    public func write_timeout(this) -> Outcome[Maybe[Duration], IoError]

    /// Set TCP_NODELAY (disable Nagle's algorithm)
    public func set_nodelay(this, nodelay: Bool) -> Outcome[Unit, IoError]

    /// Get TCP_NODELAY
    public func nodelay(this) -> Outcome[Bool, IoError]

    /// Set TTL
    public func set_ttl(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Get TTL
    public func ttl(this) -> Outcome[U32, IoError]

    /// Clone as new handle
    public func try_clone(this) -> Outcome[This, IoError]

    /// Take read half
    public func take_read(this) -> Outcome[ReadHalf, IoError]

    /// Take write half
    public func take_write(this) -> Outcome[WriteHalf, IoError]

    /// Peek at incoming data without consuming
    public func peek(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.tcp]
}

extend TcpStream with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.tcp]
}

extend TcpStream with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.tcp]

    func flush(this) -> Outcome[Unit, IoError]
}

public type Shutdown = Read | Write | Both
```

### 4.2 TcpListener

```tml
public type TcpListener {
    handle: RawSocket,
}

extend TcpListener {
    /// Bind to address and listen
    public func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io.network.tcp, io.network.listen]

    /// Accept incoming connection
    public func accept(this) -> Outcome[(TcpStream, SocketAddr), IoError]
    effects: [io.network.tcp, io.network.listen]

    /// Get local address
    public func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Set non-blocking mode
    public func set_nonblocking(this, nonblocking: Bool) -> Outcome[Unit, IoError]

    /// Clone as new handle
    public func try_clone(this) -> Outcome[This, IoError]

    /// Iterator over incoming connections
    public func incoming(this) -> Incoming
}

public type Incoming {
    listener: ref TcpListener,
}

extend Incoming with Iterator {
    type Item = Outcome[TcpStream, IoError]

    func next(this) -> Maybe[Outcome[TcpStream, IoError]]
    effects: [io.network.tcp]
}
```

## 5. UDP

### 5.1 UdpSocket

```tml
public type UdpSocket {
    handle: RawSocket,
}

extend UdpSocket {
    /// Bind to local address
    public func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io.network.udp]

    /// Connect to remote (for send/recv instead of send_to/recv_from)
    public func connect(this, addr: impl ToSocketAddrs) -> Outcome[Unit, IoError]
    effects: [io.network.udp]

    /// Send data to connected peer
    public func send(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.udp]

    /// Receive data from connected peer
    public func recv(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.udp]

    /// Send data to specific address
    public func send_to(this, buf: ref [U8], addr: impl ToSocketAddrs) -> Outcome[U64, IoError]
    effects: [io.network.udp]

    /// Receive data and sender address
    public func recv_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    effects: [io.network.udp]

    /// Peek at incoming data
    public func peek(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.udp]

    /// Peek with sender address
    public func peek_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    effects: [io.network.udp]

    /// Get local address
    public func local_addr(this) -> Outcome[SocketAddr, IoError]

    /// Get peer address (if connected)
    public func peer_addr(this) -> Outcome[SocketAddr, IoError]

    /// Set read timeout
    public func set_read_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set write timeout
    public func set_write_timeout(this, dur: Maybe[Duration]) -> Outcome[Unit, IoError]

    /// Set broadcast permission
    public func set_broadcast(this, broadcast: Bool) -> Outcome[Unit, IoError]

    /// Get broadcast permission
    public func broadcast(this) -> Outcome[Bool, IoError]

    /// Set TTL
    public func set_ttl(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Get TTL
    public func ttl(this) -> Outcome[U32, IoError]

    /// Set multicast TTL
    public func set_multicast_ttl_v4(this, ttl: U32) -> Outcome[Unit, IoError]

    /// Join multicast group
    public func join_multicast_v4(this, multiaddr: ref Ipv4Addr, interface: ref Ipv4Addr) -> Outcome[Unit, IoError]

    /// Leave multicast group
    public func leave_multicast_v4(this, multiaddr: ref Ipv4Addr, interface: ref Ipv4Addr) -> Outcome[Unit, IoError]

    /// Set non-blocking mode
    public func set_nonblocking(this, nonblocking: Bool) -> Outcome[Unit, IoError]

    /// Clone as new handle
    public func try_clone(this) -> Outcome[This, IoError]
}
```

## 6. DNS Resolution

### 6.1 ToSocketAddrs Trait

```tml
public behaviorToSocketAddrs {
    type Iter: Iterator[Item = SocketAddr]

    func to_socket_addrs(this) -> Outcome[This.Iter, IoError]
    effects: [io.network]
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
public func lookup_host(host: ref str) -> Outcome[LookupHost, IoError]
effects: [io.network]

public type LookupHost {
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
module unix

public type UnixStream {
    handle: RawFd,
}

extend UnixStream {
    public func connect(path: impl AsRef[Path]) -> Outcome[This, IoError]
    effects: [io.network]

    public func pair() -> Outcome[(This, This), IoError]
    effects: [io.network]

    public func local_addr(this) -> Outcome[SocketAddr, IoError]
    public func peer_addr(this) -> Outcome[SocketAddr, IoError]
    public func shutdown(this, how: Shutdown) -> Outcome[Unit, IoError]
}

extend UnixStream with Read { ... }
extend UnixStream with Write { ... }

public type UnixListener {
    handle: RawFd,
}

extend UnixListener {
    public func bind(path: impl AsRef[Path]) -> Outcome[This, IoError]
    effects: [io.network]

    public func accept(this) -> Outcome[(UnixStream, SocketAddr), IoError]
    effects: [io.network]

    public func incoming(this) -> Incoming
}

public type UnixDatagram {
    handle: RawFd,
}

extend UnixDatagram {
    public func bind(path: impl AsRef[Path]) -> Outcome[This, IoError]
    public func connect(this, path: impl AsRef[Path]) -> Outcome[Unit, IoError]
    public func send(this, buf: ref [U8]) -> Outcome[U64, IoError]
    public func recv(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    public func send_to(this, buf: ref [U8], path: impl AsRef[Path]) -> Outcome[U64, IoError]
    public func recv_from(this, buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), IoError]
    public func pair() -> Outcome[(This, This), IoError]
}
```

## 8. Async Support

### 8.1 Async TCP

```tml
module async

public type AsyncTcpStream {
    inner: TcpStream,
}

extend AsyncTcpStream {
    public async func connect(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io.network.tcp]

    public async func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.tcp]

    public async func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io.network.tcp]

    public async func write_all(this, buf: ref [U8]) -> Outcome[Unit, IoError]
    effects: [io.network.tcp]
}

public type AsyncTcpListener {
    inner: TcpListener,
}

extend AsyncTcpListener {
    public async func bind(addr: impl ToSocketAddrs) -> Outcome[This, IoError]
    effects: [io.network.tcp]

    public async func accept(this) -> Outcome[(AsyncTcpStream, SocketAddr), IoError]
    effects: [io.network.tcp]
}
```

## 9. Error Types

```tml
public type AddrParseError {
    kind: AddrParseErrorKind,
}

public type AddrParseErrorKind =
    | Empty
    | InvalidIpv4
    | InvalidIpv6
    | InvalidPort
    | InvalidFormat
```

## 10. Examples

### 10.1 TCP Client

```tml
module tcp_client
caps: [io.network.tcp]

import std.net.TcpStream
import std.io.{BufReader, BufWriter, BufRead, Write}

public func main() -> Outcome[Unit, Error] {
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
module tcp_server
caps: [io.network.tcp]

import std.net.{TcpListener, TcpStream}
import std.thread

public func main() -> Outcome[Unit, Error] {
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
module udp_echo
caps: [io.network.udp]

import std.net.UdpSocket

public func main() -> Outcome[Unit, Error] {
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
