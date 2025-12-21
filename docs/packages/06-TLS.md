# std.tls — TLS/SSL Connections

## 1. Overview

The `std.tls` package provides TLS (Transport Layer Security) for secure network communication.

```tml
import std.tls
import std.tls.{TlsConnector, TlsAcceptor, TlsStream}
```

## 2. Capabilities

```tml
caps: [io.network.tls]      // TLS connections
caps: [io.file.read]        // For loading certificates (optional)
```

## 3. Client Connections

### 3.1 TlsConnector

```tml
public type TlsConnector {
    config: TlsConfig,
}

extend TlsConnector {
    /// Create with default system roots
    public func new() -> Result[This, TlsError]

    /// Create with custom configuration
    public func with_config(config: TlsConfig) -> Result[This, TlsError]

    /// Connect to server
    public func connect[S: Read + Write](
        this,
        domain: &str,
        stream: S
    ) -> Result[TlsStream[S], TlsError]
    effects: [io.network.tls]

    /// Connect without hostname verification (dangerous!)
    public func connect_without_verification[S: Read + Write](
        this,
        stream: S
    ) -> Result[TlsStream[S], TlsError]
    effects: [io.network.tls]
}
```

### 3.2 TlsConnectorBuilder

```tml
public type TlsConnectorBuilder {
    min_version: Option[TlsVersion],
    max_version: Option[TlsVersion],
    root_certs: List[Certificate],
    client_cert: Option[(Certificate, PrivateKey)],
    alpn_protocols: List[String],
    verify_hostname: Bool,
    verify_cert: Bool,
}

extend TlsConnectorBuilder {
    public func new() -> This

    /// Set minimum TLS version
    public func min_version(this, version: TlsVersion) -> This

    /// Set maximum TLS version
    public func max_version(this, version: TlsVersion) -> This

    /// Add root certificate
    public func add_root_cert(this, cert: Certificate) -> This

    /// Load root certificates from file
    public func load_root_certs(this, path: &Path) -> Result[This, TlsError]
    effects: [io.file.read]

    /// Use system root certificates
    public func use_system_roots(this) -> Result[This, TlsError]

    /// Set client certificate for mutual TLS
    public func client_cert(this, cert: Certificate, key: PrivateKey) -> This

    /// Set ALPN protocols
    public func alpn_protocols(this, protocols: &[&str]) -> This

    /// Disable hostname verification (dangerous!)
    public func danger_disable_hostname_verification(this) -> This

    /// Disable certificate verification (dangerous!)
    public func danger_disable_cert_verification(this) -> This

    /// Build connector
    public func build(this) -> Result[TlsConnector, TlsError]
}
```

## 4. Server Connections

### 4.1 TlsAcceptor

```tml
public type TlsAcceptor {
    config: TlsConfig,
}

extend TlsAcceptor {
    /// Create with certificate and key
    public func new(cert: Certificate, key: PrivateKey) -> Result[This, TlsError]

    /// Create with certificate chain
    public func with_chain(
        certs: &[Certificate],
        key: PrivateKey
    ) -> Result[This, TlsError]

    /// Create with custom configuration
    public func with_config(config: TlsConfig) -> Result[This, TlsError]

    /// Accept TLS connection
    public func accept[S: Read + Write](this, stream: S) -> Result[TlsStream[S], TlsError]
    effects: [io.network.tls]
}
```

### 4.2 TlsAcceptorBuilder

```tml
public type TlsAcceptorBuilder {
    cert_chain: List[Certificate],
    private_key: Option[PrivateKey],
    min_version: Option[TlsVersion],
    max_version: Option[TlsVersion],
    client_auth: ClientAuth,
    alpn_protocols: List[String],
}

public type ClientAuth =
    | NoClientAuth
    | RequestClientCert
    | RequireClientCert
    | RequireAndVerifyClientCert(List[Certificate])

extend TlsAcceptorBuilder {
    public func new() -> This

    /// Set certificate and key
    public func identity(this, cert: Certificate, key: PrivateKey) -> This

    /// Set certificate chain
    public func cert_chain(this, certs: &[Certificate]) -> This

    /// Set private key
    public func private_key(this, key: PrivateKey) -> This

    /// Load from PKCS#12/PFX file
    public func load_pkcs12(this, path: &Path, password: &str) -> Result[This, TlsError]
    effects: [io.file.read]

    /// Set minimum TLS version
    public func min_version(this, version: TlsVersion) -> This

    /// Set maximum TLS version
    public func max_version(this, version: TlsVersion) -> This

    /// Set client authentication mode
    public func client_auth(this, auth: ClientAuth) -> This

    /// Set ALPN protocols
    public func alpn_protocols(this, protocols: &[&str]) -> This

    /// Build acceptor
    public func build(this) -> Result[TlsAcceptor, TlsError]
}
```

