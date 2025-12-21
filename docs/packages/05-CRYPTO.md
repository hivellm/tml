# std.crypto — Cryptographic Primitives

## 1. Overview

The `std.crypto` package provides cryptographic primitives: hashing, encryption, signatures, and random number generation.

```tml
import std.crypto
import std.crypto.{sha256, aes, rsa}
```

## 2. Capabilities

```tml
caps: [io.random]  // Only for secure random generation
// Most crypto operations require no capabilities
```

## 3. Hash Functions

### 3.1 Common Interface

```tml
public trait Hasher {
    /// Update with data
    func update(this, data: &[U8])

    /// Finalize and return digest
    func finalize(this) -> List[U8]

    /// Reset to initial state
    func reset(this)

    /// Get output size in bytes
    func output_size(this) -> U64
}

/// One-shot hash function
public func hash[H: Hasher + Default](data: &[U8]) -> List[U8] {
    var hasher = H.default()
    hasher.update(data)
    return hasher.finalize()
}
```

### 3.2 SHA-2 Family

```tml
module sha2

public type Sha256 {
    state: [U32; 8],
    buffer: [U8; 64],
    buffer_len: U64,
    total_len: U64,
}

extend Sha256 {
    public const OUTPUT_SIZE: U64 = 32
    public const BLOCK_SIZE: U64 = 64

    public func new() -> This
    public func update(this, data: &[U8])
    public func finalize(this) -> [U8; 32]
    public func finalize_reset(this) -> [U8; 32]
    public func reset(this)
}

extend Sha256 with Hasher { ... }
extend Sha256 with Default { ... }

public type Sha384 { ... }
public type Sha512 { ... }

/// Convenience functions
public func sha256(data: &[U8]) -> [U8; 32] {
    var hasher = Sha256.new()
    hasher.update(data)
    return hasher.finalize()
}

public func sha384(data: &[U8]) -> [U8; 48]
public func sha512(data: &[U8]) -> [U8; 64]
```

### 3.3 SHA-3 Family

```tml
module sha3

public type Sha3_256 { ... }
public type Sha3_384 { ... }
public type Sha3_512 { ... }
public type Keccak256 { ... }

public func sha3_256(data: &[U8]) -> [U8; 32]
public func sha3_384(data: &[U8]) -> [U8; 48]
public func sha3_512(data: &[U8]) -> [U8; 64]
public func keccak256(data: &[U8]) -> [U8; 32]
```

### 3.4 BLAKE Family

```tml
module blake

public type Blake2b {
    output_size: U64,
    key: Option[List[U8]],
    ...
}

extend Blake2b {
    public func new(output_size: U64) -> This
    public func with_key(output_size: U64, key: &[U8]) -> Result[This, Error]
}

public type Blake2s { ... }
public type Blake3 { ... }

public func blake2b(data: &[U8], output_size: U64) -> List[U8]
public func blake2s(data: &[U8], output_size: U64) -> List[U8]
public func blake3(data: &[U8]) -> [U8; 32]
```

### 3.5 Legacy Hashes (Not for Security)

```tml
module md5

public type Md5 { ... }
public func md5(data: &[U8]) -> [U8; 16]

module sha1

public type Sha1 { ... }
public func sha1(data: &[U8]) -> [U8; 20]
```

## 4. Message Authentication Codes

### 4.1 HMAC

```tml
module hmac

public type Hmac[H: Hasher] {
    inner: H,
    outer: H,
    key_block: List[U8],
}

extend Hmac[H: Hasher] {
    public func new(key: &[U8]) -> This
    public func update(this, data: &[U8])
    public func finalize(this) -> List[U8]
    public func verify(this, tag: &[U8]) -> Bool
    public func reset(this)
}

/// Convenience types
public type HmacSha256 = Hmac[Sha256]
public type HmacSha384 = Hmac[Sha384]
public type HmacSha512 = Hmac[Sha512]

/// One-shot HMAC
public func hmac_sha256(key: &[U8], data: &[U8]) -> [U8; 32] {
    var h = HmacSha256.new(key)
    h.update(data)
    return h.finalize()
}
```

### 4.2 Poly1305

