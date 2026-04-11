# Native C Compilation — Eliminating the Zig CC Dependency

**Date**: 2026-04-05
**Scope**: Analysis of how TML can compile its C runtime without depending on an external C compiler
**Context**: Linker analysis (docs/analyses/linker/) covers the linking side; this document covers C compilation
**Companion**: docs/analyses/linker/README.md, docs/analyses/compiler-selfhosting/04-runtime-dependencies.md

---

## 1. Why TML Needs C Compilation

TML is a compiled systems language. The compiler itself is written in C++. Between the C++
compiler source and the final `tml.exe` there is a layer of C runtime code that must also
be compiled. This C runtime provides the OS interface that TML programs depend on: printing,
memory allocation, networking, cryptography, and panic handling.

Today, this C runtime is compiled using **Zig CC** — a wrapper around Clang 20 that Zig
bundles with portable MSVC-ABI support. When `scripts/build.bat` runs, it invokes Zig CC
to compile every `.c` file in `compiler/runtime/` and `lib/std/runtime/` into COFF object
files, which are then linked into `tml.exe` by the embedded LLD.

This creates a hard dependency on Zig. Every developer building TML from source needs Zig
installed. The CI pipeline needs Zig. Any future user who wants to bootstrap TML from source
needs Zig. This is acceptable today but becomes a problem as TML moves toward self-containment.

The goal of this analysis is to identify every viable path to eliminating the Zig CC dependency
for C runtime compilation, and recommend the right strategy for each phase of the roadmap.

---

## 2. The C Runtime Files — Complete Inventory

The following table lists every C source file in the TML runtime, its line count, purpose,
and whether migration to TML is feasible.

### 2.1 compiler/runtime/ (Primary Runtime)

