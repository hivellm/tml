# std::crypto — Cryptographic Primitives

## 1. Overview

The \x60std::crypto` package provides cryptographic primitives: hashing, encryption, signatures, and random number generation.

```tml
use std::crypto
use std::crypto.{sha256, aes, rsa}
```

## 2. Capabilities

```tml
caps: [io.random]  // Only for secure random generation
// Most crypto operations require no capabilities
```

## 3. Hash Functions

### 3.1 Common Interface

```tml
pub behaviorHasher {
    /// Update with data
    func update(this, data: ref [U8])

    /// Finalize and return digest
    func finalize(this) -> List[U8]

    /// Reset to initial state
    func reset(this)

    /// Get output size in bytes
    func output_size(this) -> U64
}

/// One-shot hash function
pub func hash[H: Hasher + Default](data: ref [U8]) -> List[U8] {
    var hasher = H.default()
    hasher.update(data)
    return hasher.finalize()
}
```

### 3.2 SHA-2 Family

```tml
mod sha2

pub type Sha256 {
    state: [U32; 8],
    buffer: [U8; 64],
    buffer_len: U64,
    total_len: U64,
}

extend Sha256 {
    pub const OUTPUT_SIZE: U64 = 32
    pub const BLOCK_SIZE: U64 = 64

    pub func new() -> This
    pub func update(this, data: ref [U8])
    pub func finalize(this) -> [U8; 32]
    pub func finalize_reset(this) -> [U8; 32]
    pub func reset(this)
}

extend Sha256 with Hasher { ... }
extend Sha256 with Default { ... }

pub type Sha384 { ... }
pub type Sha512 { ... }

/// Convenience functions
pub func sha256(data: ref [U8]) -> [U8; 32] {
    var hasher = Sha256.new()
    hasher.update(data)
    return hasher.finalize()
}

pub func sha384(data: ref [U8]) -> [U8; 48]
pub func sha512(data: ref [U8]) -> [U8; 64]
```

### 3.3 SHA-3 Family

```tml
mod sha3

pub type Sha3_256 { ... }
pub type Sha3_384 { ... }
pub type Sha3_512 { ... }
pub type Keccak256 { ... }

pub func sha3_256(data: ref [U8]) -> [U8; 32]
pub func sha3_384(data: ref [U8]) -> [U8; 48]
pub func sha3_512(data: ref [U8]) -> [U8; 64]
pub func keccak256(data: ref [U8]) -> [U8; 32]
```

### 3.4 BLAKE Family

```tml
mod blake

pub type Blake2b {
    output_size: U64,
    key: Maybe[List[U8]],
    ...
}

extend Blake2b {
    pub func new(output_size: U64) -> This
    pub func with_key(output_size: U64, key: ref [U8]) -> Outcome[This, Error]
}

pub type Blake2s { ... }
pub type Blake3 { ... }

pub func blake2b(data: ref [U8], output_size: U64) -> List[U8]
pub func blake2s(data: ref [U8], output_size: U64) -> List[U8]
pub func blake3(data: ref [U8]) -> [U8; 32]
```

### 3.5 Legacy Hashes (Not for Security)

```tml
mod md5

pub type Md5 { ... }
pub func md5(data: ref [U8]) -> [U8; 16]

mod sha1

pub type Sha1 { ... }
pub func sha1(data: ref [U8]) -> [U8; 20]
```

## 4. Message Authentication Codes

### 4.1 HMAC

```tml
mod hmac

pub type Hmac[H: Hasher] {
    inner: H,
    outer: H,
    key_block: List[U8],
}

extend Hmac[H: Hasher] {
    pub func new(key: ref [U8]) -> This
    pub func update(this, data: ref [U8])
    pub func finalize(this) -> List[U8]
    pub func verify(this, tag: ref [U8]) -> Bool
    pub func reset(this)
}

/// Convenience types
pub type HmacSha256 = Hmac[Sha256]
pub type HmacSha384 = Hmac[Sha384]
pub type HmacSha512 = Hmac[Sha512]

/// One-shot HMAC
pub func hmac_sha256(key: ref [U8], data: ref [U8]) -> [U8; 32] {
    var h = HmacSha256.new(key)
    h.update(data)
    return h.finalize()
}
```

### 4.2 Poly1305

```tml
mod poly1305

pub type Poly1305 {
    r: [U32; 5],
    h: [U32; 5],
    pad: [U32; 4],
    ...
}