## 5. TLS Stream

### 5.1 TlsStream

```tml
public type TlsStream[S] {
    inner: S,
    session: TlsSession,
}

extend TlsStream[S: Read + Write] {
    /// Get peer certificate
    public func peer_certificate(this) -> Option[&Certificate]

    /// Get peer certificate chain
    public func peer_certificates(this) -> &[Certificate]

    /// Get negotiated ALPN protocol
    public func alpn_protocol(this) -> Option[&str]

    /// Get negotiated TLS version
    public func tls_version(this) -> TlsVersion

    /// Get negotiated cipher suite
    public func cipher_suite(this) -> CipherSuite

    /// Get SNI hostname (server only)
    public func sni_hostname(this) -> Option[&str]

    /// Check if handshake is complete
    public func is_handshake_complete(this) -> Bool

    /// Get underlying stream
    public func get_ref(this) -> &S

    /// Get mutable reference to underlying stream
    public func get_mut(this) -> &mut S

    /// Unwrap to underlying stream
    public func into_inner(this) -> S

    /// Initiate graceful shutdown
    public func shutdown(this) -> Result[Unit, TlsError]
    effects: [io.network.tls]
}

extend TlsStream[S: Read + Write] with Read {
    func read(this, buf: &mut [U8]) -> Result[U64, IoError]
    effects: [io.network.tls]
}

extend TlsStream[S: Read + Write] with Write {
    func write(this, buf: &[U8]) -> Result[U64, IoError]
    effects: [io.network.tls]

    func flush(this) -> Result[Unit, IoError]
    effects: [io.network.tls]
}
```

## 6. Certificates and Keys

### 6.1 Certificate

```tml
public type Certificate {
    der: List[U8],
}

extend Certificate {
    /// Parse from DER bytes
    public func from_der(der: &[U8]) -> Result[This, CertError]

    /// Parse from PEM string
    public func from_pem(pem: &str) -> Result[This, CertError]

    /// Parse multiple from PEM string
    public func from_pem_multiple(pem: &str) -> Result[List[This], CertError]

    /// Load from DER file
    public func load_der(path: &Path) -> Result[This, CertError]
    effects: [io.file.read]

    /// Load from PEM file
    public func load_pem(path: &Path) -> Result[This, CertError]
    effects: [io.file.read]

    /// Load multiple from PEM file
    public func load_pem_multiple(path: &Path) -> Result[List[This], CertError]
    effects: [io.file.read]

    /// Get DER bytes
    public func to_der(this) -> &[U8]

    /// Get PEM string
    public func to_pem(this) -> String

    /// Get subject name
    public func subject(this) -> Result[Name, CertError]

    /// Get issuer name
    public func issuer(this) -> Result[Name, CertError]

    /// Get validity period
    public func validity(this) -> Result[(SystemTime, SystemTime), CertError]

    /// Get serial number
    public func serial_number(this) -> Result[List[U8], CertError]

    /// Check if self-signed
    public func is_self_signed(this) -> Bool

    /// Get public key
    public func public_key(this) -> Result[PublicKey, CertError]
}
```

### 6.2 PrivateKey

```tml
public type PrivateKey {
    kind: KeyKind,
    der: List[U8],
}

public type KeyKind = Rsa | Ecdsa | Ed25519

extend PrivateKey {
    /// Parse from DER bytes (PKCS#8)
    public func from_der(der: &[U8]) -> Result[This, KeyError]

    /// Parse from PEM string
    public func from_pem(pem: &str) -> Result[This, KeyError]

    /// Load from DER file
    public func load_der(path: &Path) -> Result[This, KeyError]
    effects: [io.file.read]

    /// Load from PEM file
    public func load_pem(path: &Path) -> Result[This, KeyError>
    effects: [io.file.read]

    /// Load from encrypted PEM file
    public func load_encrypted_pem(path: &Path, password: &str) -> Result[This, KeyError]
    effects: [io.file.read]

    /// Get DER bytes
    public func to_der(this) -> &[U8]

    /// Get PEM string
    public func to_pem(this) -> String

    /// Get key type
    public func kind(this) -> KeyKind
}
```

### 6.3 PKCS#12

```tml
module pkcs12

public type Pkcs12 {
    cert: Certificate,
    chain: List[Certificate],
    key: PrivateKey,
}

extend Pkcs12 {
    /// Parse from DER bytes
    public func from_der(der: &[U8], password: &str) -> Result[This, Pkcs12Error]

    /// Load from file
    public func load(path: &Path, password: &str) -> Result[This, Pkcs12Error]
    effects: [io.file.read]

    /// Create PKCS#12 bundle
    public func create(
        cert: Certificate,
        key: PrivateKey,
        chain: &[Certificate],
        password: &str
    ) -> Result[List[U8], Pkcs12Error]
}
```

