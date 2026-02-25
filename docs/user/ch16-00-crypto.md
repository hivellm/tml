# Cryptography

TML provides a comprehensive cryptography module (`std::crypto`) backed by platform-native implementations. This chapter covers the most common use cases: hashing, HMAC, encryption, key derivation, and secure random generation.

## Hashing

Hash functions turn arbitrary data into fixed-size digests. TML supports MD5, SHA-1, and the SHA-2 family.

### One-Shot Hashing

The simplest way to hash data is with one-shot functions:

```tml
use std::crypto::hash::{sha256, sha512, md5, sha1, Digest}

func main() -> I32 {
    var d: Digest = sha256("Hello, TML!")
    println(d.to_hex())     // hex-encoded hash
    println(d.to_base64())  // base64-encoded hash
    d.destroy()

    return 0
}
```

Each hash function returns a `Digest` object. Call `.to_hex()` or `.to_base64()` to get the string representation, and `.destroy()` to free the native resources.

### Available Hash Functions

| Function | Digest Size | Use Case |
|----------|-------------|----------|
| `md5(msg)` | 128-bit | Legacy checksums (not secure) |
| `sha1(msg)` | 160-bit | Legacy compatibility (not secure) |
| `sha256(msg)` | 256-bit | General purpose, recommended |
| `sha384(msg)` | 384-bit | Higher security |
| `sha512(msg)` | 512-bit | Maximum security |
| `sha512_256(msg)` | 256-bit | SHA-512 with 256-bit output |

### Streaming Hash

For large data or data arriving in chunks, use the streaming interface:

```tml
use std::crypto::hash::{HashStream, HashAlgorithm}

var stream: HashStream = HashStream.new(HashAlgorithm.SHA256)
stream.update("first chunk")
stream.update("second chunk")
var digest: Digest = stream.finalize()
println(digest.to_hex())
digest.destroy()
```

### Complete Example

This example hashes a message with multiple algorithms:

```tml
use std::crypto::hash::{sha256, sha512, md5, Digest}
use std::zlib::crc32::{crc32}

func hash_message(msg: Str) {
    println("Input: \"{msg}\"")

    let checksum: I64 = crc32(msg)
    println("CRC32:   {checksum.to_string()}")

    var d: Digest = md5(msg)
    println("MD5:     {d.to_hex()}")
    d.destroy()

    var d2: Digest = sha256(msg)
    println("SHA-256: {d2.to_hex()}")
    d2.destroy()

    var d3: Digest = sha512(msg)
    println("SHA-512: {d3.to_hex()}")
    d3.destroy()
}

func main() -> I32 {
    hash_message("Hello, TML!")
    hash_message("The quick brown fox jumps over the lazy dog")
    return 0
}
```

## HMAC

HMAC (Hash-based Message Authentication Code) verifies both data integrity and authenticity using a secret key:

```tml
use std::crypto::hmac::{hmac_sha256, hmac_sha512, HmacDigest}

var mac: HmacDigest = hmac_sha256("secret-key", "message to authenticate")
println(mac.to_hex())
mac.destroy()
```

### Verifying an HMAC

```tml
use std::crypto::hmac::{hmac_sha256, verify_hmac}

let expected_hex: Str = "a1b2c3..."
let valid: Bool = verify_hmac("secret-key", "message", expected_hex, "sha256")
if valid {
    println("Message is authentic")
}
```

### Available HMAC Functions

| Function | Description |
|----------|-------------|
| `hmac_sha256(key, msg)` | HMAC with SHA-256 |
| `hmac_sha384(key, msg)` | HMAC with SHA-384 |
| `hmac_sha512(key, msg)` | HMAC with SHA-512 |
| `verify_hmac(key, msg, hex, algo)` | Constant-time verification |

## Symmetric Encryption

TML supports AES and ChaCha20 for symmetric encryption. AES-GCM is the recommended default.

