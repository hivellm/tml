# std::crypto — Cryptographic Primitives

## 1. Overview

The `std::crypto` module provides comprehensive cryptographic functionality: hashing, HMAC, symmetric encryption, digital signatures, key management, key derivation, and cryptographically secure random number generation.

```tml
use std::crypto::hash::{sha256, sha512, md5, Digest}
use std::crypto::hmac::{hmac_sha256, Hmac}
use std::crypto::cipher::{Cipher, Decipher, CipherAlgorithm}
use std::crypto::sign::{sign, verify}
use std::crypto::key::{generate_key, generate_key_pair}
use std::crypto::kdf::{pbkdf2, hkdf}
use std::crypto::random::{random_bytes, random_int, random_uuid}
```

**Module path:** `std::crypto`

**Runtime:** Backed by platform-native cryptography (Windows BCRYPT/CNG, macOS SecFramework, OpenSSL on Linux) via FFI.

## 2. Submodules

| Module | Description |
|--------|-------------|
| `std::crypto::hash` | Cryptographic hash functions (MD5, SHA-1, SHA-2) |
| `std::crypto::hmac` | Hash-based Message Authentication Codes |
| `std::crypto::cipher` | Symmetric encryption/decryption (AES, ChaCha20) |
| `std::crypto::sign` | Digital signatures (RSA, ECDSA, Ed25519) |
| `std::crypto::key` | Key generation and management |
| `std::crypto::kdf` | Key derivation (PBKDF2, scrypt, HKDF, Argon2) |
| `std::crypto::random` | Cryptographically secure random generation |
| `std::crypto::rsa` | RSA encryption/decryption |
| `std::crypto::dh` | Diffie-Hellman key exchange |
| `std::crypto::ecdh` | Elliptic Curve Diffie-Hellman |
| `std::crypto::x509` | X.509 certificate parsing |
| `std::crypto::error` | Error types (CryptoError, CryptoErrorKind) |
| `std::crypto::constants` | Cryptographic constants |

## 3. Hash Functions

### 3.1 One-Shot Hashing

```tml
use std::crypto::hash::{md5, sha1, sha256, sha384, sha512, sha512_256, Digest}

// Hash a string — returns a Digest object
var d: Digest = sha256("Hello, TML!")
println(d.to_hex())    // hex-encoded hash string
println(d.to_base64()) // base64-encoded hash string
d.destroy()            // free resources

// Other algorithms
var d_md5: Digest = md5("data")
var d_sha1: Digest = sha1("data")
var d_sha384: Digest = sha384("data")
var d_sha512: Digest = sha512("data")
var d_sha512_256: Digest = sha512_256("data")
```

### 3.2 Buffer Hashing

```tml
use std::crypto::hash::{sha256_bytes, md5_bytes}

let buf = Buffer::from("binary data")
var d: Digest = sha256_bytes(ref buf)
println(d.to_hex())
d.destroy()
```

### 3.3 Streaming Hash (Incremental)

```tml
use std::crypto::hash::{Hash, HashAlgorithm}

// Create a streaming hasher
var hasher = Hash::create(HashAlgorithm::Sha256)

// Feed data incrementally
hasher.update("Hello, ")
hasher.update("world!")

// Or feed binary data
let buf = Buffer::from("more data")
hasher.update_bytes(ref buf)

// Finalize (can only call once)
var digest: Digest = hasher.digest()
println(digest.to_hex())
digest.destroy()
hasher.destroy()
```

### 3.4 Copy for Parallel Computation

```tml
use std::crypto::hash::{Hash, HashAlgorithm}

var hasher = Hash::create(HashAlgorithm::Sha256)
hasher.update("common prefix")

// Fork the hasher state
var branch1 = hasher.copy()
var branch2 = hasher.copy()

branch1.update(" suffix A")
branch2.update(" suffix B")

var d1 = branch1.digest()
var d2 = branch2.digest()
// d1 and d2 are different hashes with the same prefix
```