extend Poly1305 {
    pub func new(key: ref [U8; 32]) -> This
    pub func update(this, data: ref [U8])
    pub func finalize(this) -> [U8; 16]
    pub func verify(this, tag: ref [U8; 16]) -> Bool
}
```

## 5. Symmetric Encryption

### 5.1 AES

```tml
mod aes

pub type AesKey = Aes128 | Aes192 | Aes256

pub type Aes128 { round_keys: [[U8; 16]; 11] }
pub type Aes192 { round_keys: [[U8; 16]; 13] }
pub type Aes256 { round_keys: [[U8; 16]; 15] }

extend Aes128 {
    pub func new(key: ref [U8; 16]) -> This
    pub func encrypt_block(this, block: mut ref [U8; 16])
    pub func decrypt_block(this, block: mut ref [U8; 16])
}

// Similar for Aes192, Aes256
```

### 5.2 AES-GCM (AEAD)

```tml
mod aes_gcm

pub type AesGcm {
    key: AesKey,
}

pub type Nonce = [U8; 12]
pub type Tag = [U8; 16]

extend AesGcm {
    pub func new(key: ref [U8]) -> Outcome[This, Error]

    /// Encrypt with associated data
    pub func encrypt(
        this,
        nonce: ref Nonce,
        plaintext: ref [U8],
        associated_data: ref [U8]
    ) -> (List[U8], Tag)

    /// Decrypt and verify
    pub func decrypt(
        this,
        nonce: ref Nonce,
        ciphertext: ref [U8],
        associated_data: ref [U8],
        tag: ref Tag
    ) -> Outcome[List[U8], AuthError]

    /// Encrypt in place
    pub func encrypt_in_place(
        this,
        nonce: ref Nonce,
        associated_data: ref [U8],
        buffer: mut ref List[U8]
    ) -> Tag

    /// Decrypt in place
    pub func decrypt_in_place(
        this,
        nonce: ref Nonce,
        associated_data: ref [U8],
        buffer: mut ref List[U8],
        tag: ref Tag
    ) -> Outcome[Unit, AuthError]
}

pub type AuthError { message: String }
```

### 5.3 ChaCha20-Poly1305

```tml
mod chacha20poly1305

pub type ChaCha20Poly1305 {
    key: [U8; 32],
}

pub type Nonce = [U8; 12]
pub type Tag = [U8; 16]

extend ChaCha20Poly1305 {
    pub func new(key: ref [U8; 32]) -> This

    pub func encrypt(
        this,
        nonce: ref Nonce,
        plaintext: ref [U8],
        associated_data: ref [U8]
    ) -> (List[U8], Tag)

    pub func decrypt(
        this,
        nonce: ref Nonce,
        ciphertext: ref [U8],
        associated_data: ref [U8],
        tag: ref Tag
    ) -> Outcome[List[U8], AuthError]
}

// XChaCha20-Poly1305 with extended nonce
pub type XChaCha20Poly1305 { ... }
pub type XNonce = [U8; 24]
```

## 6. Asymmetric Encryption

### 6.1 RSA

```tml
mod rsa

pub type RsaPublicKey {
    n: BigUint,  // Modulus
    e: BigUint,  // Public exponent
}

pub type RsaPrivateKey {
    public_key: RsaPublicKey,
    d: BigUint,  // Private exponent
    p: BigUint,  // Prime 1
    q: BigUint,  // Prime 2
}

extend RsaPrivateKey {
    /// Generate new key pair
    pub func generate(bits: U32) -> Outcome[This, Error]
    effects: [io.random]

    /// Get public key
    pub func public_key(this) -> ref RsaPublicKey

    /// Sign message (PKCS#1 v1.5)
    pub func sign_pkcs1v15(this, hash: ref [U8], hash_algo: HashAlgo) -> Outcome[List[U8], Error]

    /// Sign message (PSS)
    pub func sign_pss(this, hash: ref [U8], hash_algo: HashAlgo) -> Outcome[List[U8], Error]
    effects: [io.random]

    /// Decrypt (OAEP)
    pub func decrypt_oaep(this, ciphertext: ref [U8], hash_algo: HashAlgo) -> Outcome[List[U8], Error]

    /// Decrypt (PKCS#1 v1.5)
    pub func decrypt_pkcs1v15(this, ciphertext: ref [U8]) -> Outcome[List[U8], Error]
}

