# 16 — Blocked Modules: Crypto, JSON, String Ops

Three major stdlib categories are completely blocked by codegen bugs, preventing any performance measurement.

## Blocked Benchmarks

### 1. Crypto (std::crypto) — N002 Link Failure

**File**: `crypto_bench.tml`
**Tests**: SHA-256, SHA-512, MD5 hashing; HMAC; hex conversion
**Error**:
```
error: failed to open object crypto_DTMLHASO.obj: FileNotFound
lld-link: error: crypto.c: unknown file type
```

**Root cause**: C runtime files (`compiler/runtime/crypto/*.c`) are passed as source to the linker instead of pre-compiled `.obj` files. The build pipeline's dependency resolution for C runtime modules is broken.

**Affected modules**:
- `std::crypto::hash` — SHA-1, SHA-2, SHA-3, MD5, BLAKE2/3
- `std::crypto::hmac` — HMAC computation
- `std::crypto::cipher` — AES, ChaCha20
- `std::crypto::sign` — RSA, ECDSA, Ed25519
- `std::crypto::kdf` — PBKDF2, Argon2, scrypt
- `std::crypto::rsa` — RSA encryption
- `std::crypto::x509` — Certificate handling
- `std::net::tls` — TLS/SSL (also blocked)

### 2. JSON (std::json) — K001 Boolean Codegen

**File**: `json_bench.tml`
**Tests**: Small/medium/large JSON parsing, generation, SIMD parsing
**Error**:
```
K001: '%t4892' defined with type 'i32' but expected 'i1'
%t4893 = icmp eq i1 %t4892, 1
```

**Root cause**: A boolean value (`i1`) is being generated as `i32` somewhere in the JSON parsing pipeline. The `icmp eq i1` instruction receives an `i32` operand. This is a type mismatch in the MIR→LLVM emission for boolean comparisons within the JSON parser's code path.

**Affected modules**:
- `std::json` — SIMD-optimized parser
- `std::json::builder` — Fluent JSON builder
- `std::json::serialize` — ToJson/FromJson behaviors
- Any module that uses `std::json` transitively

### 3. String Operations (core::str) — K001 Undefined Symbol

**File**: `string_bench.tml`, `text_bench.tml`
**Tests**: String length, contains, find, split, trim, SIMD search
**Error**:
```
K001: use of undefined value '@tml_N4core3str3lenE_S'
```

**Root cause**: The `core::str::len()` function's symbol is not being emitted in the generated LLVM IR. The function exists in the source, type-checks, and has a valid mangled name, but the codegen pipeline doesn't generate its body. Likely a missing entry in the symbol table or an incorrect dead-code elimination.

**Affected modules**:
- `core::str` — All string operations (len, contains, find, split, trim, etc.)
- `core::str::simd` — SIMD-optimized string search
- `core::fmt` — Display/Debug formatting (uses str::len internally)
- `std::text::Text` — StringBuilder (uses str::len for push_str)
- `core::encoding::*` — Partially (some work, some don't)

## Impact Assessment

| Category | Modules Blocked | % of stdlib | Priority |
|----------|----------------|-------------|----------|
| String ops | ~15 modules | ~8% | Critical |
| JSON | 4 modules | ~2% | High |
| Crypto | 10 modules | ~5% | High |
| TLS/Net | 3 modules | ~2% | Medium |
| **Total** | **~32 modules** | **~17%** | |

**17% of the standard library is unbenmarkable** due to 2 codegen bugs (K001) and 1 build issue (N002).

## Fix Priority

| Bug | Fix Location | Complexity | Unblocks |
|-----|-------------|------------|----------|
| K001 str::len | `thir_mir_builder.cpp` or `instructions.cpp` | Medium | String, Text, fmt, encoding |
| K001 bool i32/i1 | `emit_binary_op` or boolean lowering | Low | JSON, any bool-heavy code |
| N002 crypto .obj | `build.bat` / CMake C runtime compilation | Low | Crypto, TLS, networking |