### 3.5 Digest Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `to_hex` | `(self) -> Str` | Hex-encoded hash string |
| `to_base64` | `(self) -> Str` | Base64-encoded hash string |
| `bytes` | `(self) -> ref Buffer` | Raw hash bytes |
| `destroy` | `(mut self)` | Free resources |

### 3.6 Supported Algorithms

| Algorithm | Output Size | `HashAlgorithm` Variant | Notes |
|-----------|-------------|------------------------|-------|
| MD5 | 128-bit (16 bytes) | `Md5` | Legacy only, not secure |
| SHA-1 | 160-bit (20 bytes) | `Sha1` | Deprecated for security |
| SHA-256 | 256-bit (32 bytes) | `Sha256` | **Recommended** |
| SHA-384 | 384-bit (48 bytes) | `Sha384` | Higher security margin |
| SHA-512 | 512-bit (64 bytes) | `Sha512` | Best for 64-bit platforms |
| SHA-512/256 | 256-bit (32 bytes) | `Sha512_256` | Truncated SHA-512 |

### 3.7 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `md5` | `(data: Str) -> Digest` | MD5 hash of string |
| `md5_bytes` | `(data: ref Buffer) -> Digest` | MD5 hash of buffer |
| `sha1` | `(data: Str) -> Digest` | SHA-1 hash |
| `sha256` | `(data: Str) -> Digest` | SHA-256 hash |
| `sha256_bytes` | `(data: ref Buffer) -> Digest` | SHA-256 hash of buffer |
| `sha384` | `(data: Str) -> Digest` | SHA-384 hash |
| `sha512` | `(data: Str) -> Digest` | SHA-512 hash |
| `sha512_256` | `(data: Str) -> Digest` | SHA-512/256 hash |
| `Hash::create` | `(algorithm: HashAlgorithm) -> Hash` | Create streaming hasher |

## 4. HMAC (Message Authentication)

### 4.1 One-Shot HMAC

```tml
use std::crypto::hmac::{hmac_sha256, hmac_sha512, HmacDigest}

var mac: HmacDigest = hmac_sha256("secret-key", "message to authenticate")
println(mac.to_hex())
mac.destroy()

// Other variants
var mac_512 = hmac_sha512("key", "message")
```

### 4.2 Streaming HMAC

```tml
use std::crypto::hmac::{Hmac, HashAlgorithm}

var hmac = Hmac::create(HashAlgorithm::Sha256, "my-secret-key")
hmac.update("chunk 1")
hmac.update("chunk 2")
var digest: HmacDigest = hmac.digest()
println(digest.to_hex())
digest.destroy()
hmac.destroy()
```

### 4.3 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `hmac_md5` | `(key: Str, data: Str) -> HmacDigest` | HMAC-MD5 |
| `hmac_sha1` | `(key: Str, data: Str) -> HmacDigest` | HMAC-SHA1 |
| `hmac_sha256` | `(key: Str, data: Str) -> HmacDigest` | HMAC-SHA256 (recommended) |
| `hmac_sha384` | `(key: Str, data: Str) -> HmacDigest` | HMAC-SHA384 |
| `hmac_sha512` | `(key: Str, data: Str) -> HmacDigest` | HMAC-SHA512 |
| `Hmac::create` | `(algo: HashAlgorithm, key: Str) -> Hmac` | Create streaming HMAC |

## 5. Symmetric Encryption (AES, ChaCha20)

### 5.1 Cipher Algorithms

```tml
use std::crypto::cipher::CipherAlgorithm

// AES modes
CipherAlgorithm::Aes128Cbc    // AES-128 CBC
CipherAlgorithm::Aes256Cbc    // AES-256 CBC
CipherAlgorithm::Aes128Gcm    // AES-128 GCM (AEAD)
CipherAlgorithm::Aes256Gcm    // AES-256 GCM (AEAD)
CipherAlgorithm::Aes128Ctr    // AES-128 CTR (stream)
CipherAlgorithm::Aes256Ctr    // AES-256 CTR (stream)

// ChaCha20 variants
CipherAlgorithm::ChaCha20Poly1305   // ChaCha20-Poly1305 (AEAD)
CipherAlgorithm::XChaCha20Poly1305  // XChaCha20-Poly1305 (extended nonce)
```

