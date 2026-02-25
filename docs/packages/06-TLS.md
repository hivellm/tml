# std::tls — TLS/SSL Connections

## 1. Overview

The `std::tls` package provides TLS (Transport Layer Security) for secure network communication.

```tml
use std::tls
use std::tls.{TlsConnector, TlsAcceptor, TlsStream}
```

## 2. Capabilities

```tml
caps: [io::network.tls]      // TLS connections
caps: [io::file.read]        // For loading certificates (optional)
```

## 3. Client Connections

### 3.1 TlsConnector

```tml
pub type TlsConnector {
    config: TlsConfig,
}

extend TlsConnector {
    /// Create with default system roots
    pub func new() -> Outcome[This, TlsError]

    /// Create with custom configuration
    pub func with_config(config: TlsConfig) -> Outcome[This, TlsError]

    /// Connect to server
    pub func connect[S: Read + Write](
        this,
        domain: ref str,
        stream: S
    ) -> Outcome[TlsStream[S], TlsError]
    effects: [io::network.tls]

    /// Connect without hostname verification (dangerous!)
    pub func connect_without_verification[S: Read + Write](
        this,
        stream: S
    ) -> Outcome[TlsStream[S], TlsError]
    effects: [io::network.tls]
}
```

### 3.2 TlsConnectorBuilder

```tml
pub type TlsConnectorBuilder {
    min_version: Maybe[TlsVersion],
    max_version: Maybe[TlsVersion],
    root_certs: List[Certificate],
    client_cert: Maybe[(Certificate, PrivateKey)],
    alpn_protocols: List[String],
    verify_hostname: Bool,
    verify_cert: Bool,
}

extend TlsConnectorBuilder {
    pub func new() -> This

    /// Set minimum TLS version
    pub func min_version(this, version: TlsVersion) -> This

    /// Set maximum TLS version
    pub func max_version(this, version: TlsVersion) -> This

    /// Add root certificate
    pub func add_root_cert(this, cert: Certificate) -> This

    /// Load root certificates from file
    pub func load_root_certs(this, path: ref Path) -> Outcome[This, TlsError]
    effects: [io::file.read]

    /// Use system root certificates
    pub func use_system_roots(this) -> Outcome[This, TlsError]

    /// Set client certificate for mutual TLS
    pub func client_cert(this, cert: Certificate, key: PrivateKey) -> This

    /// Set ALPN protocols
    pub func alpn_protocols(this, protocols: ref [ref str]) -> This

    /// Disable hostname verification (dangerous!)
    pub func danger_disable_hostname_verification(this) -> This

    /// Disable certificate verification (dangerous!)
    pub func danger_disable_cert_verification(this) -> This

    /// Build connector
    pub func build(this) -> Outcome[TlsConnector, TlsError]
}
```

## 4. Server Connections

### 4.1 TlsAcceptor

```tml
pub type TlsAcceptor {
    config: TlsConfig,
}

extend TlsAcceptor {
    /// Create with certificate and key
    pub func new(cert: Certificate, key: PrivateKey) -> Outcome[This, TlsError]

    /// Create with certificate chain
    pub func with_chain(
        certs: ref [Certificate],
        key: PrivateKey
    ) -> Outcome[This, TlsError]

    /// Create with custom configuration
    pub func with_config(config: TlsConfig) -> Outcome[This, TlsError]

    /// Accept TLS connection
    pub func accept[S: Read + Write](this, stream: S) -> Outcome[TlsStream[S], TlsError]
    effects: [io::network.tls]
}
```

### 4.2 TlsAcceptorBuilder