extend RsaPublicKey {
    /// Verify signature (PKCS#1 v1.5)
    pub func verify_pkcs1v15(this, hash: ref [U8], signature: ref [U8], hash_algo: HashAlgo) -> Bool

    /// Verify signature (PSS)
    pub func verify_pss(this, hash: ref [U8], signature: ref [U8], hash_algo: HashAlgo) -> Bool

    /// Encrypt (OAEP)
    pub func encrypt_oaep(this, plaintext: ref [U8], hash_algo: HashAlgo) -> Outcome[List[U8], Error]
    effects: [io.random]

    /// Encrypt (PKCS#1 v1.5)
    pub func encrypt_pkcs1v15(this, plaintext: ref [U8]) -> Outcome[List[U8], Error]
    effects: [io.random]
}

pub type HashAlgo = Sha256 | Sha384 | Sha512
```

### 6.2 ECDSA

```tml
mod ecdsa

pub type Curve = P256 | P384 | P521 | Secp256k1

pub type EcdsaPublicKey {
    curve: Curve,
    point: (BigUint, BigUint),
}

pub type EcdsaPrivateKey {
    curve: Curve,
    scalar: BigUint,
    public_key: EcdsaPublicKey,
}

extend EcdsaPrivateKey {
    pub func generate(curve: Curve) -> Outcome[This, Error]
    effects: [io.random]

    pub func from_bytes(curve: Curve, bytes: ref [U8]) -> Outcome[This, Error]
    pub func to_bytes(this) -> List[U8]
    pub func public_key(this) -> ref EcdsaPublicKey
    pub func sign(this, message_hash: ref [U8]) -> Outcome[Signature, Error]
    effects: [io.random]
}

extend EcdsaPublicKey {
    pub func from_bytes(curve: Curve, bytes: ref [U8]) -> Outcome[This, Error]
    pub func to_bytes(this, compressed: Bool) -> List[U8]
    pub func verify(this, message_hash: ref [U8], signature: ref Signature) -> Bool
}

pub type Signature {
    r: BigUint,
    s: BigUint,
}

extend Signature {
    pub func from_der(bytes: ref [U8]) -> Outcome[This, Error]
    pub func to_der(this) -> List[U8]
    pub func from_bytes(bytes: ref [U8]) -> Outcome[This, Error]
    pub func to_bytes(this) -> List[U8]
}
```

### 6.3 Ed25519

```tml
mod ed25519

pub type PublicKey = [U8; 32]
pub type PrivateKey = [U8; 32]
pub type Signature = [U8; 64]

/// Generate key pair
pub func generate_keypair() -> (PrivateKey, PublicKey)
effects: [io.random]

/// Derive public key from private key
pub func public_key(private_key: ref PrivateKey) -> PublicKey

/// Sign message
pub func sign(private_key: ref PrivateKey, message: ref [U8]) -> Signature

/// Verify signature
pub func verify(public_key: ref PublicKey, message: ref [U8], signature: ref Signature) -> Bool
```

## 7. Key Exchange

### 7.1 X25519

```tml
mod x25519

pub type PublicKey = [U8; 32]
pub type PrivateKey = [U8; 32]
pub type SharedSecret = [U8; 32]

/// Generate key pair
pub func generate_keypair() -> (PrivateKey, PublicKey)
effects: [io.random]

/// Derive public key from private key
pub func public_key(private_key: ref PrivateKey) -> PublicKey

/// Compute shared secret
pub func diffie_hellman(private_key: ref PrivateKey, peer_public: ref PublicKey) -> SharedSecret
```

### 7.2 ECDH

```tml
mod ecdh

pub func diffie_hellman(
    private_key: ref EcdsaPrivateKey,
    peer_public: ref EcdsaPublicKey
) -> Outcome[List[U8], Error]
```

## 8. Key Derivation

### 8.1 HKDF

```tml
mod hkdf

/// Extract pseudorandom key from input key material
pub func extract[H: Hasher](salt: ref [U8], ikm: ref [U8]) -> List[U8]

/// Expand pseudorandom key to desired length
pub func expand[H: Hasher](prk: ref [U8], info: ref [U8], length: U64) -> Outcome[List[U8], Error]

/// Combined extract and expand
pub func derive[H: Hasher](
    salt: ref [U8],
    ikm: ref [U8],
    info: ref [U8],
    length: U64
) -> Outcome[List[U8], Error]

/// HKDF-SHA256
pub func hkdf_sha256(salt: ref [U8], ikm: ref [U8], info: ref [U8], length: U64) -> Outcome[List[U8], Error]
```

### 8.2 PBKDF2

```tml
mod pbkdf2