### 5.2 CipherAlgorithm Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `name` | `(self) -> Str` | Algorithm name |
| `key_size` | `(self) -> I64` | Required key size in bytes |
| `iv_size` | `(self) -> I64` | Required IV/nonce size |
| `block_size` | `(self) -> I64` | Block size |
| `is_aead` | `(self) -> Bool` | Whether it supports authenticated encryption |
| `tag_size` | `(self) -> I64` | Authentication tag size (AEAD only) |

### 5.3 Encryption / Decryption

```tml
use std::crypto::cipher::{Cipher, Decipher, CipherAlgorithm}

// Encrypt
var cipher = Cipher::new(CipherAlgorithm::Aes256Gcm, ref key, ref iv).unwrap()
cipher.set_aad_str("additional authenticated data").unwrap()  // AEAD only
cipher.update("plaintext")
let ciphertext: Buffer = cipher.finalize().unwrap()
let tag: AuthTag = cipher.get_auth_tag().unwrap()  // AEAD only
cipher.destroy()

// Decrypt
var decipher = Decipher::new(CipherAlgorithm::Aes256Gcm, ref key, ref iv).unwrap()
decipher.set_aad_str("additional authenticated data").unwrap()
decipher.set_auth_tag(ref tag).unwrap()  // AEAD only
decipher.update_bytes(ref ciphertext)
let plaintext: Buffer = decipher.finalize().unwrap()
decipher.destroy()
```

### 5.4 Convenience Functions

```tml
use std::crypto::cipher::{
    aes_encrypt, aes_decrypt,
    aes_gcm_encrypt, aes_gcm_decrypt,
    encrypt_string, decrypt_string
}

// Simple AES-CBC encrypt/decrypt
let ciphertext = aes_encrypt(ref key, ref iv, ref plaintext).unwrap()
let plaintext = aes_decrypt(ref key, ref iv, ref ciphertext).unwrap()

// AES-GCM with authentication
let (ciphertext, tag) = aes_gcm_encrypt(ref key, ref nonce, ref plaintext, ref aad).unwrap()
let plaintext = aes_gcm_decrypt(ref key, ref nonce, ref ciphertext, ref aad, ref tag).unwrap()

// String convenience (auto-generates IV, base64 output)
let encrypted_b64: Str = encrypt_string(ref key, "secret message").unwrap()
let decrypted: Str = decrypt_string(ref key, encrypted_b64).unwrap()
```

### 5.5 Supported Cipher Algorithms

| Algorithm | Key Size | IV/Nonce | AEAD | Notes |
|-----------|----------|----------|------|-------|
| AES-128-CBC | 16 | 16 | No | Standard block cipher |
| AES-192-CBC | 24 | 16 | No | Extended key |
| AES-256-CBC | 32 | 16 | No | Maximum security |
| AES-128-CTR | 16 | 16 | No | Stream mode |
| AES-256-CTR | 32 | 16 | No | Stream mode |
| AES-128-GCM | 16 | 12 | Yes | **Recommended** |
| AES-256-GCM | 32 | 12 | Yes | **Recommended** |
| AES-128-CCM | 16 | 12 | Yes | Constrained environments |
| AES-256-CCM | 32 | 12 | Yes | Constrained environments |
| ChaCha20-Poly1305 | 32 | 12 | Yes | Fast on platforms without AES-NI |
| XChaCha20-Poly1305 | 32 | 24 | Yes | Extended nonce (safer) |

## 6. Digital Signatures

### 6.1 Signing and Verification