```tml
pub type TlsAcceptorBuilder {
    cert_chain: List[Certificate],
    private_key: Maybe[PrivateKey],
    min_version: Maybe[TlsVersion],
    max_version: Maybe[TlsVersion],
    client_auth: ClientAuth,
    alpn_protocols: List[String],
}

pub type ClientAuth =
    | NoClientAuth
    | RequestClientCert
    | RequireClientCert
    | RequireAndVerifyClientCert(List[Certificate])

extend TlsAcceptorBuilder {
    pub func new() -> This

    /// Set certificate and key
    pub func identity(this, cert: Certificate, key: PrivateKey) -> This

    /// Set certificate chain
    pub func cert_chain(this, certs: ref [Certificate]) -> This

    /// Set private key
    pub func private_key(this, key: PrivateKey) -> This

    /// Load from PKCS#12/PFX file
    pub func load_pkcs12(this, path: ref Path, password: ref str) -> Outcome[This, TlsError]
    effects: [io::file.read]

    /// Set minimum TLS version
    pub func min_version(this, version: TlsVersion) -> This

    /// Set maximum TLS version
    pub func max_version(this, version: TlsVersion) -> This

    /// Set client authentication mode
    pub func client_auth(this, auth: ClientAuth) -> This

    /// Set ALPN protocols
    pub func alpn_protocols(this, protocols: ref [ref str]) -> This

    /// Build acceptor
    pub func build(this) -> Outcome[TlsAcceptor, TlsError]
}
```

## 5. TLS Stream

### 5.1 TlsStream

```tml
pub type TlsStream[S] {
    inner: S,
    session: TlsSession,
}

extend TlsStream[S: Read + Write] {
    /// Get peer certificate
    pub func peer_certificate(this) -> Maybe[ref Certificate]

    /// Get peer certificate chain
    pub func peer_certificates(this) -> ref [Certificate]

    /// Get negotiated ALPN protocol
    pub func alpn_protocol(this) -> Maybe[ref str]

    /// Get negotiated TLS version
    pub func tls_version(this) -> TlsVersion

    /// Get negotiated cipher suite
    pub func cipher_suite(this) -> CipherSuite

    /// Get SNI hostname (server only)
    pub func sni_hostname(this) -> Maybe[ref str]

    /// Check if handshake is complete
    pub func is_handshake_complete(this) -> Bool

    /// Get underlying stream
    pub func get_ref(this) -> ref S

    /// Get mutable reference to underlying stream
    pub func get_mut(this) -> mut ref S

    /// Unwrap to underlying stream
    pub func into_inner(this) -> S

    /// Initiate graceful shutdown
    pub func shutdown(this) -> Outcome[Unit, TlsError]
    effects: [io::network.tls]
}

extend TlsStream[S: Read + Write] with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tls]
}

extend TlsStream[S: Read + Write] with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    effects: [io::network.tls]

    func flush(this) -> Outcome[Unit, IoError]
    effects: [io::network.tls]
}
```

## 6. Certificates and Keys

### 6.1 Certificate

```tml
pub type Certificate {
    der: List[U8],
}

extend Certificate {
    /// Parse from DER bytes
    pub func from_der(der: ref [U8]) -> Outcome[This, CertError]

    /// Parse from PEM string
    pub func from_pem(pem: ref str) -> Outcome[This, CertError]

    /// Parse multiple from PEM string
    pub func from_pem_multiple(pem: ref str) -> Outcome[List[This], CertError]

    /// Load from DER file
    pub func load_der(path: ref Path) -> Outcome[This, CertError]
    effects: [io::file.read]

    /// Load from PEM file
    pub func load_pem(path: ref Path) -> Outcome[This, CertError]
    effects: [io::file.read]

    /// Load multiple from PEM file
    pub func load_pem_multiple(path: ref Path) -> Outcome[List[This], CertError]
    effects: [io::file.read]

    /// Get DER bytes
    pub func to_der(this) -> ref [U8]

    /// Get PEM string
    pub func to_pem(this) -> String

    /// Get subject name
    pub func subject(this) -> Outcome[Name, CertError]

    /// Get issuer name
    pub func issuer(this) -> Outcome[Name, CertError]

    /// Get validity period
    pub func validity(this) -> Outcome[(SystemTime, SystemTime), CertError]

    /// Get serial number
    pub func serial_number(this) -> Outcome[List[U8], CertError]

    /// Check if self-signed
    pub func is_self_signed(this) -> Bool

    /// Get public key
    pub func public_key(this) -> Outcome[PublicKey, CertError]
}
```

### 6.2 PrivateKey

