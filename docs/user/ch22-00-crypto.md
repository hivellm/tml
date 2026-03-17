# Chapter 22 — Cryptography

TML's `std::crypto` module provides cryptographic primitives backed by platform-
native implementations (OpenSSL on Linux/macOS, BCrypt/CNG on Windows). The API
is organized into sub-modules: `hash`, `hmac`, `cipher`, `kdf`, `sign`, `rsa`,
`key`, and `random`. All operations use TML's standard error handling —
`Outcome[T, CryptoError]` — so failures surface at compile time.

## Hashing

Hash functions map arbitrary data to a fixed-size digest. Use them for data
integrity checks, content addressing, and as building blocks in higher-level
constructs. Never use them alone for password storage.

### One-Shot Functions

```tml
use std::crypto::hash::{sha256, sha512, sha384, md5, sha1, Digest}

func main() -> I32 {
    let msg: Str = "Hello, TML!"

    var d256: Digest = sha256(msg)
    println("SHA-256: {d256.to_hex()}")
    println("Base64:  {d256.to_base64()}")
    d256.destroy()

    var d512: Digest = sha512(msg)
    println("SHA-512: {d512.to_hex()}")
    d512.destroy()

    return 0
}
```

Each function returns a `Digest` value. Call `.to_hex()` or `.to_base64()` to
encode it as a string. Call `.destroy()` when done to free native resources.

### Available Hash Functions

| Function | Digest Size | Notes |
|----------|-------------|-------|
| `md5(msg)` | 128-bit | Legacy only — not secure for new code |
| `sha1(msg)` | 160-bit | Legacy compatibility — avoid for security |
| `sha256(msg)` | 256-bit | Recommended general-purpose default |
| `sha384(msg)` | 384-bit | Higher-security alternative |
| `sha512(msg)` | 512-bit | Maximum security, slower on 32-bit |
| `sha512_256(msg)` | 256-bit | SHA-512 truncated — faster than SHA-256 on 64-bit |

### Streaming Hash

For data that arrives in chunks — large files, network streams — use the
incremental interface to avoid loading everything into memory:

```tml
use std::crypto::hash::{HashStream, HashAlgorithm}

var stream: HashStream = HashStream::new(HashAlgorithm::SHA256)
stream.update("first chunk of data")
stream.update("second chunk of data")
stream.update("third chunk of data")
var digest: Digest = stream.finalize()
println(digest.to_hex())
digest.destroy()
```

### Hashing a File

```tml
use std::crypto::hash::{HashStream, HashAlgorithm, Digest}
use std::file::File

func hash_file(path: Str) -> Outcome[Str, Error] {
    let f = File::open(path)!
    var stream: HashStream = HashStream::new(HashAlgorithm::SHA256)

    let reader = BufReader::new(f)
    loop chunk in reader.chunks(8192) {
        stream.update(chunk!)
    }

    var digest: Digest = stream.finalize()
    let hex: Str = digest.to_hex()
    digest.destroy()
    Ok(hex)
}
```

## HMAC — Message Authentication

HMAC (Hash-based Message Authentication Code) proves that a message came from
a party holding a specific secret key and that it has not been tampered with.
It is the building block for API authentication and JWT verification.

```tml
use std::crypto::hmac::{hmac_sha256, hmac_sha512, HmacDigest}

var mac: HmacDigest = hmac_sha256("my-secret-key", "message to authenticate")
println(mac.to_hex())
mac.destroy()
```

### Verifying an HMAC

Use `verify_hmac` for constant-time comparison. Never compare HMAC values with
`==` — that leaks timing information and enables timing attacks:

```tml
use std::crypto::hmac::verify_hmac

let key: Str = "my-secret-key"
let message: Str = "important payload"
let received_hex: Str = "a1b2c3..."

let valid: Bool = verify_hmac(key, message, received_hex, "sha256")
if not valid {
    return Err(Error::new("HMAC verification failed"))
}
```

### Available HMAC Functions

| Function | Description |
|----------|-------------|
| `hmac_sha256(key, msg)` | HMAC-SHA-256 — most common |
| `hmac_sha384(key, msg)` | HMAC-SHA-384 |
| `hmac_sha512(key, msg)` | HMAC-SHA-512 |
| `verify_hmac(key, msg, hex, algo)` | Constant-time verification |

## Symmetric Encryption

Symmetric encryption uses the same key to encrypt and decrypt. TML supports AES
and ChaCha20. Always prefer authenticated modes (GCM, Poly1305) — they detect
tampering automatically without a separate HMAC step.

### AES-256-GCM (Recommended)