```tml
module poly1305

public type Poly1305 {
    r: [U32; 5],
    h: [U32; 5],
    pad: [U32; 4],
    ...
}

extend Poly1305 {
    public func new(key: &[U8; 32]) -> This
    public func update(this, data: &[U8])
    public func finalize(this) -> [U8; 16]
    public func verify(this, tag: &[U8; 16]) -> Bool
}
```

## 5. Symmetric Encryption

### 5.1 AES

```tml
module aes

public type AesKey = Aes128 | Aes192 | Aes256

public type Aes128 { round_keys: [[U8; 16]; 11] }
public type Aes192 { round_keys: [[U8; 16]; 13] }
public type Aes256 { round_keys: [[U8; 16]; 15] }

extend Aes128 {
    public func new(key: &[U8; 16]) -> This
    public func encrypt_block(this, block: &mut [U8; 16])
    public func decrypt_block(this, block: &mut [U8; 16])
}

// Similar for Aes192, Aes256
```

### 5.2 AES-GCM (AEAD)

```tml
module aes_gcm

public type AesGcm {
    key: AesKey,
}

public type Nonce = [U8; 12]
public type Tag = [U8; 16]

extend AesGcm {
    public func new(key: &[U8]) -> Result[This, Error]

    /// Encrypt with associated data
    public func encrypt(
        this,
        nonce: &Nonce,
        plaintext: &[U8],
        associated_data: &[U8]
    ) -> (List[U8], Tag)

    /// Decrypt and verify
    public func decrypt(
        this,
        nonce: &Nonce,
        ciphertext: &[U8],
        associated_data: &[U8],
        tag: &Tag
    ) -> Result[List[U8], AuthError]

    /// Encrypt in place
    public func encrypt_in_place(
        this,
        nonce: &Nonce,
        associated_data: &[U8],
        buffer: &mut List[U8]
    ) -> Tag

    /// Decrypt in place
    public func decrypt_in_place(
        this,
        nonce: &Nonce,
        associated_data: &[U8],
        buffer: &mut List[U8],
        tag: &Tag
    ) -> Result[Unit, AuthError]
}

public type AuthError { message: String }
```

### 5.3 ChaCha20-Poly1305

```tml
module chacha20poly1305

public type ChaCha20Poly1305 {
    key: [U8; 32],
}

public type Nonce = [U8; 12]
public type Tag = [U8; 16]

extend ChaCha20Poly1305 {
    public func new(key: &[U8; 32]) -> This

    public func encrypt(
        this,
        nonce: &Nonce,
        plaintext: &[U8],
        associated_data: &[U8]
    ) -> (List[U8], Tag)

    public func decrypt(
        this,
        nonce: &Nonce,
        ciphertext: &[U8],
        associated_data: &[U8],
        tag: &Tag
    ) -> Result[List[U8], AuthError]
}

// XChaCha20-Poly1305 with extended nonce
public type XChaCha20Poly1305 { ... }
public type XNonce = [U8; 24]
```

## 6. Asymmetric Encryption

### 6.1 RSA

```tml
module rsa

public type RsaPublicKey {
    n: BigUint,  // Modulus
    e: BigUint,  // Public exponent
}

public type RsaPrivateKey {
    public_key: RsaPublicKey,
    d: BigUint,  // Private exponent
    p: BigUint,  // Prime 1
    q: BigUint,  // Prime 2
}

extend RsaPrivateKey {
    /// Generate new key pair
    public func generate(bits: U32) -> Result[This, Error]
    effects: [io.random]

    /// Get public key
    public func public_key(this) -> &RsaPublicKey

    /// Sign message (PKCS#1 v1.5)
    public func sign_pkcs1v15(this, hash: &[U8], hash_algo: HashAlgo) -> Result[List[U8], Error]

    /// Sign message (PSS)
    public func sign_pss(this, hash: &[U8], hash_algo: HashAlgo) -> Result[List[U8], Error]
    effects: [io.random]

    /// Decrypt (OAEP)
    public func decrypt_oaep(this, ciphertext: &[U8], hash_algo: HashAlgo) -> Result[List[U8], Error]

    /// Decrypt (PKCS#1 v1.5)
    public func decrypt_pkcs1v15(this, ciphertext: &[U8]) -> Result[List[U8], Error]
}

extend RsaPublicKey {
    /// Verify signature (PKCS#1 v1.5)
    public func verify_pkcs1v15(this, hash: &[U8], signature: &[U8], hash_algo: HashAlgo) -> Bool

    /// Verify signature (PSS)
    public func verify_pss(this, hash: &[U8], signature: &[U8], hash_algo: HashAlgo) -> Bool

    /// Encrypt (OAEP)
    public func encrypt_oaep(this, plaintext: &[U8], hash_algo: HashAlgo) -> Result[List[U8], Error]
    effects: [io.random]

    /// Encrypt (PKCS#1 v1.5)
    public func encrypt_pkcs1v15(this, plaintext: &[U8]) -> Result[List[U8], Error]
    effects: [io.random]
}

public type HashAlgo = Sha256 | Sha384 | Sha512
```