```tml
pub type PrivateKey {
    kind: KeyKind,
    der: List[U8],
}

pub type KeyKind = Rsa | Ecdsa | Ed25519

extend PrivateKey {
    /// Parse from DER bytes (PKCS#8)
    pub func from_der(der: ref [U8]) -> Outcome[This, KeyError]

    /// Parse from PEM string
    pub func from_pem(pem: ref str) -> Outcome[This, KeyError]

    /// Load from DER file
    pub func load_der(path: ref Path) -> Outcome[This, KeyError]
    effects: [io::file.read]

    /// Load from PEM file
    pub func load_pem(path: ref Path) -> Outcome[This, KeyError]
    effects: [io::file.read]

    /// Load from encrypted PEM file
    pub func load_encrypted_pem(path: ref Path, password: ref str) -> Outcome[This, KeyError]
    effects: [io::file.read]

    /// Get DER bytes
    pub func to_der(this) -> ref [U8]

    /// Get PEM string
    pub func to_pem(this) -> String

    /// Get key type
    pub func kind(this) -> KeyKind
}
```

### 6.3 PKCS#12

```tml
mod pkcs12

pub type Pkcs12 {
    cert: Certificate,
    chain: List[Certificate],
    key: PrivateKey,
}

extend Pkcs12 {
    /// Parse from DER bytes
    pub func from_der(der: ref [U8], password: ref str) -> Outcome[This, Pkcs12Error]

    /// Load from file
    pub func load(path: ref Path, password: ref str) -> Outcome[This, Pkcs12Error]
    effects: [io::file.read]

    /// Create PKCS#12 bundle
    pub func create(
        cert: Certificate,
        key: PrivateKey,
        chain: ref [Certificate],
        password: ref str
    ) -> Outcome[List[U8], Pkcs12Error]
}
```

## 7. TLS Configuration

### 7.1 TlsVersion

```tml
pub type TlsVersion = Tls10 | Tls11 | Tls12 | Tls13

extend TlsVersion {
    pub const LATEST: This = Tls13
    pub const MIN_SECURE: This = Tls12
}
```

### 7.2 CipherSuite

```tml
pub type CipherSuite {
    id: U16,
    name: String,
}

// TLS 1.3 cipher suites
pub const TLS_AES_128_GCM_SHA256: CipherSuite = ...
pub const TLS_AES_256_GCM_SHA384: CipherSuite = ...
pub const TLS_CHACHA20_POLY1305_SHA256: CipherSuite = ...

// TLS 1.2 cipher suites
pub const TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256: CipherSuite = ...
pub const TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384: CipherSuite = ...
pub const TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256: CipherSuite = ...
pub const TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384: CipherSuite = ...
```

### 7.3 TlsConfig

```tml
pub type TlsConfig {
    min_version: TlsVersion,
    max_version: TlsVersion,
    cipher_suites: List[CipherSuite],
    alpn_protocols: List[String],
    root_certs: List[Certificate],
    client_cert: Maybe[(Certificate, PrivateKey)],
    server_cert: Maybe[(List[Certificate], PrivateKey)>,
    client_auth: ClientAuth,
    session_cache_size: U64,
    session_timeout: Duration,
}
```

## 8. Error Types

```tml
pub type TlsError {
    kind: TlsErrorKind,
    message: String,
    source: Maybe[Heap[dyn Error]],
}

pub type TlsErrorKind =
    | HandshakeFailed
    | CertificateError(CertError)
    | KeyError(KeyError)
    | AlertReceived(AlertDescription)
    | PeerIncompatible
    | NoCipherSuitesInCommon
    | NoProtocolVersionInCommon
    | InvalidMessage
    | DecryptError
    | IoError(IoError)

pub type AlertDescription =
    | CloseNotify
    | UnexpectedMessage
    | BadRecordMac
    | DecryptionFailed
    | RecordOverflow
    | DecompressionFailure
    | HandshakeFailure
    | BadCertificate
    | UnsupportedCertificate
    | CertificateRevoked
    | CertificateExpired
    | CertificateUnknown
    | IllegalParameter
    | UnknownCa
    | AccessDenied
    | DecodeError
    | DecryptError
    | ProtocolVersion
    | InsufficientSecurity
    | InternalError
    | UserCanceled
    | NoRenegotiation
    | UnsupportedExtension
    | CertificateRequired
    | NoApplicationProtocol
```

## 9. Implementation Status