/// Derive key from password
pub func derive[H: Hasher](
    password: ref [U8],
    salt: ref [U8],
    iterations: U32,
    output_len: U64
) -> List[U8]

/// PBKDF2-HMAC-SHA256
pub func pbkdf2_sha256(
    password: ref [U8],
    salt: ref [U8],
    iterations: U32,
    output_len: U64
) -> List[U8]
```

### 8.3 Argon2 (Password Hashing)

```tml
mod argon2

pub type Variant = Argon2d | Argon2i | Argon2id

pub type Params {
    variant: Variant,
    memory_kib: U32,     // Memory cost in KiB
    iterations: U32,     // Time cost
    parallelism: U32,    // Parallel lanes
    output_len: U32,     // Output length
}

pub const DEFAULT_PARAMS: Params = Params {
    variant: Argon2id,
    memory_kib: 65536,   // 64 MiB
    iterations: 3,
    parallelism: 4,
    output_len: 32,
}

/// Hash password
pub func hash_password(password: ref [U8], salt: ref [U8], params: Params) -> List[U8]

/// Hash password to PHC string format
pub func hash_password_string(password: ref [U8]) -> String
effects: [io.random]

/// Verify password against PHC string
pub func verify_password(password: ref [U8], hash_string: ref str) -> Bool
```

## 9. Random Number Generation

```tml
mod random

/// Cryptographically secure random bytes
pub func bytes(buf: mut ref [U8])
effects: [io.random]

/// Generate random bytes as new buffer
pub func random_bytes(len: U64) -> List[U8]
effects: [io.random]

/// Generate random U32
pub func u32() -> U32
effects: [io.random]

/// Generate random U64
pub func u64() -> U64
effects: [io.random]

/// Generate random in range [0, max)
pub func range(max: U64) -> U64
effects: [io.random]

/// Generate random in range [min, max)
pub func range_between(min: U64, max: U64) -> U64
effects: [io.random]
```

## 10. Constant-Time Operations

```tml
mod constant_time

/// Constant-time comparison
pub func compare(a: ref [U8], b: ref [U8]) -> Bool

/// Constant-time select
pub func select(condition: Bool, a: U8, b: U8) -> U8

/// Constant-time conditional copy
pub func conditional_copy(condition: Bool, dst: mut ref [U8], src: ref [U8])
```

## 11. Examples

### 11.1 Password Hashing

```tml
mod auth
use std::crypto.argon2

func hash_password(password: ref str) -> String {
    return argon2.hash_password_string(password.as_bytes())
}

func verify_password(password: ref str, hash: ref str) -> Bool {
    return argon2.verify_password(password.as_bytes(), hash)
}
```

### 11.2 AEAD Encryption

```tml
mod secure_storage
caps: [io.random]

use std::crypto.{aes_gcm, random}
use std::crypto.aes_gcm.{AesGcm, Nonce, Tag}

func encrypt_data(key: ref [U8; 32], plaintext: ref [U8]) -> Outcome[(List[U8], Nonce, Tag), Error] {
    let cipher = AesGcm.new(key)!

    // Generate random nonce
    var nonce: Nonce = [0; 12]
    random.bytes(mut ref nonce)

    let (ciphertext, tag) = cipher.encrypt(ref nonce, plaintext, b"")
    return Ok((ciphertext, nonce, tag))
}

func decrypt_data(
    key: ref [U8; 32],
    ciphertext: ref [U8],
    nonce: ref Nonce,
    tag: ref Tag
) -> Outcome[List[U8], Error] {
    let cipher = AesGcm.new(key)!
    return cipher.decrypt(nonce, ciphertext, b"", tag).map_err(Error.from)
}
```

### 11.3 Digital Signatures

```tml
mod signing
caps: [io.random]

use std::crypto.ed25519

type KeyPair {
    private_key: ed25519.PrivateKey,
    public_key: ed25519.PublicKey,
}

extend KeyPair {
    func generate() -> This {
        let (sk, pk) = ed25519.generate_keypair()
        return This { private_key: sk, public_key: pk }
    }

    func sign(this, message: ref [U8]) -> ed25519.Signature {
        return ed25519.sign(&this.private_key, message)
    }

    func verify(this, message: ref [U8], signature: &ed25519.Signature) -> Bool {
        return ed25519.verify(&this.public_key, message, signature)
    }
}
```

---

*Previous: [04-ENCODING.md](./04-ENCODING.md)*
*Next: [06-TLS.md](./06-TLS.md) — TLS/SSL Connections*