## 7. TLS Configuration

### 7.1 TlsVersion

```tml
public type TlsVersion = Tls10 | Tls11 | Tls12 | Tls13

extend TlsVersion {
    public const LATEST: This = Tls13
    public const MIN_SECURE: This = Tls12
}
```

### 7.2 CipherSuite

```tml
public type CipherSuite {
    id: U16,
    name: String,
}

// TLS 1.3 cipher suites
public const TLS_AES_128_GCM_SHA256: CipherSuite = ...
public const TLS_AES_256_GCM_SHA384: CipherSuite = ...
public const TLS_CHACHA20_POLY1305_SHA256: CipherSuite = ...

// TLS 1.2 cipher suites
public const TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256: CipherSuite = ...
public const TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384: CipherSuite = ...
public const TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256: CipherSuite = ...
public const TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384: CipherSuite = ...
```

### 7.3 TlsConfig

```tml
public type TlsConfig {
    min_version: TlsVersion,
    max_version: TlsVersion,
    cipher_suites: List[CipherSuite],
    alpn_protocols: List[String],
    root_certs: List[Certificate],
    client_cert: Option[(Certificate, PrivateKey)],
    server_cert: Option[(List[Certificate], PrivateKey)>,
    client_auth: ClientAuth,
    session_cache_size: U64,
    session_timeout: Duration,
}
```

## 8. Error Types

```tml
public type TlsError {
    kind: TlsErrorKind,
    message: String,
    source: Option[Box[dyn Error]],
}

public type TlsErrorKind =
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

public type AlertDescription =
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
module https_client
caps: [io.network.tcp, io.network.tls]

import std.net.TcpStream
import std.tls.{TlsConnector, TlsStream}
import std.io.{BufReader, BufWriter, Write}

public func https_get(host: &str, path: &str) -> Result[String, Error] {
    // TCP connection
    let tcp = TcpStream.connect((host, 443))?

    // TLS handshake
    let connector = TlsConnector.new()?
    let tls = connector.connect(host, tcp)?

    // Send HTTP request
    var writer = BufWriter.new(&tls)
    writer.write_all(b"GET ")?
    writer.write_all(path.as_bytes())?
    writer.write_all(b" HTTP/1.1\r\nHost: ")?
    writer.write_all(host.as_bytes())?
    writer.write_all(b"\r\nConnection: close\r\n\r\n")?
    writer.flush()?

    // Read response
    var response = String.new()
    tls.read_to_string(&mut response)?

    return Ok(response)
}
```

### 9.2 TLS Server

```tml
module tls_server
caps: [io.network.tcp, io.network.tls, io.file.read]

import std.net.TcpListener
import std.tls.{TlsAcceptor, Certificate, PrivateKey}
import std.thread

public func main() -> Result[Unit, Error] {
    // Load certificate and key
    let cert = Certificate.load_pem("server.crt")?
    let key = PrivateKey.load_pem("server.key")?

    // Create TLS acceptor
    let acceptor = TlsAcceptor.new(cert, key)?

    // Listen for connections
    let listener = TcpListener.bind("0.0.0.0:8443")?
    println("Listening on port 8443")

    loop (tcp, addr) in listener.incoming() {
        let tcp = tcp?
        let acceptor = acceptor.clone()

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

func handle_connection[S: Read + Write](stream: TlsStream[S]) -> Result[Unit, Error] {
    var buf: [U8; 1024] = [0; 1024]
    let n = stream.read(&mut buf)?

    let response = b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello"
    stream.write_all(response)?

    return Ok(unit)
}
```

### 9.3 Mutual TLS

```tml
module mtls
caps: [io.network.tls, io.file.read]

import std.tls.{TlsConnectorBuilder, TlsAcceptorBuilder, Certificate, PrivateKey, ClientAuth}

// Client with certificate
func create_mtls_client() -> Result[TlsConnector, Error] {
    let client_cert = Certificate.load_pem("client.crt")?
    let client_key = PrivateKey.load_pem("client.key")?
    let ca_cert = Certificate.load_pem("ca.crt")?

    return TlsConnectorBuilder.new()
        .add_root_cert(ca_cert)
        .client_cert(client_cert, client_key)
        .min_version(TlsVersion.Tls12)
        .build()
}

// Server requiring client certificate
func create_mtls_server() -> Result[TlsAcceptor, Error] {
    let server_cert = Certificate.load_pem("server.crt")?
    let server_key = PrivateKey.load_pem("server.key")?
    let ca_cert = Certificate.load_pem("ca.crt")?

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