| File | LOC | Purpose | Migration Feasibility |
|------|-----|---------|----------------------|
| `core/essential.c` | 1,344 | Print to stdout/stderr, panic, abort, test harness entry point | **Keep in C** — direct OS I/O, platform-specific |
| `core/essential_utf8.c` | 75 | UTF-8 byte validation helpers | **Migrate to TML** — pure logic, no OS calls |
| `core/essential_ffi.c` | 43 | FFI utility helpers (pointer casting, alignment) | **Keep in C** — bridge code |
| `core/essential_random.c` | 46 | OS entropy (BCryptGenRandom on Windows, getrandom on Linux) | **Keep in C** — OS API |
| `core/essential_tracy.c` | 80 | Tracy profiler integration (conditional on TRACY_ENABLE) | **Keep in C** — profiler FFI |
| `core/cpuid.c` | 93 | CPU feature detection via CPUID instruction | **Keep in C** — inline assembly |
| `core/essential_cpuid.c` | 74 | Higher-level CPUID wrapper (AVX2/AVX-512 flags) | **Keep in C** — inline assembly |
| `memory/mem.c` | 217 | malloc/free/realloc wrappers with alignment | **Keep in C** — OS memory API |
| `memory/mem_track.c` | 559 | Debug memory tracking: allocation map, leak detection | **Keep in C** — debug tooling |
| `memory/pool.c` | 283 | Slab/pool allocator for small fixed-size objects | **Migrate to TML** — pure allocation logic |
| `memory/str_free.c` | 166 | String deallocation: recursively frees interned strings | **Migrate to TML** — pure logic |
| `collections/buffer_simd.c` | 184 | SIMD-accelerated buffer operations (memcmp, memmove) | **Migrate to TML** — TML has SIMD intrinsics |
| `collections/collections.c` | 65 | Collection utility helpers (growth factor, capacity) | **Migrate to TML** — trivial |
| `concurrency/sync.c` | 865 | Mutex, RwLock, Condvar, spin locks via OS primitives | **Keep in C** — OS thread API (CRITICAL_SECTION, pthread) |
| `concurrency/async.c` | 738 | Async task scheduler, coroutine switching | **Keep in C** — platform-specific context switching |
| `net/iocp.c` | 772 | Windows IOCP completion port integration | **Keep in C** — Windows kernel API |
| `net/tls.c` | 702 | TLS via OpenSSL (Linux) or SChannel/BCrypt (Windows) | **Keep in C** — crypto library FFI |
| `net/net.c` | 655 | TCP/UDP socket operations, Winsock/POSIX | **Keep in C** — OS socket API |
| `net/dns.c` | 356 | DNS resolution via OS resolver | **Keep in C** — OS resolver API |
| `net/poll.c` | 399 | Poll/epoll/kqueue abstraction | **Keep in C** — OS I/O multiplexing |
| `crypto/crypto.c` | 1,260 | AES, ChaCha20, HMAC via OpenSSL/BCrypt | **Keep in C** — crypto library FFI |
| `crypto/crypto_ecdh.c` | ~784 | ECDH key exchange (NIST curves) | **Keep in C** — crypto FFI |
| `crypto/crypto_dh.c` | ~766 | Diffie-Hellman group key exchange | **Keep in C** — crypto FFI |
| `crypto/crypto_x509.c` | ~686 | X.509 certificate parsing and verification | **Keep in C** — crypto FFI |
| `crypto/crypto_rsa.c` | ~668 | RSA encryption and signing | **Keep in C** — crypto FFI |
| `crypto/crypto_key.c` | ~602 | Key import/export, key management | **Keep in C** — crypto FFI |
| `crypto/crypto_sign.c` | ~513 | Ed25519/ECDSA digital signatures | **Keep in C** — crypto FFI |
| `crypto/hash.c` | 353 | SHA-256/512, MD5, Blake2 wrappers | **Keep in C** — crypto FFI |
| `crypto/crypto_kdf.c` | ~353 | HKDF, PBKDF2 key derivation | **Keep in C** — crypto FFI |
| `os/os.c` | 1,119 | OS info, registry queries, user info, system paths | **Keep in C** — OS API |
| `os/os_process.c` | 647 | Process spawning, environment variables, stdin/stdout pipes | **Keep in C** — OS API |
| `diagnostics/inspector.c` | 1,633 | WebSocket DevTools inspector protocol | **Migrate to TML** — mostly protocol logic |
| `diagnostics/backtrace.c` | 838 | Stack trace capture (StackWalk64 on Windows, libunwind on Linux) | **Keep in C** — OS debug API |
| `diagnostics/log.c` | 429 | Structured logging with level filtering | **Migrate to TML** — pure formatting |
| `diagnostics/console.c` | 204 | Console output formatting (colors, box drawing) | **Migrate to TML** — pure formatting |
| `time/time.c` | 79 | High-resolution monotonic timer (QueryPerformanceCounter/clock_gettime) | **Keep in C** — OS timer API |

### 2.2 lib/std/runtime/ (Std Library Runtime)

| File | LOC | Purpose | Migration Feasibility |
|------|-----|---------|----------------------|
| `file.c` | 936 | File I/O (CreateFile, ReadFile, WriteFile on Windows) | **Keep in C** — OS file API |
| `glob.c` | 702 | Glob pattern matching for directory enumeration | **Migrate to TML** — pure pattern logic |
| `zlib/zlib_deflate.c` | 720 | Deflate/inflate via zlib | **Keep in C** — zlib FFI |
| `zlib/zlib_zstd.c` | 714 | Zstandard compression | **Keep in C** — zstd FFI |
| `zlib/zlib_exports.c` | 613 | Compression algorithm dispatch | **Keep in C** — FFI bridge |
| `zlib/zlib_brotli.c` | 491 | Brotli compression | **Keep in C** — brotli FFI |
| `crypto/crypto_common.c` | 525 | Shared crypto utilities | **Keep in C** — crypto FFI |
| `crypto/crypto_hash_win.c` | 515 | BCrypt hash (Windows) | **Keep in C** — OS crypto API |
| `crypto/crypto_hash_macos.c` | 509 | CommonCrypto hash (macOS) | **Keep in C** — OS crypto API |
| `crypto/crypto_hash_openssl.c` | 380 | OpenSSL hash | **Keep in C** — OpenSSL FFI |
| `crypto/crypto_cipher_win.c` | 414 | BCrypt cipher (Windows) | **Keep in C** — OS crypto API |
| `crypto/crypto_random.c` | 240 | BCryptGenRandom (Windows OS entropy) | **Keep in C** — OS API |
| `crypto/crypto_kdf_win.c` | 213 | BCrypt KDF (Windows) | **Keep in C** — OS crypto API |