### 6.2 ECDSA

```tml
module ecdsa

public type Curve = P256 | P384 | P521 | Secp256k1

public type EcdsaPublicKey {
    curve: Curve,
    point: (BigUint, BigUint),
}

public type EcdsaPrivateKey {
    curve: Curve,
    scalar: BigUint,
    public_key: EcdsaPublicKey,
}

extend EcdsaPrivateKey {
    public func generate(curve: Curve) -> Result[This, Error]
    effects: [io.random]

    public func from_bytes(curve: Curve, bytes: &[U8]) -> Result[This, Error]
    public func to_bytes(this) -> List[U8]
    public func public_key(this) -> &EcdsaPublicKey
    public func sign(this, message_hash: &[U8]) -> Result[Signature, Error]
    effects: [io.random]
}

extend EcdsaPublicKey {
    public func from_bytes(curve: Curve, bytes: &[U8]) -> Result[This, Error]
    public func to_bytes(this, compressed: Bool) -> List[U8]
    public func verify(this, message_hash: &[U8], signature: &Signature) -> Bool
}

public type Signature {
    r: BigUint,
    s: BigUint,
}

extend Signature {
    public func from_der(bytes: &[U8]) -> Result[This, Error]
    public func to_der(this) -> List[U8]
    public func from_bytes(bytes: &[U8]) -> Result[This, Error]
    public func to_bytes(this) -> List[U8]
}
```

### 6.3 Ed25519

```tml
module ed25519

public type PublicKey = [U8; 32]
public type PrivateKey = [U8; 32]
public type Signature = [U8; 64]

/// Generate key pair
public func generate_keypair() -> (PrivateKey, PublicKey)
effects: [io.random]

/// Derive public key from private key
public func public_key(private_key: &PrivateKey) -> PublicKey

/// Sign message
public func sign(private_key: &PrivateKey, message: &[U8]) -> Signature

/// Verify signature
public func verify(public_key: &PublicKey, message: &[U8], signature: &Signature) -> Bool
```

## 7. Key Exchange

### 7.1 X25519

```tml
module x25519

public type PublicKey = [U8; 32]
public type PrivateKey = [U8; 32]
public type SharedSecret = [U8; 32]

/// Generate key pair
public func generate_keypair() -> (PrivateKey, PublicKey)
effects: [io.random]

/// Derive public key from private key
public func public_key(private_key: &PrivateKey) -> PublicKey

/// Compute shared secret
public func diffie_hellman(private_key: &PrivateKey, peer_public: &PublicKey) -> SharedSecret
```

### 7.2 ECDH

```tml
module ecdh

public func diffie_hellman(
    private_key: &EcdsaPrivateKey,
    peer_public: &EcdsaPublicKey
) -> Result[List[U8], Error]
```

## 8. Key Derivation

### 8.1 HKDF

```tml
module hkdf

/// Extract pseudorandom key from input key material
public func extract[H: Hasher](salt: &[U8], ikm: &[U8]) -> List[U8]

/// Expand pseudorandom key to desired length
public func expand[H: Hasher](prk: &[U8], info: &[U8], length: U64) -> Result[List[U8], Error]

/// Combined extract and expand
public func derive[H: Hasher](
    salt: &[U8],
    ikm: &[U8],
    info: &[U8],
    length: U64
) -> Result[List[U8], Error]

/// HKDF-SHA256
public func hkdf_sha256(salt: &[U8], ikm: &[U8], info: &[U8], length: U64) -> Result[List[U8], Error]
```

### 8.2 PBKDF2