### AES-GCM (Recommended)

```tml
use std::crypto::cipher::{Cipher, Decipher, CipherAlgorithm}
use std::crypto::random::{random_bytes}

// Generate a 256-bit key and 96-bit nonce
let key: Buffer = random_bytes(32)
let nonce: Buffer = random_bytes(12)

// Encrypt
var enc: Cipher = Cipher.new(CipherAlgorithm.AES_256_GCM, ref key, ref nonce)
let ciphertext: Buffer = enc.encrypt("secret message")
let tag: Buffer = enc.get_auth_tag()
enc.destroy()

// Decrypt
var dec: Decipher = Decipher.new(CipherAlgorithm.AES_256_GCM, ref key, ref nonce)
dec.set_auth_tag(ref tag)
let plaintext: Str = dec.decrypt(ref ciphertext)
dec.destroy()

println(plaintext)  // "secret message"
```

### Cipher Algorithms

| Algorithm | Key Size | Nonce | Notes |
|-----------|----------|-------|-------|
| `AES_128_GCM` | 16 bytes | 12 bytes | Authenticated, fast on AES-NI hardware |
| `AES_256_GCM` | 32 bytes | 12 bytes | Recommended default |
| `AES_128_CBC` | 16 bytes | 16 bytes | Legacy, requires separate HMAC |
| `AES_256_CBC` | 32 bytes | 16 bytes | Legacy, requires separate HMAC |
| `AES_128_CTR` | 16 bytes | 16 bytes | Stream cipher mode |
| `AES_256_CTR` | 32 bytes | 16 bytes | Stream cipher mode |
| `CHACHA20_POLY1305` | 32 bytes | 12 bytes | Authenticated, fast without hardware AES |

### Streaming Encryption

For large data, encrypt in chunks:

```tml
var enc: Cipher = Cipher.new(CipherAlgorithm.AES_256_GCM, ref key, ref nonce)
enc.update("chunk 1")
enc.update("chunk 2")
let ciphertext: Buffer = enc.finalize()
let tag: Buffer = enc.get_auth_tag()
enc.destroy()
```

## Key Derivation (KDF)

Key derivation functions convert passwords or weak keys into strong cryptographic keys.

### PBKDF2

The standard choice for password hashing:

```tml
use std::crypto::kdf::{pbkdf2}

let derived: Buffer = pbkdf2(
    "user-password",     // password
    "random-salt",       // salt (use random_bytes in production)
    100000,              // iterations (minimum 100,000 recommended)
    32,                  // output length in bytes
    "sha256"             // hash algorithm
)
```

### HKDF

Extract-and-expand key derivation, ideal for deriving multiple keys from a shared secret:

```tml
use std::crypto::kdf::{hkdf}

let derived: Buffer = hkdf(
    "shared-secret",     // input key material
    "salt-value",        // salt
    "context-info",      // context/info string
    32,                  // output length
    "sha256"             // hash algorithm
)
```

### Other KDFs

| Function | Use Case |
|----------|----------|
| `pbkdf2(pass, salt, iters, len, algo)` | Password hashing (recommended) |
| `hkdf(ikm, salt, info, len, algo)` | Key derivation from shared secrets |
| `scrypt(pass, salt, n, r, p, len)` | Memory-hard password hashing |
| `argon2(pass, salt, time, mem, par, len)` | Modern password hashing (Argon2id) |

## Secure Random Generation

The `std::crypto::random` module provides cryptographically secure random number generation (CSPRNG):

```tml
use std::crypto::random::{random_bytes, random_int, random_float, random_uuid}

// Random bytes (for keys, nonces, salts)
let key: Buffer = random_bytes(32)       // 32 random bytes
let nonce: Buffer = random_bytes(12)     // 12 random bytes

// Random integers
let n: I64 = random_int(1, 100)          // random integer in [1, 100]

// Random float
let f: F64 = random_float()              // random float in [0.0, 1.0)

// Random UUID (v4)
let id: Str = random_uuid()              // e.g. "550e8400-e29b-41d4-a716-446655440000"
```