### 2.3 Migration Summary

| Category | LOC | Files | Action |
|----------|-----|-------|--------|
| Essential (OS I/O, panic, assembly) | ~1,755 | 7 | Keep permanently |
| FFI Bridge (crypto, net, concurrency, OS) | ~13,200 | 26 | Keep as `@extern("c")` targets |
| Pure Logic (migrateable) | ~4,186 | 8+ | Migrate to TML incrementally |
| **Total compiler/runtime/** | **~13,938** | **36** | |
| **Total lib/std/runtime/** | **~5,763** | **13** | |
| **Grand total C runtime** | **~19,701** | **49** | |

The critical insight: only **~4,186 LOC (21%)** of the C runtime is pure logic that should
eventually become TML. The remaining 79% wraps OS APIs and external libraries — it stays
as C regardless of migration effort, called via `@extern("c")` FFI from TML.

---

## 3. Current Build Approach: Zig CC

### What Zig CC Is

`zig cc` is not a C compiler — it is a wrapper around the Clang 20 compiler bundled with Zig.
Zig bundles Clang with:

- Portable MSVC-ABI support via `-fms-compatibility`
- Bundled UCRT headers (no Visual Studio installation needed)
- Target-specific sysroots for 40+ platforms
- Hermetic builds (same output regardless of host system state)

When `scripts/build.bat` runs, it invokes `zig cc` with the target `x86_64-windows-msvc`,
which tells Clang to produce COFF object files with MSVC calling conventions. These objects
are then linked into `tml.exe` and `tml_compiler.dll` by the embedded LLD.

### Where Zig CC Is Invoked

In the TML build system, Zig CC appears in CMakeLists as the `CMAKE_C_COMPILER` and
`CMAKE_CXX_COMPILER`. The `zig-cc.bat` and `zig-cxx.bat` wrappers translate every CMake
compiler invocation into `zig cc -target x86_64-windows-msvc [args]`.

The C runtime files are compiled by CMake's `add_library()` directives. Each `.c` file in
`compiler/runtime/` becomes a CMake source, compiled by Zig CC, into a COFF `.obj` that is
statically linked into the final binary.

### Limitations of the Current Approach

1. **External dependency**: Zig must be installed on the developer machine and CI runners.
2. **Version coupling**: `build.bat` checks for Zig `0.14.0-dev` or `0.14.0` exactly. Any
   version mismatch causes a build failure.
3. **Windows-first**: The Zig CC wrapper scripts are `.bat` files. Linux builds require
   separate wrapper scripts.
4. **No in-process C compilation**: Every C file requires launching a subprocess. For the
   ~49 C files, this is ~49 process launches — measurable overhead on slow machines.
5. **Bootstrap dependency**: Any user who wants to build TML from source needs Zig. This
   contradicts the self-contained toolchain goal.

---

## 4. Alternative C Compilers for Embedding

Several small C compilers can be embedded into `tml.exe` as a library, eliminating the need
to invoke an external compiler process. Each is evaluated on: size, license, library API
availability, optimization quality, and target support.

### 4.1 TCC (Tiny C Compiler)

**Author**: Fabrice Bellard (also author of QEMU, FFmpeg)
**License**: LGPL 2.1
**Size**: ~100KB compiled library, ~30K lines of C source
**Optimization**: None (single-pass compilation, no optimizations at all)
**Targets**: x86, x86_64, ARM (32-bit), AArch64 (partial)

TCC is designed to be usable as a C library. The public API is minimal but complete:

```c
TCCState *tcc = tcc_new();
tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);   // or MEMORY, EXE, DLL
tcc_add_include_path(tcc, "/path/to/headers");
tcc_compile_string(tcc, c_source_code);     // compile from memory
// or:
tcc_add_file(tcc, "file.c");               // compile from file
tcc_output_file(tcc, "output.obj");        // write .obj to disk
// or:
tcc_relocate(tcc, TCC_RELOCATE_AUTO);      // load into memory
void *sym = tcc_get_symbol(tcc, "func_name"); // get function pointer
tcc_delete(tcc);
```

TCC can compile to memory and immediately execute the compiled code — it is frequently used
for runtime C compilation and JIT use cases. Several projects use TCC as an embedded C
compiler: Nim's `--cc:tcc` mode, various scripting languages that want C interop.

**Critical limitation for TML**: TCC produces completely unoptimized code. The C runtime
files contain SIMD intrinsics (`_mm256_loadu_si256`, AVX2 operations in `buffer_simd.c`).
TCC does not support SIMD intrinsics — it would fail to compile `buffer_simd.c`. More
importantly, TCC's unoptimized output for `crypto/crypto.c` would be 3-5x slower than
Clang's optimized output, affecting TML program performance directly.

**Verdict**: Suitable for bootstrapping and for compiling simple runtime files (`essential.c`,
`mem.c`, `os.c`). NOT suitable for SIMD-heavy files or crypto without pre-compiling those
to `.obj` with Clang.

### 4.2 chibicc

**Author**: Rui Ueyama (also author of LLD, mold)
**License**: MIT
**Size**: ~7,000 lines of C source
**Optimization**: None (educational compiler, single-pass)
**Targets**: x86_64 only

chibicc is an educational C compiler written to accompany a book on compiler implementation.
It supports C11 including most features needed for the TML runtime (structs, pointers,
function pointers, variadic args). It does NOT have a library API — it is invoked as a
command-line tool.

**Critical limitation for TML**: No library API means subprocess invocation only. The
educational-grade code quality means SIMD intrinsics and complex macros are unlikely to
work. The absence of optimizations is acceptable for bootstrap but not for production
runtime compilation.

**Verdict**: Useful for understanding C compiler internals. Not suitable for embedding in TML.

### 4.3 cproc

**Author**: Michael Forney
**License**: ISC (permissive)
**Size**: ~7,000 lines of C source
**Optimization**: Delegates to QBE backend (~13K lines)
**Targets**: x86_64, AArch64, RISC-V (via QBE)

cproc is a C11 compiler that uses QBE as its intermediate representation and backend. QBE
is a small compiler backend that produces reasonably optimized machine code. The combination
of cproc + QBE produces code roughly comparable to `gcc -O1`.

No library API — command-line only. The QBE backend supports limited SIMD.

**Verdict**: Interesting for its QBE integration but lacks library API. Not suitable for embedding.

### 4.4 8cc / 9cc

**Author**: Rui Ueyama (8cc), then extended (9cc)
**License**: MIT
**Size**: ~5,000 lines of C source
**Optimization**: None
**Targets**: x86_64 only

8cc is an earlier educational C compiler by the same author as chibicc. No library API,
no SIMD support, command-line only.

**Verdict**: Historical interest only. Not suitable for embedding.

### 4.5 Bundled Clang as a Library

LLVM includes Clang as a C++ library. Since TML already links against LLVM (for code
generation), adding Clang's libraries adds ~30-50MB to the binary but requires no new
external dependency.

Clang's library API for compilation:

```cpp
// Create compiler instance
clang::CompilerInstance CI;
CI.createDiagnostics();

// Set compilation options
clang::CompilerInvocation::CreateFromArgs(
    *CI.getInvocation(),
    {"-x", "c", "-O2", "-target", "x86_64-windows-msvc", "input.c"},
    CI.getDiagnostics());

// Execute compilation
clang::ExecuteCompilerInvocation(&CI);
```

This is the same quality as `zig cc` (since Zig CC wraps Clang) with full optimization,
SIMD intrinsic support, and all C11 features. The tradeoff is binary size.

**Verdict**: Best option for production quality. TML already depends on LLVM; adding Clang
library linkage is incremental. This is Phase 1b in the linker roadmap.

### 4.6 Comparison Table

| Compiler | Size | Optimization | SIMD | Library API | License | Suitable for TML |
|----------|------|-------------|------|-------------|---------|-----------------|
| TCC | ~100KB | None | No | Yes | LGPL 2.1 | Bootstrap only |
| chibicc | ~200KB | None | No | No | MIT | No |
| cproc+QBE | ~500KB | ~O1 via QBE | Partial | No | ISC | No |
| 8cc | ~150KB | None | No | MIT | No | No |
| Bundled Clang | ~30-50MB | Full (O0-O3) | Yes | Yes (C++) | Apache 2.0 | Yes (Phase 1b) |
| Zig CC (current) | External | Full (O0-O3) | Yes | No (subprocess) | MIT | Current only |

---

## 5. C Header Parsing — The FFI Challenge

When TML code includes `@extern("c")` declarations, those declarations must accurately match
the C function signatures. Today this is a manual process — developers read the C headers
and write matching TML declarations. For a self-contained toolchain, TML needs a way to
parse C headers automatically, especially for the runtime's own header files.

### 5.1 Full C Parser

A full C parser handles `#include`, macros (`#define`, `#ifdef`), typedefs, struct layouts,
and function declarations. This is essentially a C preprocessor + parser. Building one from
scratch is 10-20K lines of code and takes months.

The benefit: perfect accuracy, handles all C idioms, generates exact TML `@extern("c")`
bindings automatically.

The cost: significant implementation effort, ongoing maintenance for corner cases.

### 5.2 libclang AST (LLVM)

LLVM ships `libclang` — a C API for Clang's AST. Given a C header, libclang produces a
traversable AST with all types, function signatures, and macros resolved.

```c
CXIndex index = clang_createIndex(0, 0);
CXTranslationUnit tu = clang_parseTranslationUnit(
    index, "header.h", NULL, 0, NULL, 0,
    CXTranslationUnit_SkipFunctionBodies);
// Walk AST with clang_visitChildren
clang_disposeTranslationUnit(tu);
clang_disposeIndex(index);
```

Since TML already depends on LLVM, adding `libclang` linkage is free. This is the most
accurate approach and generates correct FFI bindings for any C header.

**Critical issue**: Using libclang for the runtime headers defeats the stated goal of
eliminating Clang as a dependency — libclang IS Clang. However, if TML already embeds
Clang (Phase 1b), then libclang comes for free.

**Verdict**: Best accuracy, correct approach IF Clang is already bundled.

### 5.3 Tree-sitter C Grammar

Tree-sitter provides a fast incremental parser for C. It can parse C headers without
executing the preprocessor:

```
tree-sitter parse header.h → CST (concrete syntax tree)
Walk CST → extract function declarations, struct definitions
```

Tree-sitter is correct for syntactic structure but cannot resolve:
- Macros (`#define MAX_SIZE 1024` — `MAX_SIZE` appears as an identifier, not `1024`)
- Platform-conditional code (`#ifdef _WIN32`)
- `typedef` chains (`typedef struct Foo_ Foo;` — need to track alias chains)

For TML's own runtime headers (which are controlled, well-known, and not heavily macro-driven),
Tree-sitter is sufficient. For general C header parsing it is not.

**Verdict**: Suitable for TML's internal use (parsing TML's own C runtime headers). Not
suitable for general-purpose C header import.

### 5.4 Custom Minimal C Header Parser

For TML's specific use case — parsing the handful of runtime headers to generate `@extern("c")`
declarations — a minimal custom parser is the right approach. TML's runtime headers are:

- No complex macros (no function-like macros with arguments)
- No complex typedefs (straightforward `typedef struct` patterns)
- No platform conditionals in the public API surface
- ~200-300 function declarations total

A parser for this specific subset can be written in ~500 lines of TML. It only needs to
handle:
1. `typedef struct X { ... } X;` — struct definitions
2. `void func(type arg, ...);` — function declarations
3. `#include` directives (follow-the-chain for type resolution)
4. `typedef T U;` — simple type aliases

This is not a general C parser — it is a TML runtime declaration extractor. It generates
the `@extern("c")` stubs that TML code uses to call into the C runtime.

**Verdict**: Best tradeoff for the specific problem. 500 LOC in TML, correct for TML's
runtime headers, no external dependencies.

---

## 6. The Nuclear Option: Rewrite ALL C Runtime in TML

The project is already on this path. Phase 4 migrated ~5,210 lines of C runtime to pure TML.
This section analyzes what remains and what is truly irreplaceable.

### 6.1 What Has Already Been Migrated (Phase 4)

- All `collections/` logic (List, HashMap, Buffer, BTreeMap) — now pure TML
- All text formatting (`fmt/`) — now pure TML
- All math formatting — now pure TML
- All search algorithms (BM25, HNSW, cosine distance) — now pure TML
- Iterator adapters, closures, slice operations — now pure TML

### 6.2 What CAN Be Migrated to TML (Next Wave)

**I/O via syscall wrappers**: `essential.c`'s print/panic functionality wraps `WriteFile`
(Windows) or `write` (Linux). These can be replaced with TML `lowlevel` blocks that call
the OS directly via `@extern("c")` declarations of the raw Win32/POSIX APIs. The Zig
language does exactly this in `std.os` — all I/O is implemented as syscall wrappers in
Zig, not C.

**Memory allocation**: `mem.c` wraps `HeapAlloc`/`HeapFree` (Windows) or `malloc`/`free`
(POSIX). These APIs can be declared as `@extern("c")` directly in TML:
```
@extern("c") func HeapAlloc(heap: RawPtr, flags: U32, bytes: U64) -> RawPtr
```
The 217 LOC of `mem.c` would become ~50 lines of TML declarations.

**Pool allocator**: `pool.c` is pure logic (slab allocation algorithm). No OS dependency.
Straightforward to port to TML. Estimated: ~200 lines of TML.

**String management**: `str_free.c` is pure deallocation logic. ~100 lines of TML.

**Formatting**: `log.c` and `console.c` are pure string formatting — already similar to
what `std::fmt` does in TML.

### 6.3 What CANNOT Be Migrated to TML

**Crypto (OpenSSL/BCrypt)**: The crypto files are FFI wrappers around OpenSSL or Windows
BCrypt APIs. The actual cryptographic implementations live in those external libraries.
Rewriting crypto in TML would mean implementing AES, RSA, ECDH, X.509, TLS handshake —
a multi-year security-critical project. FFI wrappers stay.

**Windows IOCP (iocp.c)**: IOCP is a Windows-specific kernel subsystem for async I/O.
The interface requires calling `CreateIoCompletionPort`, `GetQueuedCompletionStatusEx`,
and `PostQueuedCompletionStatus` — all Windows API calls that require specific struct
layouts dictated by the Windows kernel. These can be `@extern("c")` wrapped in TML but
the C initialization code (associating handles, setting up overlapped structures) is
complex enough that the C file is the right abstraction layer.

**Async context switching (async.c)**: Coroutine/fiber switching requires saving and
restoring register state, which on x86_64 means inline assembly. C allows inline assembly.
TML would need `lowlevel` blocks for this, and the complexity justifies keeping it in C.

**Stack unwinding (backtrace.c)**: Stack traces on Windows require `StackWalk64` from
`DbgHelp.dll`, which is a COM-style API with complex initialization. On Linux, libunwind.
Both are C library APIs that are much simpler to call from C than from TML.

### 6.4 What IOCP Migration Would Look Like

Even though full IOCP migration is complex, the declaration layer is straightforward:

```tml
// TML declarations for IOCP
@extern("c") func CreateIoCompletionPort(
    file_handle: RawPtr,
    existing: RawPtr,
    key: U64,
    threads: U32
) -> RawPtr

@extern("c") func GetQueuedCompletionStatusEx(
    port: RawPtr,
    entries: mut ref OverlappedEntry,
    count: U32,
    removed: mut ref U32,
    timeout: U32,
    alertable: Bool
) -> Bool
```

The struct layout (`OverlappedEntry`, `OVERLAPPED`) must exactly match Windows kernel
definitions — solvable with TML's `@repr(C)` attribute on structs.

### 6.5 Migration Feasibility by File Group

| Group | LOC | TML Migration | Effort | Priority |
|-------|-----|--------------|--------|----------|
| Pure logic (pool, str_free, utf8, fmt) | ~700 | 100% possible | Low (1-2 weeks) | Medium |
| I/O via syscall FFI (essential.c) | 1,344 | ~60% (rest stays as FFI decls) | Medium (3-4 weeks) | Low |
| Memory allocation (mem.c) | 217 | ~80% (10% stays as FFI decls) | Low (1 week) | Medium |
| OS utilities (os.c, os_process.c) | 1,766 | ~50% (Win32 calls stay as FFI) | Medium | Low |
| Diagnostics (log.c, console.c) | 633 | 100% possible | Low (1 week) | Medium |
| Inspector protocol (inspector.c) | 1,633 | ~80% (WebSocket pure logic) | High (1 month) | Low |
| Crypto (9 files) | ~5,985 | 0% (external library FFI) | N/A | Never |
| Network (net.c, iocp.c, tls.c, dns.c, poll.c) | ~2,884 | ~20% (only init code migratable) | High | Never (for FFI bridge) |
| Concurrency (sync.c, async.c) | 1,603 | ~30% (algorithm logic only) | High | Low |

---

## 7. Recommended Strategy

The strategy is divided into three phases aligned with the existing roadmap.

### 7.1 Short-Term: Pre-compiled .lib Files (Current State)

The current approach (Zig CC compiles `.c` → `.obj`, CMake links) is correct for now.
To make this more robust:

1. **Add a `--clang` build path**: The existing `--clang` flag in `build.bat` lets the
   build system use a system Clang instead of Zig CC. Validate this path in CI so any
   Clang 18+ works as a Zig CC replacement.

2. **Ship pre-compiled `.lib` files**: Compile the C runtime once with Zig CC and commit
   the resulting `tml_runtime.lib` to the repository. Users who don't modify the C runtime
   (which is almost everyone) can build `tml.exe` without any C compiler. Only developers
   who modify `compiler/runtime/` need to recompile the C runtime.

   This is the same approach used by Python (ships pre-built `python3X.lib`) and many
   other projects. The `.lib` files are ~2-3MB total.

3. **Gitignore-exempt the `.lib` files**: Add them to git tracking explicitly (they are
   build artifacts, but small and stable). The Cranelift bridge (already Rust-compiled)
   follows this same pattern.

### 7.2 Medium-Term: Bundle Clang as `tml-cc` (Phase 1b, Linker Roadmap)

This is already planned in the linker roadmap as Phase 1b. TML already links against LLVM.
Adding Clang frontend libraries extends the binary by 30-50MB but:

- Eliminates the Zig dependency completely
- Provides `tml-cc` command with the same quality as `zig cc`
- Enables in-process C compilation (no subprocess overhead)
- Unlocks automatic header parsing via `libclang`

The implementation involves adding Clang library targets to `CMakeLists.txt` and writing a
thin `compiler/src/backend/clang_cc.cpp` wrapper (~300 LOC) that exposes:

```cpp
bool compile_c_file(std::string_view source_path,
                    std::string_view output_path,
                    int optimization_level,
                    std::string_view target_triple);
```

This wrapper is then called during the TML compiler build process itself, replacing the
Zig CC invocation.

### 7.3 Long-Term: Migrate Pure-Logic C to TML (Ongoing with Phase 4)

Phase 4 already established the migration methodology. Continue that process for the
remaining pure-logic C files:

**Priority 1 (High, minimal effort)**:
- `collections/collections.c` (65 LOC): Collection utilities → already mostly duplicated in TML stdlib
- `core/essential_utf8.c` (75 LOC): UTF-8 validation → pure logic, `std::str` covers this
- `memory/str_free.c` (166 LOC): String deallocation → straightforward TML

**Priority 2 (Medium)**:
- `memory/pool.c` (283 LOC): Slab allocator → `core::alloc` could absorb this
- `diagnostics/log.c` (429 LOC): Structured logging → pure TML with template literals
- `diagnostics/console.c` (204 LOC): Console formatting → `std::fmt` handles this
- `lib/std/runtime/glob.c` (702 LOC): File glob → pure pattern matching, no OS API

**Priority 3 (Low, complex)**:
- `diagnostics/inspector.c` (1,633 LOC): WebSocket inspector protocol — complex but mostly pure logic
- `memory/mem.c` (217 LOC): Requires OS memory API declarations in TML first

**Never migrate** (correct as permanent C FFI):
- All crypto files (~5,985 LOC)
- All net files (~2,884 LOC)
- Concurrency: sync.c, async.c (1,603 LOC)
- OS interface: os.c, os_process.c (1,766 LOC)
- LLVM/LLD C++ shims (1,593 LOC)

### 7.4 Decision Matrix

| Strategy | When | Effort | Benefit | Risk |
|----------|------|--------|---------|------|
| Pre-compiled `.lib` files | Now | 1 day | Eliminates Zig for most users | Binary artifacts in git (small) |
| `--clang` build path | Now | 1 week | Fallback for users with Clang installed | Minimal |
| Bundle Clang (tml-cc) | Phase 1b | 3-4 weeks | Full Zig elimination, in-process C compilation | Medium (binary size) |
| Migrate pure-logic C to TML | Phase 4 ongoing | 2-4 months (total) | Fewer C files, shorter critical path | Low |
| Full C runtime in TML | Post-Phase 6 | 1-2 years | True zero-C bootstrap | High (crypto/net reimplementation risky) |
| Embed TCC | Optional | 2 weeks | Bootstrap option, very small | Limited (no SIMD, unoptimized) |

---

## 8. Summary

TML's C runtime contains **~19,700 LOC** across 49 files. Of these:

- **~4,186 LOC (21%)** is pure logic that can be migrated to TML incrementally
- **~14,464 LOC (73%)** is FFI bridge code (crypto, net, OS) that stays as `@extern("c")` targets
- **~1,050 LOC (6%)** is truly irreplaceable (inline assembly, direct OS I/O)

The critical dependency is Zig CC for compilation. The three-phase elimination strategy:

1. **Now**: Ship pre-compiled `.lib` files; validate `--clang` path
2. **Phase 1b**: Bundle Clang as `tml-cc`, eliminating the Zig executable dependency
3. **Ongoing**: Migrate the 4,186 LOC of pure-logic C to TML as part of Phase 4 continuation

The goal is not to eliminate all C — crypto, networking, and OS interface code is correctly
written in C and called via FFI. The goal is to eliminate the *compiler dependency* on Zig
or any external C compiler, while keeping the runtime C as a static pre-compiled artifact
that ships with the TML toolchain.

---

*Related documents: [linker/README.md](../linker/README.md) | [compiler-selfhosting/04-runtime-dependencies.md](../compiler-selfhosting/04-runtime-dependencies.md)*
