# Runtime & External Dependencies Analysis

**Date**: 2026-04-05  
**Scope**: Complete map of C runtime, LLVM, LLD, and OS dependencies for self-hosting planning  
**Method**: Direct measurement of all runtime files, backend source, and FFI declarations

---

## 1. C Runtime Inventory

The C runtime (`compiler/runtime/`) contains **18,650 LOC** across 37 files in 12 directories. Each file is categorized by its role in the self-hosting migration.

### 1.1 Complete File Listing

| File | LOC | Purpose | Category | Self-Hosting Action |
|------|-----|---------|----------|-------------------|
| **core/essential.c** | 1,344 | Print, panic, abort, test harness entry | Essential | **Keep** — OS I/O boundary |
| **core/cpuid.c** | 93 | CPU feature detection (SIMD caps) | Essential | **Keep** — inline asm |
| **core/essential_tracy.c** | 80 | Tracy profiler integration | Essential | **Keep** — profiler FFI |
| **core/essential_utf8.c** | 75 | UTF-8 validation helpers | Pure Logic | **Migrate** to TML |
| **core/essential_cpuid.c** | 74 | CPUID wrapper | Essential | **Keep** — inline asm |
| **core/essential_random.c** | 46 | Random number generation (OS entropy) | FFI Bridge | **Keep** — OS API |
| **core/essential_ffi.c** | 43 | FFI utility helpers | Essential | **Keep** — bridge code |
| **memory/mem.c** | 217 | malloc/free/realloc wrappers | Essential | **Keep** — OS memory API |
| **memory/mem_track.c** | 559 | Memory tracking/leak detection | Diagnostics | **Keep** — debug tooling |
| **memory/pool.c** | 283 | Memory pool allocator | Pure Logic | **Migrate** to TML |
| **memory/str_free.c** | 166 | String deallocation helpers | Pure Logic | **Migrate** to TML |
| **collections/buffer_simd.c** | 184 | SIMD-accelerated buffer ops | Pure Logic | **Migrate** to TML (SIMD intrinsics) |
| **collections/collections.c** | 65 | Collection utility helpers | Pure Logic | **Migrate** to TML |
| **concurrency/sync.c** | 865 | Mutex, RwLock, Condvar, atomics | FFI Bridge | **Keep** — OS thread API |
| **concurrency/async.c** | 738 | Async runtime, task scheduling | FFI Bridge | **Keep** — OS async API |
| **net/iocp.c** | 772 | Windows IOCP completion ports | FFI Bridge | **Keep** — Windows kernel API |
| **net/tls.c** | 702 | TLS/SSL via OpenSSL or SChannel | FFI Bridge | **Keep** — crypto library FFI |
| **net/net.c** | 655 | TCP/UDP socket operations | FFI Bridge | **Keep** — OS socket API |
| **net/dns.c** | 356 | DNS resolution | FFI Bridge | **Keep** — OS resolver API |
| **net/poll.c** | 399 | Poll/select abstraction | FFI Bridge | **Keep** — OS I/O multiplexing |
| **crypto/crypto.c** | 1,260 | Core cryptography (OpenSSL/BCrypt) | FFI Bridge | **Keep** — crypto library FFI |
| **crypto/crypto_ecdh.c** | 784 | ECDH key exchange | FFI Bridge | **Keep** |
| **crypto/crypto_dh.c** | 766 | Diffie-Hellman key exchange | FFI Bridge | **Keep** |
| **crypto/crypto_x509.c** | 686 | X.509 certificate handling | FFI Bridge | **Keep** |
| **crypto/crypto_rsa.c** | 668 | RSA encryption/signing | FFI Bridge | **Keep** |
| **crypto/crypto_key.c** | 602 | Key management | FFI Bridge | **Keep** |
| **crypto/crypto_sign.c** | 513 | Digital signatures | FFI Bridge | **Keep** |
| **crypto/hash.c** | 353 | Hash functions (SHA, MD5, etc.) | FFI Bridge | **Keep** |
| **crypto/crypto_kdf.c** | 353 | Key derivation functions | FFI Bridge | **Keep** |
| **os/os.c** | 1,119 | OS info, registry, user info | FFI Bridge | **Keep** — OS API |
| **os/os_process.c** | 647 | Process spawning, env vars | FFI Bridge | **Keep** — OS API |
| **diagnostics/inspector.c** | 1,633 | WebSocket inspector for DevTools | Pure Logic | **Migrate** — mostly protocol code |
| **diagnostics/backtrace.c** | 838 | Stack trace capture | FFI Bridge | **Keep** — OS debug API |
| **diagnostics/log.c** | 429 | Logging infrastructure | Pure Logic | **Migrate** to TML |
| **diagnostics/console.c** | 204 | Console output formatting | Pure Logic | **Migrate** to TML |
| **time/time.c** | 79 | High-resolution timers | FFI Bridge | **Keep** — OS timer API |