```tml
use std::crypto::sign::{sign, verify, SignatureAlgorithm}
use std::crypto::key::{generate_key_pair, KeyType}

// Generate key pair
let pair = generate_key_pair(KeyType::Ed25519, 0).unwrap()

// Sign
let signature: Buffer = sign(
    SignatureAlgorithm::Ed25519,
    ref pair.private_key,
    "message to sign"
).unwrap()

// Verify
let valid: Bool = verify(
    SignatureAlgorithm::Ed25519,
    ref pair.public_key,
    "message to sign",
    ref signature
).unwrap()
```

### 6.2 Signature Algorithms

| Algorithm | `SignatureAlgorithm` | Notes |
|-----------|---------------------|-------|
| RSA PKCS#1 v1.5 + SHA-256 | `RsaSha256` | Traditional RSA |
| RSA PKCS#1 v1.5 + SHA-512 | `RsaSha512` | |
| RSA-PSS + SHA-256 | `RsaPssSha256` | Recommended RSA |
| RSA-PSS + SHA-512 | `RsaPssSha512` | |
| ECDSA + SHA-256 | `EcdsaSha256` | Elliptic curve |
| ECDSA + SHA-384 | `EcdsaSha384` | |
| Ed25519 | `Ed25519` | **Recommended** |
| Ed448 | `Ed448` | Higher security margin |

## 7. Key Management

### 7.1 Symmetric Keys

```tml
use std::crypto::key::{generate_key, create_secret_key, SecretKey}

// Generate random symmetric key (32 bytes for AES-256)
let key: SecretKey = generate_key(32).unwrap()

// Create from existing bytes
let key: SecretKey = create_secret_key(ref my_buffer)
```

### 7.2 Asymmetric Key Pairs

```tml
use std::crypto::key::{generate_key_pair, KeyType, KeyPair}

// Generate RSA key pair
let rsa_pair: KeyPair = generate_key_pair(KeyType::Rsa, 2048).unwrap()

// Generate Ed25519 key pair (key_size ignored)
let ed_pair: KeyPair = generate_key_pair(KeyType::Ed25519, 0).unwrap()

// Generate EC P-256 key pair
let ec_pair: KeyPair = generate_key_pair(KeyType::Ec, 256).unwrap()
```

### 7.3 Key Import

```tml
use std::crypto::key::{create_private_key, create_public_key, KeyFormat}

let private_key = create_private_key(pem_string, KeyFormat::Pem).unwrap()
let public_key = create_public_key(pem_string, KeyFormat::Pem).unwrap()
```

### 7.4 Key Types

| `KeyType` | Description | Typical Sizes |
|-----------|-------------|---------------|
| `Rsa` | RSA | 2048, 3072, 4096 bits |
| `RsaPss` | RSA-PSS | 2048, 3072, 4096 bits |
| `Ec` | Elliptic Curve (P-256, P-384, P-521) | 256, 384, 521 bits |
| `Ed25519` | Edwards curve 25519 | Fixed 256-bit |
| `Ed448` | Edwards curve 448 | Fixed 448-bit |
| `X25519` | X25519 key exchange | Fixed 256-bit |
| `X448` | X448 key exchange | Fixed 448-bit |
| `Dh` | Diffie-Hellman | 2048+ bits |
| `Dsa` | DSA (legacy) | 2048+ bits |

## 8. Key Derivation Functions

### 8.1 PBKDF2

```tml
use std::crypto::kdf::{pbkdf2}
use std::crypto::hash::HashAlgorithm

let salt = Buffer::from("random-salt-value")
let derived_key: Buffer = pbkdf2(
    "user-password",       // password
    ref salt,              // salt
    100000,                // iterations (100K+ recommended)
    32,                    // output key length
    HashAlgorithm::Sha256  // digest algorithm
).unwrap()
```

### 8.2 HKDF (Extract-and-Expand)