```tml
module pbkdf2

/// Derive key from password
public func derive[H: Hasher](
    password: &[U8],
    salt: &[U8],
    iterations: U32,
    output_len: U64
) -> List[U8]

/// PBKDF2-HMAC-SHA256
public func pbkdf2_sha256(
    password: &[U8],
    salt: &[U8],
    iterations: U32,
    output_len: U64
) -> List[U8]
```

### 8.3 Argon2 (Password Hashing)

```tml
module argon2

public type Variant = Argon2d | Argon2i | Argon2id

public type Params {
    variant: Variant,
    memory_kib: U32,     // Memory cost in KiB
    iterations: U32,     // Time cost
    parallelism: U32,    // Parallel lanes
    output_len: U32,     // Output length
}

public const DEFAULT_PARAMS: Params = Params {
    variant: Argon2id,
    memory_kib: 65536,   // 64 MiB
    iterations: 3,
    parallelism: 4,
    output_len: 32,
}

/// Hash password
public func hash_password(password: &[U8], salt: &[U8], params: Params) -> List[U8]

/// Hash password to PHC string format
public func hash_password_string(password: &[U8]) -> String
effects: [io.random]

/// Verify password against PHC string
public func verify_password(password: &[U8], hash_string: &str) -> Bool
```

## 9. Random Number Generation

```tml
module random

/// Cryptographically secure random bytes
public func bytes(buf: &mut [U8])
effects: [io.random]

/// Generate random bytes as new buffer
public func random_bytes(len: U64) -> List[U8]
effects: [io.random]

/// Generate random U32
public func u32() -> U32
effects: [io.random]

/// Generate random U64
public func u64() -> U64
effects: [io.random]

/// Generate random in range [0, max)
public func range(max: U64) -> U64
effects: [io.random]

/// Generate random in range [min, max)
public func range_between(min: U64, max: U64) -> U64
effects: [io.random]
```

## 10. Constant-Time Operations

```tml
module constant_time

/// Constant-time comparison
public func compare(a: &[U8], b: &[U8]) -> Bool

/// Constant-time select
public func select(condition: Bool, a: U8, b: U8) -> U8

/// Constant-time conditional copy
public func conditional_copy(condition: Bool, dst: &mut [U8], src: &[U8])
```

## 11. Examples

### 11.1 Password Hashing

```tml
module auth
import std.crypto.argon2

func hash_password(password: &str) -> String {
    return argon2.hash_password_string(password.as_bytes())
}

func verify_password(password: &str, hash: &str) -> Bool {
    return argon2.verify_password(password.as_bytes(), hash)
}
```

### 11.2 AEAD Encryption

```tml
module secure_storage
caps: [io.random]

import std.crypto.{aes_gcm, random}
import std.crypto.aes_gcm.{AesGcm, Nonce, Tag}

func encrypt_data(key: &[U8; 32], plaintext: &[U8]) -> Result[(List[U8], Nonce, Tag), Error] {
    let cipher = AesGcm.new(key)?

    // Generate random nonce
    var nonce: Nonce = [0; 12]
    random.bytes(&mut nonce)

    let (ciphertext, tag) = cipher.encrypt(&nonce, plaintext, b"")
    return Ok((ciphertext, nonce, tag))
}

func decrypt_data(
    key: &[U8; 32],
    ciphertext: &[U8],
    nonce: &Nonce,
    tag: &Tag
) -> Result[List[U8], Error] {
    let cipher = AesGcm.new(key)?
    return cipher.decrypt(nonce, ciphertext, b"", tag).map_err(Error.from)
}
```

### 11.3 Digital Signatures

```tml
module signing
caps: [io.random]

import std.crypto.ed25519

type KeyPair {
    private_key: ed25519.PrivateKey,
    public_key: ed25519.PublicKey,
}

extend KeyPair {
    func generate() -> This {
        let (sk, pk) = ed25519.generate_keypair()
        return This { private_key: sk, public_key: pk }
    }

    func sign(this, message: &[U8]) -> ed25519.Signature {
        return ed25519.sign(&this.private_key, message)
    }

    func verify(this, message: &[U8], signature: &ed25519.Signature) -> Bool {
        return ed25519.verify(&this.public_key, message, signature)
    }
}
```

---

*Previous: [04-ENCODING.md](./04-ENCODING.md)*
*Next: [06-TLS.md](./06-TLS.md) — TLS/SSL Connections*
