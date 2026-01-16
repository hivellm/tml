# std::tls — TLS/SSL Connections

## 1. Overview

The \x60std::tls` package provides TLS (Transport Layer Security) for secure network communication.

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
    pub func load_pem(path: ref Path) -> Outcome[This, KeyError>
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

## 9. Examples

### 9.1 HTTPS Client

```tml
mod https_client
caps: [io::network.tcp, io::network.tls]

use std::net.TcpStream
use std::tls.{TlsConnector, TlsStream}
use std::io.{BufReader, BufWriter, Write}

pub func https_get(host: ref str, path: ref str) -> Outcome[String, Error] {
    // TCP connection
    let tcp = TcpStream.connect((host, 443))!

    // TLS handshake
    let connector = TlsConnector.new()!
    let tls = connector.connect(host, tcp)!

    // Send HTTP request
    var writer = BufWriter.new(ref tls)
    writer.write_all(b"GET ")!
    writer.write_all(path.as_bytes())!
    writer.write_all(b" HTTP/1.1\r\nHost: ")!
    writer.write_all(host.as_bytes())!
    writer.write_all(b"\r\nConnection: close\r\n\r\n")!
    writer.flush()!

    // Read response
    var response = String.new()
    tls.read_to_string(&mut response)!

    return Ok(response)
}
```

### 9.2 TLS Server

```tml
mod tls_server
caps: [io::network.tcp, io::network.tls, io::file.read]

use std::net.TcpListener
use std::tls.{TlsAcceptor, Certificate, PrivateKey}
use std::thread

pub func main() -> Outcome[Unit, Error] {
    // Load certificate and key
    let cert = Certificate.load_pem("server.crt")!
    let key = PrivateKey.load_pem("server.key")!

    // Create TLS acceptor
    let acceptor = TlsAcceptor.new(cert, key)!

    // Listen for connections
    let listener = TcpListener.bind("0.0.0.0:8443")!
    println("Listening on port 8443")

    loop (tcp, addr) in listener.incoming() {
        let tcp = tcp?
        let acceptor = acceptor.duplicate()

        thread.spawn(do() {
            when acceptor.accept(tcp) {
                Ok(tls) -> {
                    println("TLS connection from: " + addr.to_string())
                    println("  Version: " + tls.tls_version().to_string())
                    println("  Cipher: " + tls.cipher_suite().name)
                    handle_connection(tls).ok()
                },
                Err(e) -> {
                    eprintln("TLS error: " + e.to_string())
                },
            }
        })
    }

    return Ok(unit)
}

func handle_connection[S: Read + Write](stream: TlsStream[S]) -> Outcome[Unit, Error] {
    var buf: [U8; 1024] = [0; 1024]
    let n = stream.read(&mut buf)!

    let response = b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello"
    stream.write_all(response)!

    return Ok(unit)
}
```

### 9.3 Mutual TLS

```tml
mod mtls
caps: [io::network.tls, io::file.read]

use std::tls.{TlsConnectorBuilder, TlsAcceptorBuilder, Certificate, PrivateKey, ClientAuth}

// Client with certificate
func create_mtls_client() -> Outcome[TlsConnector, Error] {
    let client_cert = Certificate.load_pem("client.crt")!
    let client_key = PrivateKey.load_pem("client.key")!
    let ca_cert = Certificate.load_pem("ca.crt")!

    return TlsConnectorBuilder.new()
        .add_root_cert(ca_cert)
        .client_cert(client_cert, client_key)
        .min_version(TlsVersion.Tls12)
        .build()
}

// Server requiring client certificate
func create_mtls_server() -> Outcome[TlsAcceptor, Error] {
    let server_cert = Certificate.load_pem("server.crt")!
    let server_key = PrivateKey.load_pem("server.key")!
    let ca_cert = Certificate.load_pem("ca.crt")!

    return TlsAcceptorBuilder.new()
        .identity(server_cert, server_key)
        .client_auth(ClientAuth.RequireAndVerifyClientCert([ca_cert]))
        .min_version(TlsVersion.Tls12)
        .build()
}
```

---

*Previous: [05-CRYPTO.md](./05-CRYPTO.md)*
*Next: [07-HTTP.md](./07-HTTP.md) — HTTP Client and Server*