```tml
use std::crypto::kdf::{hkdf, hkdf_extract, hkdf_expand}
use std::crypto::hash::HashAlgorithm

let salt = Buffer::from("salt")
let ikm = Buffer::from("input key material")

// Combined extract + expand
let derived: Buffer = hkdf(
    HashAlgorithm::Sha256,
    ref ikm,
    ref salt,
    "context info",  // info string
    32               // output length
).unwrap()

// Or separately
let prk: Buffer = hkdf_extract(HashAlgorithm::Sha256, ref ikm, ref salt).unwrap()
let output: Buffer = hkdf_expand(HashAlgorithm::Sha256, ref prk, "info", 32).unwrap()
```

### 8.3 KDF Comparison

| KDF | Use Case | Notes |
|-----|----------|-------|
| **PBKDF2** | Password hashing | FIPS compliant, use 100K+ iterations |
| **scrypt** | Password hashing | Memory-hard, GPU resistant |
| **HKDF** | Key derivation from DH | Not for passwords |
| **Argon2** | Password hashing | **Recommended** for new applications |

## 9. Cryptographically Secure Random

### 9.1 Random Generation

```tml
use std::crypto::random::{
    random_bytes, random_fill, random_int, random_uuid, timing_safe_equal
}

// Generate random bytes
let buf: Buffer = random_bytes(32)

// Fill existing buffer with random data
var buf = Buffer::new(64)
random_fill(mut ref buf)

// Random integer in range
let n: I64 = random_int(1, 100)  // [1, 100]

// Random UUID v4
let uuid: Str = random_uuid()  // e.g., "550e8400-e29b-41d4-a716-446655440000"

// Constant-time comparison (prevents timing attacks)
let equal: Bool = timing_safe_equal(ref buf_a, ref buf_b)
```

### 9.2 Platform Implementation

| Platform | CSPRNG Source |
|----------|--------------|
| Windows | BCryptGenRandom (CNG) |
| Linux | getrandom() syscall |
| macOS | SecRandomCopyBytes |
| Unix | /dev/urandom |

## 10. RSA Encryption

```tml
use std::crypto::rsa::{
    public_encrypt, private_decrypt, RsaPadding
}
use std::crypto::key::{generate_key_pair, KeyType}

let pair = generate_key_pair(KeyType::Rsa, 2048).unwrap()

// Encrypt with public key
let ciphertext = public_encrypt(
    ref pair.public_key,
    ref plaintext_buffer,
    RsaPadding::OaepSha256
).unwrap()

// Decrypt with private key
let plaintext = private_decrypt(
    ref pair.private_key,
    ref ciphertext,
    RsaPadding::OaepSha256
).unwrap()
```

### 10.1 RSA Padding Modes

| `RsaPadding` | Description | Notes |
|--------------|-------------|-------|
| `Pkcs1` | PKCS#1 v1.5 | Legacy, avoid for new code |
| `OaepSha256` | OAEP with SHA-256 | **Recommended** |
| `OaepSha384` | OAEP with SHA-384 | Higher security margin |
| `OaepSha512` | OAEP with SHA-512 | Maximum security |

## 11. Key Exchange

### 11.1 ECDH (Elliptic Curve Diffie-Hellman)

```tml
use std::crypto::ecdh::{create_ecdh, EcCurve}

let alice = create_ecdh(EcCurve::X25519).unwrap()
let bob = create_ecdh(EcCurve::X25519).unwrap()

// Exchange public keys and compute shared secret
// (The shared secret is the same for both parties)
```

### 11.2 Supported Curves

| `EcCurve` | Description | Key Size |
|-----------|-------------|----------|
| `P256` | NIST P-256 | 32 bytes |
| `P384` | NIST P-384 | 48 bytes |
| `P521` | NIST P-521 | 66 bytes |
| `X25519` | Curve25519 | 32 bytes |
| `X448` | Curve448 | 56 bytes |

## 12. Error Handling

### 12.1 CryptoError