## Digital Signatures

Digital signatures prove that a message was created by a known sender and was not altered.

### Ed25519 (Recommended)

```tml
use std::crypto::key::{generate_key_pair, KeyPair}
use std::crypto::sign::{sign, verify}

// Generate key pair
var kp: KeyPair = generate_key_pair("ed25519")

// Sign
let signature: Buffer = sign(ref kp.private_key, "message to sign", "ed25519")

// Verify
let valid: Bool = verify(ref kp.public_key, "message to sign", ref signature, "ed25519")
println("Valid: {valid.to_string()}")

kp.destroy()
```

### Signature Algorithms

| Algorithm | Key Size | Signature Size | Notes |
|-----------|----------|---------------|-------|
| `ed25519` | 32 bytes | 64 bytes | Fast, recommended |
| `ecdsa_p256` | 32 bytes | 64 bytes | NIST P-256 curve |
| `ecdsa_p384` | 48 bytes | 96 bytes | NIST P-384 curve |
| `rsa_pkcs1` | 2048+ bits | key-size dependent | Legacy RSA |
| `rsa_pss` | 2048+ bits | key-size dependent | Modern RSA padding |

## RSA Encryption

For asymmetric encryption (encrypting with a public key):

```tml
use std::crypto::rsa::{rsa_encrypt, rsa_decrypt}
use std::crypto::key::{generate_key_pair, KeyPair}

var kp: KeyPair = generate_key_pair("rsa2048")

let ciphertext: Buffer = rsa_encrypt(ref kp.public_key, "secret data", "oaep-sha256")
let plaintext: Str = rsa_decrypt(ref kp.private_key, ref ciphertext, "oaep-sha256")

kp.destroy()
```

## Error Handling

All cryptographic operations can fail. Handle errors with `Outcome`:

```tml
use std::crypto::cipher::{Cipher, CipherAlgorithm}
use std::crypto::error::{CryptoError}

func encrypt_data(key: ref Buffer, data: Str) -> Outcome[Buffer, CryptoError] {
    let nonce: Buffer = random_bytes(12)
    var enc: Cipher = Cipher.new(CipherAlgorithm.AES_256_GCM, key, ref nonce)
    let result: Buffer = enc.encrypt(data)
    enc.destroy()
    Ok(result)
}
```

Common error kinds:

| Error Kind | Cause |
|------------|-------|
| `InvalidKeyLength` | Key size doesn't match algorithm |
| `InvalidNonce` | Nonce/IV wrong size |
| `AuthenticationFailed` | GCM/Poly1305 tag verification failed |
| `DecryptionFailed` | Ciphertext corrupted or wrong key |
| `UnsupportedAlgorithm` | Algorithm not available on platform |

## Best Practices

1. **Use AES-256-GCM or ChaCha20-Poly1305** for encryption — they provide authentication built-in
2. **Use SHA-256 or SHA-512** for hashing — avoid MD5 and SHA-1 for security
3. **Use PBKDF2 or Argon2** for password hashing — never hash passwords with plain SHA-256
4. **Always use random nonces** — never reuse a nonce with the same key
5. **Call `.destroy()`** on Digest, Cipher, Decipher, and KeyPair objects to free native resources
6. **Use constant-time comparison** (`verify_hmac`) for authentication tags — avoid `==`

## See Also

- [Compression](ch17-00-compression.md) — CRC32 checksums and data compression
- [std::crypto Package Reference](../packages/05-CRYPTO.md) — Complete API reference
- [Standard Library](ch10-00-standard-library.md) — Overview of all standard modules

---

*Previous: [ch15-00-oop.md](ch15-00-oop.md)*
*Next: [ch17-00-compression.md](ch17-00-compression.md)*