```tml
use std::crypto::cipher::{Cipher, Decipher, CipherAlgorithm}
use std::crypto::random::random_bytes

func encrypt_message(key: ref Buffer, plaintext: Str) -> Outcome[(Buffer, Buffer, Buffer), Error] {
    // Generate a fresh 96-bit nonce for each message
    let nonce: Buffer = random_bytes(12)

    var enc: Cipher = Cipher::new(CipherAlgorithm::AES_256_GCM, key, ref nonce)
    let ciphertext: Buffer = enc.encrypt(plaintext)
    let tag: Buffer = enc.get_auth_tag()
    enc.destroy()

    Ok((ciphertext, nonce, tag))
}

func decrypt_message(key: ref Buffer, ciphertext: ref Buffer, nonce: ref Buffer, tag: ref Buffer) -> Outcome[Str, Error] {
    var dec: Decipher = Decipher::new(CipherAlgorithm::AES_256_GCM, key, nonce)
    dec.set_auth_tag(tag)
    let plaintext: Str = dec.decrypt(ciphertext)
    dec.destroy()
    Ok(plaintext)
}
```

### ChaCha20-Poly1305

A strong alternative to AES-GCM, particularly on platforms without AES hardware
acceleration:

```tml
use std::crypto::cipher::{Cipher, Decipher, CipherAlgorithm}
use std::crypto::random::random_bytes

let key: Buffer = random_bytes(32)
let nonce: Buffer = random_bytes(12)

var enc: Cipher = Cipher::new(CipherAlgorithm::CHACHA20_POLY1305, ref key, ref nonce)
let ciphertext: Buffer = enc.encrypt("secret data")
let tag: Buffer = enc.get_auth_tag()
enc.destroy()
```

### Cipher Algorithm Reference

| Algorithm | Key Size | Nonce/IV | Notes |
|-----------|----------|----------|-------|
| `AES_128_GCM` | 16 bytes | 12 bytes | Authenticated; fast with AES-NI |
| `AES_256_GCM` | 32 bytes | 12 bytes | Recommended default |
| `AES_128_CBC` | 16 bytes | 16 bytes | Legacy — requires separate HMAC |
| `AES_256_CBC` | 32 bytes | 16 bytes | Legacy — requires separate HMAC |
| `AES_128_CTR` | 16 bytes | 16 bytes | Stream mode — no authentication |
| `AES_256_CTR` | 32 bytes | 16 bytes | Stream mode — no authentication |
| `CHACHA20_POLY1305` | 32 bytes | 12 bytes | Authenticated; fast without hardware AES |

### Streaming Encryption

For large files, encrypt in chunks to avoid holding the entire plaintext in
memory:

```tml
use std::crypto::cipher::{Cipher, CipherAlgorithm}
use std::crypto::random::random_bytes
use std::file::File

func encrypt_file(src: Str, dst: Str, key: ref Buffer) -> Outcome[Unit, Error] {
    let nonce: Buffer = random_bytes(12)
    var enc: Cipher = Cipher::new(CipherAlgorithm::AES_256_GCM, key, ref nonce)

    let in_file  = File::open(src)!
    let out_file = File::create(dst)!

    let reader = BufReader::new(in_file)
    loop chunk in reader.chunks(65536) {
        enc.update(chunk!)
        let partial: Buffer = enc.flush_output()
        out_file.write(ref partial)!
    }

    let final_block: Buffer = enc.finalize()
    let tag: Buffer = enc.get_auth_tag()
    out_file.write(ref final_block)!
    out_file.write(ref tag)!  // append 16-byte auth tag
    enc.destroy()

    Ok(unit)
}
```

## Key Derivation (KDF)

Key derivation functions convert low-entropy secrets (passwords, shared keys)
into high-entropy cryptographic keys. Always derive encryption keys from
passwords — never use a password as a key directly.

### PBKDF2 — Password Hashing

The standard choice for password storage. Use at least 100,000 iterations;
increase as hardware speeds up:

```tml
use std::crypto::kdf::pbkdf2
use std::crypto::random::random_bytes

func hash_password(password: Str) -> (Buffer, Buffer) {
    let salt: Buffer = random_bytes(32)  // always generate a fresh salt

    let key: Buffer = pbkdf2(
        password,
        ref salt,
        200000,   // iterations
        32,       // output length in bytes
        "sha256"  // underlying hash
    )

    (salt, key)
}

func verify_password(password: Str, salt: ref Buffer, stored_key: ref Buffer) -> Bool {
    let derived: Buffer = pbkdf2(password, salt, 200000, 32, "sha256")
    derived == *stored_key  // constant-time comparison handled internally
}
```

### HKDF — Key Derivation from Shared Secrets