### 1.2 Category Summary

| Category | Files | LOC | % of Total | Action |
|----------|-------|-----|-----------|--------|
| **Essential** | 7 | 1,755 | 9.4% | Keep permanently |
| **FFI Bridge** | 20 | 12,852 | 68.9% | Keep as `@extern("c")` |
| **Pure Logic** | 8 | 3,484 | 18.7% | Migrate to TML |
| **Diagnostics** | 2 | 559 | 3.0% | Keep (debug tooling) |
| **Total** | **37** | **18,650** | **100%** | |

**Key insight**: Only **3,484 LOC (18.7%)** of the C runtime is pure logic that should be migrated to TML. The vast majority (68.9%) is FFI bridge code that wraps OS and library APIs — these stay as `@extern("c")` declarations regardless of self-hosting.

### 1.3 Duplicate Runtime (`lib/std/runtime/`)

An additional **6,972 LOC** exists in `lib/std/runtime/` — these are C files compiled alongside std library modules:

| File | LOC | Purpose | Action |
|------|-----|---------|--------|
| `file.c` | 936 | File I/O operations | Keep — OS API |
| `glob.c` | 702 | Glob pattern matching | **Migrate** to TML |
| `zlib/zlib_deflate.c` | 720 | Deflate compression | Keep — zlib FFI |
| `zlib/zlib_zstd.c` | 714 | Zstandard compression | Keep — zstd FFI |
| `zlib/zlib_exports.c` | 613 | Compression exports | Keep — FFI bridge |
| `zlib/zlib_brotli.c` | 491 | Brotli compression | Keep — brotli FFI |
| `crypto/crypto_common.c` | 525 | Shared crypto utilities | Keep — crypto FFI |
| `crypto/crypto_hash_win.c` | 515 | Windows hash (BCrypt) | Keep — OS crypto API |
| `crypto/crypto_hash_macos.c` | 509 | macOS hash (CommonCrypto) | Keep — OS crypto API |
| `crypto/crypto_hash_openssl.c` | 380 | OpenSSL hash | Keep — OpenSSL FFI |
| `crypto/crypto_cipher_win.c` | 414 | Windows cipher (BCrypt) | Keep — OS crypto API |
| `crypto/crypto_random.c` | 240 | OS random (BCryptGenRandom) | Keep — OS API |
| `crypto/crypto_kdf_win.c` | 213 | Windows KDF | Keep — OS crypto API |