```tml
use std::crypto::error::CryptoError

let result = generate_key_pair(KeyType::Rsa, 512)
when result {
    Ok(pair) => println("Key generated")
    Err(e) => {
        println("Error: " + e.get_message())
        println("Details: " + e.get_details())
    }
}
```

### 12.2 Error Factory Methods

```tml
// Create specific error types
CryptoError::new("general error message")
CryptoError::with_details("message", "technical details")
CryptoError::invalid_parameter("key must be 32 bytes")
CryptoError::invalid_key("key format not recognized")
CryptoError::invalid_iv("IV must be 12 bytes for GCM")
CryptoError::auth_failed()
CryptoError::verification_failed()
CryptoError::unsupported_algorithm("SHA-3")
CryptoError::operation_failed("decryption failed")
```

### 12.3 Error Kinds

| Kind | Description |
|------|-------------|
| `InvalidKey` | Key is wrong size or format |
| `InvalidIv` | IV/nonce is wrong size |
| `InvalidAuthTag` | Authentication tag is invalid |
| `AuthenticationFailed` | AEAD authentication failed |
| `InvalidSignature` | Signature format is invalid |
| `VerificationFailed` | Signature verification failed |
| `UnsupportedAlgorithm` | Algorithm not available |
| `InvalidParameter` | Generic invalid parameter |
| `InvalidPadding` | Padding is invalid |
| `KeyDerivationFailed` | KDF operation failed |
| `RandomGenerationFailed` | CSPRNG failure |
| `CertificateParseError` | X.509 parse error |
| `KeyExchangeFailed` | DH/ECDH failure |
| `OperationFailed` | Generic operation failure |

## 13. Complete Example

```tml
// Hash Table Generator — CRC32 + MD5 + SHA-256 + SHA-512
use std::crypto::hash::{sha256, sha512, md5, Digest}
use std::zlib::crc32::{crc32}

func hash_message(label: Str, msg: Str) {
    println("-------------------------------------------")
    print("Input:   \"")
    print(msg)
    println("\"")
    print("Label:   ")
    println(label)

    // CRC32 (from std::zlib)
    let checksum: I64 = crc32(msg)
    print("CRC32:   ")
    println(checksum.to_string())

    // MD5
    var d_md5: Digest = md5(msg)
    print("MD5:     ")
    println(d_md5.to_hex())
    d_md5.destroy()

    // SHA-256
    var d_sha256: Digest = sha256(msg)
    print("SHA-256: ")
    println(d_sha256.to_hex())
    d_sha256.destroy()

    // SHA-512
    var d_sha512: Digest = sha512(msg)
    print("SHA-512: ")
    println(d_sha512.to_hex())
    d_sha512.destroy()
}

func main() -> I32 {
    println("===========================================")
    println("  TML Hash Table Generator")
    println("===========================================")

    hash_message("empty string", "")
    hash_message("pangram", "The quick brown fox jumps over the lazy dog")
    hash_message("numeric", "1234567890")

    return 0
}
```

## 14. Security Recommendations

| Task | Recommended Algorithm |
|------|----------------------|
| Hashing | SHA-256 or SHA-512 |
| HMAC | HMAC-SHA256 |
| Symmetric encryption | AES-256-GCM or ChaCha20-Poly1305 |
| Asymmetric encryption | RSA-OAEP-SHA256 (2048+ bits) |
| Signatures | Ed25519 or RSA-PSS-SHA256 |
| Password hashing | Argon2id, then scrypt, then PBKDF2 |
| Key exchange | X25519 or ECDH P-256 |
| Random generation | `random_bytes()` (uses OS CSPRNG) |

**Avoid for security:** MD5, SHA-1, DES, ECB mode, RSA PKCS#1 v1.5 padding.

---

*Previous: [04-ENCODING.md](./04-ENCODING.md)*
*Next: [06-TLS.md](./06-TLS.md) — TLS/SSL Connections*