Extract-and-expand: derive one or more subkeys from a single input key material.
Commonly used in TLS handshakes and key agreement protocols:

```tml
use std::crypto::kdf::hkdf

// Derive a 32-byte encryption key and a 32-byte MAC key from one shared secret
let shared_secret: Str = "ecdh-derived-shared-secret"

let enc_key: Buffer = hkdf(shared_secret, "application-salt", "encryption", 32, "sha256")
let mac_key: Buffer = hkdf(shared_secret, "application-salt", "authentication", 32, "sha256")
```

### KDF Reference

| Function | Signature | Use Case |
|----------|-----------|----------|
| `pbkdf2` | `(pass, salt, iters, len, algo)` | Password hashing |
| `hkdf` | `(ikm, salt, info, len, algo)` | Key derivation from shared secrets |
| `scrypt` | `(pass, salt, n, r, p, len)` | Memory-hard password hashing |
| `argon2` | `(pass, salt, time, mem, par, len)` | Modern recommended password hashing |

For new applications, prefer **Argon2id** — it is memory-hard, resistant to
GPU attacks, and the current OWASP recommendation:

```tml
use std::crypto::kdf::argon2
use std::crypto::random::random_bytes

let salt: Buffer = random_bytes(16)
let key: Buffer = argon2(
    "user-password",
    ref salt,
    3,      // time cost (iterations)
    65536,  // memory cost in kilobytes (64 MB)
    4,      // parallelism
    32      // output length
)
```

## Cryptographically Secure Random Numbers

`std::crypto::random` draws from the OS CSPRNG (`/dev/urandom` on Linux/macOS,
`BCryptGenRandom` on Windows). Use this for keys, nonces, salts, and tokens —
never for statistical simulations (use `std::random` for that).

```tml
use std::crypto::random::{random_bytes, random_int, random_float, random_uuid}

// Keys and nonces
let aes_key: Buffer  = random_bytes(32)  // 256-bit AES key
let nonce: Buffer    = random_bytes(12)  // 96-bit GCM nonce
let salt: Buffer     = random_bytes(32)  // 256-bit KDF salt

// Random integers in a range
let token_id: I64 = random_int(1, 1000000)

// Random floats
let probability: F64 = random_float()  // [0.0, 1.0)

// Random UUID v4
let request_id: Str = random_uuid()  // e.g. "550e8400-e29b-41d4-a716-446655440000"
```

## Digital Signatures

Digital signatures let a receiver verify that a message was produced by the
holder of a specific private key and has not been modified since signing.

### Ed25519 (Recommended)

Ed25519 is fast, small (32-byte keys, 64-byte signatures), and resistant to
common implementation pitfalls:

```tml
use std::crypto::key::{generate_key_pair, KeyPair}
use std::crypto::sign::{sign, verify}

func main() -> Outcome[Unit, Error] {
    // Generate a key pair (do this once and persist the keys securely)
    var kp: KeyPair = generate_key_pair("ed25519")

    let message: Str = "message to sign"

    // Sign
    let signature: Buffer = sign(ref kp.private_key, message, "ed25519")

    // Verify
    let valid: Bool = verify(ref kp.public_key, message, ref signature, "ed25519")
    println("valid: {valid}")  // true

    // Tampered message — verification fails
    let tampered: Bool = verify(ref kp.public_key, "wrong message", ref signature, "ed25519")
    println("tampered: {tampered}")  // false

    kp.destroy()
    Ok(unit)
}
```

### Signature Algorithm Reference

| Algorithm | Private Key | Public Key | Signature | Notes |
|-----------|-------------|------------|-----------|-------|
| `ed25519` | 32 bytes | 32 bytes | 64 bytes | Recommended |
| `ecdsa_p256` | 32 bytes | 64 bytes | 64 bytes | NIST P-256 |
| `ecdsa_p384` | 48 bytes | 96 bytes | 96 bytes | NIST P-384 |
| `rsa_pkcs1` | variable | variable | key-size | Legacy RSA padding |
| `rsa_pss` | variable | variable | key-size | Modern RSA padding |

### Serializing Keys

```tml
use std::crypto::key::{KeyPair, import_private_key, import_public_key}

// Export keys as PEM strings
let priv_pem: Str = kp.private_key.to_pem()
let pub_pem: Str  = kp.public_key.to_pem()

// Save to files
File::write_string("private.pem", priv_pem)!
File::write_string("public.pem", pub_pem)!

// Import from PEM
let priv_pem_str = File::read_to_string("private.pem")!
let imported_kp: KeyPair = import_private_key(priv_pem_str, "ed25519")!
```

## RSA Asymmetric Encryption