> **Note**: The spec above (sections 1-8) describes the full planned API. The current
> implementation uses a lower-level OpenSSL wrapper API. The examples below show the
> **actual working API** as of v0.1.6 (2026-02-23).

### Current API (implemented)

```tml
use std::net::tls::{TlsContext, TlsStream, TlsVersion, TlsVerifyMode}

// TlsContext — wraps SSL_CTX
TlsContext::client() -> Outcome[TlsContext, NetError]     // Client context (TLS 1.2+ default)
TlsContext::server(cert, key) -> Outcome[TlsContext, NetError]  // Server context
ctx.set_verify_mode(TlsVerifyMode)                        // None, Peer, RequireClientCert
ctx.set_ca_file(path) -> Outcome[(), NetError]             // Load custom CA bundle
ctx.set_min_version(TlsVersion) -> Outcome[(), NetError]   // Force minimum version
ctx.set_max_version(TlsVersion) -> Outcome[(), NetError]   // Force maximum version
ctx.set_ciphers(cipher_list) -> Outcome[(), NetError]       // TLS 1.2 cipher list
ctx.set_ciphersuites(suites) -> Outcome[(), NetError]       // TLS 1.3 cipher suites

// TlsStream — wraps SSL
TlsStream::connect(ctx, socket_fd, hostname) -> Outcome[TlsStream, NetError]  // Client handshake + SNI
TlsStream::accept(ctx, socket_fd) -> Outcome[TlsStream, NetError]             // Server accept
stream.read(buf) -> Outcome[I64, NetError]                 // Encrypted read
stream.write(buf) -> Outcome[I64, NetError]                // Encrypted write
stream.write_str(s) -> Outcome[I64, NetError]              // String write
stream.version() -> Str                                    // e.g. "TLSv1.3"
stream.cipher() -> Str                                     // e.g. "TLS_AES_256_GCM_SHA384"
stream.alpn() -> Str                                       // Negotiated ALPN protocol
stream.peer_cn() -> Str                                    // Peer certificate CN
stream.peer_cert_pem() -> Str                              // PEM-encoded peer certificate
stream.verify_result() -> I32                              // 0 = X509_V_OK
stream.peer_verified() -> Bool                             // Certificate validation result
stream.shutdown()                                          // Graceful TLS close

// TlsVersion constants
TlsVersion::TLS_1_0()  // 0x0301
TlsVersion::TLS_1_1()  // 0x0302
TlsVersion::TLS_1_2()  // 0x0303 (minimum recommended)
TlsVersion::TLS_1_3()  // 0x0304

// TlsVerifyMode
TlsVerifyMode::None()              // No verification (testing only)
TlsVerifyMode::Peer()              // Verify server cert (standard for clients)
TlsVerifyMode::RequireClientCert() // Require client cert (mutual TLS servers)
```

### Platform notes

- **Windows**: CA certificates loaded from the Windows system certificate store via `wincrypt.h` (`CertOpenSystemStoreA("ROOT")`). No cert bundle file needed.
- **Linux/macOS**: Uses OpenSSL's `SSL_CTX_set_default_verify_paths()` to find system CA bundles.
- **Backend**: OpenSSL 3.x via vcpkg (Windows) or system packages (Linux/macOS).

## 10. Working Examples

### 10.1 HTTPS Client (TLS 1.3)

```tml
use std::net::dns
use std::net::ip::Ipv4Addr
use std::net::{SocketAddr, SocketAddrV4}
use std::net::tcp::TcpStream
use std::net::tls::{TlsContext, TlsStream, TlsVerifyMode}
use std::net::tls
use std::net::sys::RawSocket

func https_get(host: Str, path: Str) -> Outcome[Str, Str] {
    // 1. DNS resolution
    let ip: Ipv4Addr = dns::lookup(host).unwrap()
    let addr: SocketAddr = SocketAddr::V4(SocketAddrV4::new(ip, 443 as U16))

    // 2. TCP connection
    let tcp: TcpStream = TcpStream::connect(addr).unwrap()

    // 3. TLS handshake with certificate verification
    let ctx: TlsContext = TlsContext::client().unwrap()
    ctx.set_verify_mode(TlsVerifyMode::Peer())
    let raw: RawSocket = tcp.into_raw_socket()
    let fd: I64 = raw.handle
    let stream: TlsStream = TlsStream::connect(ctx, fd, host).unwrap()

    // 4. Send HTTP request over TLS
    let req: Str = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n"
    let _w = stream.write_str(req)

    // 5. Read response
    // ... (read loop using stream.read())

    stream.shutdown()
    Ok("done")
}
```