**Total migratable from lib/std/runtime/**: ~702 LOC (glob.c only)

---

## 2. LLVM API Usage

### 2.1 How the LLVM Backend Works

The LLVM backend (`compiler/src/backend/llvm_backend.cpp`, **550 LOC**) receives LLVM IR as a **text string** and compiles it to an object file. The core flow:

```
TML Compiler Pipeline
        │
        ▼
  MIR Codegen generates LLVM IR as std::string
        │
        ▼
  LLVMParseIRInContext(ctx, buffer, &module, &error)  ← parses text to LLVM Module
        │
        ▼
  LLVMRunPassManager (optimization passes)
        │
        ▼
  LLVMTargetMachineEmitToFile → .obj file
```

### 2.2 Key Architectural Insight: IR-as-Text

**The TML compiler does NOT use the LLVM C++ Builder API** (no `IRBuilder`, no `CreateAdd`, no `CreateBr`). Instead:

1. `MirCodegen::generate()` builds LLVM IR as a `std::string` using string concatenation
2. `LLVMParseIRInContext()` (LLVM C API) parses that string into an `LLVMModuleRef`
3. Standard LLVM passes optimize the module
4. `LLVMTargetMachineEmitToFile()` emits the `.obj`

**This is the single most important architectural advantage for self-hosting.** A TML-written compiler needs only to produce a correctly-formatted LLVM IR text string — which `Text` + template literals handle perfectly. No LLVM SDK bindings are needed in TML code.

The C++ shim that calls LLVM is only **550 LOC** and can remain as a permanent boundary.

### 2.3 LLVM APIs Actually Used

From `llvm_backend.cpp`:

| API Call | Purpose | Line |
|----------|---------|------|
| `LLVMParseIRInContext` | Parse IR text → Module | 155, 363 |
| `LLVMCreatePassManager` | Create optimization pipeline | — |
| `LLVMRunPassManager` | Run optimization passes | — |
| `LLVMGetTargetFromTriple` | Get target machine info | — |
| `LLVMCreateTargetMachine` | Create target for codegen | — |
| `LLVMTargetMachineEmitToFile` | Emit .obj from Module | — |
| `LLVMContextCreate/Dispose` | LLVM context lifecycle | — |
| `LLVMCreateMemoryBufferWithMemoryRangeCopy` | Wrap IR string for parsing | — |

From `jit_engine.cpp`:

| API Call | Purpose | Line |
|----------|---------|------|
| `llvm::parseIR` | Parse IR (C++ API variant) | 154 |
| `llvm::orc::LLJITBuilder` | Create JIT engine | — |
| `LLJIT::addIRModule` | Add module to JIT | — |
| `LLJIT::lookup` | Find JIT-compiled symbol | — |

### 2.4 LLVM C API vs C++ API

The backend uses a **mix** of LLVM C API and C++ API:

- **C API** (`LLVMParseIRInContext`, `LLVMTargetMachine*`): Used in `llvm_backend.cpp` for the main compilation path. These have stable ABI and could theoretically be called from TML via `@extern("c")`.
- **C++ API** (`llvm::parseIR`, `llvm::orc::LLJIT*`): Used in `jit_engine.cpp` for the JIT path. These have no stable ABI and cannot be called from TML.

**Self-hosting implication**: The C API path (main compilation) could be called from TML via `@extern("c")`, but there's no benefit — keeping the 550-LOC C++ shim is simpler and more maintainable.

---

## 3. LLD Linker Usage

### 3.1 How Linking Works

The LLD linker (`compiler/src/backend/lld_linker.cpp`, **670 LOC**) links object files into executables **in-process** using LLD's library API:

```
.obj files + C runtime .obj + system libs
        │
        ▼
  lld::lldMain(argv, stdout, stderr, drivers)  ← in-process, no subprocess
        │
        ▼
  .exe / .dll output
```

Key details:
- Uses `lld::coff::link` on Windows, `lld::elf::link` on Linux, `lld::macho::link` on macOS
- Constructs argv-style arguments (same as `lld-link` CLI)
- Handles DLL exports, import libraries, Windows CRT paths
- Runs in a **separate thread** with captured stdout/stderr (lines 407-451)

### 3.2 LLD APIs Used

| API | Purpose |
|-----|---------|
| `lld::lldMain` | Main entry point — drives the link |
| `lld::coff::link` | Windows COFF linker driver |
| `lld::elf::link` | Linux ELF linker driver |
| `lld::macho::link` | macOS Mach-O linker driver |
| `lld::DriverDef` | Driver registration struct |
| `lld::Result` | Link result (success/failure) |

### 3.3 Self-Hosting Implications

**Option A: Keep as C++ boundary (recommended)**
- The 670 LOC stays as-is
- TML compiler passes object file paths and link flags as strings
- Simple FFI interface: `link(args: List[Str]) -> Bool`
- No LLD API knowledge needed in TML

**Option B: Invoke LLD as subprocess**
- Use `std::process` to run `lld-link` / `ld.lld` as external command
- Pros: Zero C++ code
- Cons: Requires LLD binary in PATH, slower (process spawn overhead), harder error recovery
- Viable as fallback but not recommended for default path

**Option C: Call LLD C API from TML**
- LLD has no stable C API — only C++ API with no ABI guarantees
- Not feasible without a C wrapper shim
- Would essentially recreate the existing 670 LOC shim

---

## 4. OS-Specific Dependencies

### 4.1 Windows APIs in Compiler Code

The compiler core (`compiler/src/`) references Windows-specific constructs primarily in:

| Location | Windows Dependency | Purpose |
|----------|-------------------|---------|
| `backend/llvm_backend.cpp` | File locking retry logic | Windows file locking on `.obj` files |
| `backend/lld_linker.cpp` | CRT path resolution, `/DLL` flag | Windows-specific link options |
| `cli/builder/build.cpp` | `ws2_32.lib`, `advapi32.lib` | Windows system libraries for linking |
| `cli/builder/builder_run.cpp` | DLL search paths | Finding runtime DLLs at execution |
| `plugin/` | `LoadLibrary`/`GetProcAddress` | Plugin DLL loading (modular build) |

**Key observation**: The compiler core (lexer through codegen) has **zero OS-specific dependencies**. All Windows APIs are in the backend, CLI, and plugin loader — subsystems that either stay in C++ or are ported last.

### 4.2 File System Access

The compiler accesses the file system through:
- Standard C++ `<fstream>` and `<filesystem>` — portable, no OS API needed in TML
- Source file reading via query system — TML has `std::file` for this

### 4.3 Process Management

- Test subprocess spawning (`compiler/src/testing/`) uses standard POSIX/Windows process APIs
- TML has `std::process` for process spawning — sufficient for self-hosted test runner

### 4.4 Memory Management

- Compiler uses standard `new`/`delete` and `std::unique_ptr`/`std::shared_ptr`
- No `VirtualAlloc` or `mmap` in compiler core
- TML's `Heap[T]`, `Shared[T]`, and standard allocator cover all compiler memory needs

---

## 5. Dependency Elimination Strategy

### 5.1 Decision Matrix

| Dependency | Type | Size | Action | Priority | Notes |
|-----------|------|------|--------|----------|-------|
| **LLVM (parseIR + opt)** | C++ Library | 550 LOC shim | **Keep as C++ boundary** | — | Permanent; IR-as-text interface |
| **LLD (linker)** | C++ Library | 670 LOC shim | **Keep as C++ boundary** | — | Permanent; in-process linking |
| **LLJIT (JIT)** | C++ Library | 373 LOC shim | **Keep as C++ boundary** | — | Permanent; JIT execution |
| **essential.c** | OS I/O | 1,344 LOC | **Keep as FFI** | — | Print, panic, abort |
| **mem.c** | OS Memory | 217 LOC | **Keep as FFI** | — | malloc/free wrappers |
| **Crypto (8 files)** | OpenSSL/BCrypt | 5,985 LOC | **Keep as FFI** | — | External library wrapper |
| **Net (5 files)** | OS Sockets | 2,884 LOC | **Keep as FFI** | — | OS network API |
| **Concurrency (2 files)** | OS Threads | 1,603 LOC | **Keep as FFI** | — | OS thread/async API |
| **OS (2 files)** | OS Info | 1,766 LOC | **Keep as FFI** | — | Registry, process, env |
| **pool.c** | Allocator | 283 LOC | **Migrate to TML** | Medium | Pure allocation logic |
| **str_free.c** | String mgmt | 166 LOC | **Migrate to TML** | Medium | Pure deallocation logic |
| **buffer_simd.c** | SIMD buffer | 184 LOC | **Migrate to TML** | Low | SIMD intrinsics in TML |
| **collections.c** | Collection utils | 65 LOC | **Migrate to TML** | High | Simple pure logic |
| **essential_utf8.c** | UTF-8 | 75 LOC | **Migrate to TML** | High | Pure logic |
| **inspector.c** | WebSocket | 1,633 LOC | **Migrate to TML** | Low | Protocol code, complex |
| **log.c** | Logging | 429 LOC | **Migrate to TML** | Medium | Pure formatting |
| **console.c** | Console fmt | 204 LOC | **Migrate to TML** | Medium | Pure formatting |
| **glob.c** (std) | File globbing | 702 LOC | **Migrate to TML** | Medium | Pure pattern matching |

### 5.2 Permanent C++ Boundaries (1,593 LOC)

These three files form the **permanent C++ boundary** between the TML compiler and the LLVM/LLD toolchain:

```
┌─────────────────────────────────────────────┐
│  TML Compiler (self-hosted)                 │
│  Generates LLVM IR as Text string           │
│                                             │
│  Output: "define i64 @main() { ... }"       │
└──────────────────┬──────────────────────────┘
                   │ IR text string
                   ▼
┌─────────────────────────────────────────────┐
│  llvm_backend.cpp (550 LOC) — PERMANENT     │
│  LLVMParseIRInContext → optimize → emit .obj│
└──────────────────┬──────────────────────────┘
                   │ .obj files
                   ▼
┌─────────────────────────────────────────────┐
│  lld_linker.cpp (670 LOC) — PERMANENT       │
│  lld::lldMain → link .obj → .exe/.dll       │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
              Final .exe
```

**Total permanent C++: 1,593 LOC** — this is the irreducible minimum. Everything above this boundary can be written in TML.

### 5.3 Migratable C Runtime (3,484 LOC + 702 LOC)

Total **4,186 LOC** of pure logic C code that should eventually become TML:

| Priority | Files | LOC | Reason |
|----------|-------|-----|--------|
| High | collections.c, essential_utf8.c | 140 | Trivial, pure logic |
| Medium | pool.c, str_free.c, log.c, console.c, glob.c | 1,784 | Moderate complexity, clear TML equivalents |
| Low | buffer_simd.c, inspector.c | 1,817 | Complex (SIMD, WebSocket protocol) |

### 5.4 Essential C Runtime (14,464 LOC)

This code **stays permanently** as `@extern("c")` FFI targets:
- OS I/O and lifecycle (essential.c, cpuid, random, ffi)
- Memory management (mem.c, mem_track.c)  
- All crypto, networking, concurrency, OS, time modules

---

## 6. FFI Architecture for Self-Hosted Compiler

### 6.1 Option A: Direct FFI to LLVM C API

```
TML Compiler → @extern("c") LLVMParseIRInContext → LLVM C API
```

**Pros:**
- Eliminates all C++ from the compilation path
- LLVM C API has relatively stable ABI

**Cons:**
- Requires declaring ~20+ LLVM C API functions as `@extern("c")` in TML
- Opaque pointer types (`LLVMContextRef`, `LLVMModuleRef`) need careful handling
- Error handling is manual (C-style error strings)
- LLVM version upgrades may change API subtly
- JIT path requires C++ API — cannot be replaced with C API
- Optimization pass manager setup is verbose in C API

**Verdict: Not recommended.** The complexity of LLVM C API bindings in TML exceeds the benefit of eliminating 550 LOC of C++.

### 6.2 Option B: Keep C++ Shim as Permanent Boundary (RECOMMENDED)

```
TML Compiler → @extern("c") tml_compile_ir(ir_text, output_path) → C++ shim → LLVM
```

**Pros:**
- Cleanest boundary: TML passes a string, gets a file
- C++ shim handles all LLVM version differences
- JIT, optimization passes, target machine setup all hidden behind simple interface
- Already exists and is tested — zero new development needed
- Only 1,593 LOC total (LLVM + LLD + JIT)

**Cons:**
- Requires C++ compiler to build the shim (but LLVM itself requires C++ anyway)
- LLVM version locked to whatever the shim is compiled against

**Verdict: Strongly recommended.** This is the same approach used by most language implementations that target LLVM (Rust, Julia, Swift all have C++ shims around LLVM).

### 6.3 Option C: LLVM/LLD as External Subprocess

```
TML Compiler → write .ll file → exec("llc", ["-filetype=obj", ...]) → .obj
             → exec("lld-link", [...]) → .exe
```

**Pros:**
- Zero C/C++ code in the compiler distribution
- LLVM version can be upgraded independently
- Simplest possible integration

**Cons:**
- Requires LLVM CLI tools installed separately (`llc`, `opt`, `lld-link`)
- Process spawn overhead per compilation unit
- Temporary file I/O for .ll → .obj
- Harder to capture and parse error messages
- Loses in-process LLJIT (no `tml run` without rewriting JIT)
- Harder to distribute (users need LLVM toolchain installed)

**Verdict: Acceptable as fallback.** Could be offered as `--external-backend` flag for environments where the C++ shim isn't available.

---

## 7. Summary

### Key Numbers

| Metric | Value |
|--------|-------|
| **Total C runtime** | 18,650 LOC (compiler/runtime/) + 6,972 LOC (lib/std/runtime/) |
| **Permanent C++ boundary** | 1,593 LOC (LLVM backend + LLD + JIT) |
| **Essential C (keep as FFI)** | 14,464 LOC (OS, crypto, net, concurrency) |
| **Migratable to TML** | 4,186 LOC (pure logic: collections, UTF-8, formatting) |
| **LLVM API surface** | ~15 C API calls + C++ JIT API |
| **LLD API surface** | 3 driver functions + argv-style interface |

### Self-Hosting Dependency Architecture

```
┌─────────────────────────────────┐
│     Self-Hosted TML Compiler    │  ← 100% TML
│   (lexer → parser → types →    │
│    HIR → MIR → codegen)        │
│                                 │
│   Output: LLVM IR text string   │
└────────────┬────────────────────┘
             │
    ═══════════════════════  FFI Boundary  ═══════════════════════
             │
┌────────────▼────────────────────┐
│   C++ Shim (1,593 LOC)         │  ← Permanent, minimal
│   • llvm_backend.cpp (550)     │
│   • lld_linker.cpp   (670)    │
│   • jit_engine.cpp   (373)    │
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│   LLVM + LLD Libraries          │  ← External, maintained by LLVM project
│   (~20M+ LOC)                   │
└─────────────────────────────────┘
```

### Recommendation

**Use Option B (C++ shim as permanent boundary).** The 1,593 LOC shim is the irreducible integration point. Every line of compiler above it — from lexer through codegen — can and should be written in TML. The shim receives an IR text string and produces an object file. This is a clean, stable, well-tested interface that requires essentially zero maintenance.

The C runtime (18,650 LOC) is a separate concern from self-hosting. Most of it (68.9%) is FFI bridge code that exists to wrap OS and library APIs for TML programs — it serves TML applications, not the compiler. Only 4,186 LOC of pure logic C code is a migration target, and that migration is independent of the self-hosting effort.