RSA is used when you need to encrypt a small secret for a known recipient
(using their public key), such as encrypting a session key:

```tml
use std::crypto::rsa::{rsa_encrypt, rsa_decrypt}
use std::crypto::key::{generate_key_pair, KeyPair}

var kp: KeyPair = generate_key_pair("rsa2048")

// Encrypt with public key (anyone can do this)
let ciphertext: Buffer = rsa_encrypt(ref kp.public_key, "session-key-bytes", "oaep-sha256")

// Decrypt with private key (only the key holder can do this)
let plaintext: Str = rsa_decrypt(ref kp.private_key, ref ciphertext, "oaep-sha256")

println(plaintext)  // "session-key-bytes"
kp.destroy()
```

RSA should only encrypt small values (typically symmetric keys). For bulk data
encryption, use RSA to encrypt an AES key and AES to encrypt the data itself.

## X.509 Certificates

Parse and inspect TLS certificates:

```tml
use std::crypto::x509::{Certificate, parse_cert}

let pem: Str = File::read_to_string("server.crt")!
let cert: Certificate = parse_cert(pem)!

println(cert.subject())      // "CN=example.com"
println(cert.issuer())       // "CN=Let's Encrypt Authority X3"
println(cert.not_after())    // expiry timestamp
println(cert.is_expired())   // false
```

## Error Handling

Every cryptographic operation that can fail returns `Outcome[T, CryptoError]`.
The `!` operator propagates errors automatically:

```tml
use std::crypto::cipher::{Cipher, CipherAlgorithm}
use std::crypto::error::{CryptoError, CryptoErrorKind}

func safe_decrypt(key: ref Buffer, ciphertext: ref Buffer, nonce: ref Buffer, tag: ref Buffer) -> Outcome[Str, CryptoError] {
    var dec: Decipher = Decipher::new(CipherAlgorithm::AES_256_GCM, key, nonce)
    dec.set_auth_tag(tag)
    let result: Str = dec.decrypt(ciphertext)
    dec.destroy()
    Ok(result)
}

func main() -> Outcome[Unit, Error] {
    when safe_decrypt(ref key, ref ct, ref nonce, ref bad_tag) {
        Ok(text) => println(text),
        Err(e) => when e.kind() {
            CryptoErrorKind::AuthenticationFailed =>
                println("ciphertext was tampered with"),
            CryptoErrorKind::InvalidKeyLength =>
                println("key is the wrong size for this algorithm"),
            _ =>
                println("crypto error: {e.message()}"),
        },
    }
    Ok(unit)
}
```

### Error Kinds

| Kind | Cause |
|------|-------|
| `InvalidKeyLength` | Key size does not match the algorithm requirements |
| `InvalidNonce` | Nonce or IV is the wrong size |
| `AuthenticationFailed` | GCM or Poly1305 authentication tag verification failed |
| `DecryptionFailed` | Ciphertext corrupted or key/nonce do not match |
| `InvalidPadding` | CBC padding is incorrect |
| `UnsupportedAlgorithm` | Algorithm not available on the current platform |
| `KeyGenerationFailed` | OS random source failed |

## Best Practices

1. **Use AES-256-GCM or ChaCha20-Poly1305 for encryption.** Both are
   authenticated — they detect tampering without a separate HMAC.

2. **Never reuse a nonce with the same key.** Each message must have a unique,
   randomly generated nonce.

3. **Use SHA-256 or SHA-512 for hashing.** Avoid MD5 and SHA-1 for any
   security-sensitive purpose.

4. **Use Argon2id or PBKDF2 for passwords.** Never hash passwords with plain
   SHA-256.

5. **Use `verify_hmac` for constant-time comparison.** Never compare
   authentication tags or HMAC values with `==`.

6. **Call `.destroy()`** on `Digest`, `HmacDigest`, `Cipher`, `Decipher`, and
   `KeyPair` objects. These hold native resources that are not freed by TML's
   ownership system.

7. **Use Ed25519 for signatures.** It is fast, small, and has a simple API
   with no foot-guns.

8. **Generate fresh keys with `random_bytes`.** Never derive keys directly from
   passwords — always use a KDF with a random salt.

## See Also

- [Chapter 23 — Compression](ch23-00-compression.md) — CRC32 checksums and data compression
- [Chapter 24 — Networking and HTTP](ch24-00-networking.md) — TLS in network connections
- [Chapter 20 — Standard Library](ch20-00-standard-library.md) — Overview of all modules

---

*Previous: [Chapter 21 — Working with JSON](ch21-00-json.md)*
*Next: [Chapter 23 — Compression](ch23-00-compression.md)*