### 10.2 Force TLS 1.2 with Version Constraints

```tml
use std::net::dns
use std::net::ip::Ipv4Addr
use std::net::{SocketAddr, SocketAddrV4}
use std::net::tcp::TcpStream
use std::net::tls::{TlsContext, TlsStream, TlsVersion, TlsVerifyMode}
use std::net::tls
use std::net::sys::RawSocket

func connect_tls12(host: Str) -> I32 {
    let ip: Ipv4Addr = dns::lookup(host).unwrap()
    let addr: SocketAddr = SocketAddr::V4(SocketAddrV4::new(ip, 443 as U16))
    let tcp: TcpStream = TcpStream::connect(addr).unwrap()

    let ctx: TlsContext = TlsContext::client().unwrap()
    ctx.set_verify_mode(TlsVerifyMode::None())

    // Force TLS 1.2 only
    let _r1 = ctx.set_min_version(TlsVersion::TLS_1_2())
    let _r2 = ctx.set_max_version(TlsVersion::TLS_1_2())

    let raw: RawSocket = tcp.into_raw_socket()
    let fd: I64 = raw.handle
    let stream: TlsStream = TlsStream::connect(ctx, fd, host).unwrap()

    let ver: Str = stream.version()    // "TLSv1.2"
    let cipher: Str = stream.cipher()  // e.g. "ECDHE-ECDSA-CHACHA20-POLY1305"

    stream.shutdown()
    0
}
```

### 10.3 Certificate Verification

```tml
use std::net::dns
use std::net::ip::Ipv4Addr
use std::net::{SocketAddr, SocketAddrV4}
use std::net::tcp::TcpStream
use std::net::tls::{TlsContext, TlsStream, TlsVerifyMode}
use std::net::tls
use std::net::sys::RawSocket

func verify_certificate(host: Str) -> Bool {
    let ip: Ipv4Addr = dns::lookup(host).unwrap()
    let addr: SocketAddr = SocketAddr::V4(SocketAddrV4::new(ip, 443 as U16))
    let tcp: TcpStream = TcpStream::connect(addr).unwrap()

    // Enable certificate verification (validates against system CA store)
    let ctx: TlsContext = TlsContext::client().unwrap()
    ctx.set_verify_mode(TlsVerifyMode::Peer())

    let raw: RawSocket = tcp.into_raw_socket()
    let fd: I64 = raw.handle
    let stream: TlsStream = TlsStream::connect(ctx, fd, host).unwrap()

    // Check verification result
    let verify_code: I32 = stream.verify_result()  // 0 = X509_V_OK
    let verified: Bool = stream.peer_verified()     // true if valid CA chain

    // Inspect certificate
    let cn: Str = stream.peer_cn()                  // e.g. "*.google.com"
    let pem: Str = stream.peer_cert_pem()           // Full PEM certificate

    stream.shutdown()
    verified
}
```

## 11. Test Coverage

| Test File | Tests | What it validates |
|-----------|-------|-------------------|
| `tls_google.test.tml` | 2 | Handshake, version, cipher, CN, ALPN, PEM export |
| `tls_verify.test.tml` | 1 | Force TLS 1.2 via set_min/max_version |
| `tls_cloudflare.test.tml` | 1 | Force TLS 1.3 via set_min/max_version |
| `tls_cert_verify.test.tml` | 1 | Real certificate verification (Peer mode, X509_V_OK) |
| `tls_context.test.tml` | 5 | Context creation, version/cipher config, error paths |
| `tls_errors.test.tml` | 4 | Error handling, invalid host, connection failures |

**Coverage**: `std::net::tls` at 91.2% (31/34 functions covered).

---

*Previous: [05-CRYPTO.md](./05-CRYPTO.md)*
*Next: [07-HTTP.md](./07-HTTP.md) — HTTP Client and Server*
