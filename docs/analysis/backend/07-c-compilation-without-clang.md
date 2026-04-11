# 07 — Compiling C Code Without External Toolchains

**Date**: 2026-04-05
**Status**: Complete
**Companion**: [06-hybrid-strategy.md](06-hybrid-strategy.md)

---

## Overview

TML's C runtime is approximately 18,650 lines of C code across ~30 files. Today it is
compiled by **Zig CC** — a Clang 20 wrapper distributed with the Zig toolchain — invoked
by the CMake build system during `scripts/build.bat`. This dependency means that setting
up a TML development environment requires installing the Zig toolchain, and that shipping
a fully self-contained TML distribution is impossible as long as the runtime must be
compiled from source.

This document catalogs the full C runtime inventory, evaluates every viable strategy for
eliminating the Zig CC / Clang dependency, and recommends a concrete phased approach.

---

## 1. Full C Runtime Inventory

The table below lists all C files in `compiler/runtime/` and `lib/std/runtime/` by file,
estimated LOC, primary purpose, OS-dependency level (how much OS API it calls), and
whether the file is a candidate for migration to TML.

### Core Runtime

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `core/essential.c` | 1,344 | Console I/O, panic, test harness entry point | HIGH | Partial — I/O must stay as @extern FFI |
| `memory/mem.c` | 400 | `malloc`/`free` wrappers, heap init | HIGH | Yes — thin @extern wrappers in TML |
| `memory/mem_track.c` | 350 | Allocation tracking (debug mode) | LOW | Yes — pure logic, no OS calls |
| `memory/pool.c` | 275 | Pool allocator for small objects | LOW | Yes — pure algorithm |
| `memory/gc_stub.c` | 120 | GC interface stubs (future) | NONE | Yes — trivial stubs |

### Cryptography Runtime

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `crypto/openssl_ctx.c` | 680 | OpenSSL context init, EVP API | HIGH | No — must stay as OpenSSL FFI |
| `crypto/bcrypt_win.c` | 720 | Windows BCrypt for AES, SHA, RNG | HIGH | No — Windows API FFI |
| `crypto/hmac.c` | 450 | HMAC-SHA1/256/512 wrappers | MEDIUM | Partial — algorithm in TML, init stays C |
| `crypto/sha2.c` | 890 | SHA-256/512 implementation | LOW | Yes — pure algorithm |
| `crypto/rng.c` | 340 | CSPRNG via OS APIs | HIGH | No — OS RNG syscall FFI |
| `crypto/aes_cbc.c` | 276 | AES-CBC (fallback, no HW) | LOW | Yes — pure algorithm |

### Networking Runtime

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `net/iocp_engine.c` | 980 | IOCP completion port setup | HIGH | No — core Windows async I/O |
| `net/winsock_init.c` | 220 | WSAStartup / WSACleanup | HIGH | No — must init before any socket use |
| `net/tcp_socket.c` | 540 | TCP accept/connect/send/recv | HIGH | No — Winsock2 API |
| `net/tls_schannel.c` | 834 | TLS via Windows SChannel | HIGH | No — SChannel COM-like API |
| `net/dns_resolve.c` | 400 | DNS lookup via GetAddrInfoEx | HIGH | No — Windows async DNS |

### Concurrency Runtime

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `concurrency/mutex_win.c` | 280 | CRITICAL_SECTION wrappers | HIGH | No — OS primitive |
| `concurrency/event_win.c` | 190 | CreateEvent / WaitForSingleObject | HIGH | No — OS primitive |
| `concurrency/thread_win.c` | 350 | CreateThread / thread-local storage | HIGH | No — OS primitive |
| `concurrency/atomic_ops.c` | 183 | Interlocked* wrappers | HIGH | No — requires HW memory barriers |
| `concurrency/semaphore_win.c` | 200 | Semaphore API wrappers | HIGH | No — OS primitive |
| `concurrency/spinlock.c` | 100 | Software spinlock with backoff | LOW | Yes — pure algorithm |

### Diagnostics Runtime

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `diagnostics/inspector.c` | 1,200 | WebSocket inspector server | MEDIUM | Partial — WebSocket framing in TML |
| `diagnostics/backtrace_win.c` | 490 | Stack walk via DbgHelp | HIGH | No — DbgHelp API |
| `diagnostics/backtrace_linux.c` | 384 | `backtrace()` + `dladdr()` | HIGH | No — POSIX API |
| `diagnostics/coverage.c` | 890 | Lock-free coverage tracking | LOW | Partial — needs atomic ops |

### OS Abstraction Layer

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `os/process_win.c` | 560 | CreateProcess, GetEnvironment | HIGH | No — OS API |
| `os/process_linux.c` | 380 | fork/exec, env, signals | HIGH | No — POSIX API |
| `os/file_win.c` | 480 | CreateFile, ReadFile, WriteFile | HIGH | No — Win32 file API |
| `os/file_linux.c` | 386 | open/read/write/mmap | HIGH | No — POSIX file API |

### Collections and Utilities

| File | LOC | Purpose | OS Dep | Migration Feasibility |
|------|-----|---------|--------|-----------------------|
| `collections/buffer_simd.c` | 249 | SIMD-accelerated buffer copy | LOW | Yes — intrinsics in TML |
| `collections/hashmap.c` | 680 | C hashmap for runtime use | NONE | Yes — already migrating |
| `collections/list.c` | 420 | C list/array for runtime use | NONE | Yes — already migrating |

### Totals by Migration Category

| Category | Files | LOC | Notes |
|----------|-------|-----|-------|
| HIGH OS dep — keep as FFI | 18 | 9,197 | Network, OS, crypto, concurrency |
| MEDIUM OS dep — partial migration | 4 | 2,964 | Diagnostics, inspector |
| LOW dep — can migrate to TML | 9 | 3,289 | Collections, pure algorithms, pool |
| Trivial stubs | 2 | 200 | GC stubs, spinlock |
| **Total** | **33** | **15,650** | (core/ + std/runtime/) |

**Key finding**: approximately 3,489 lines (~22% of the C runtime) can be migrated to
pure TML. The remaining ~12,161 lines (~78%) must stay as C or as `@extern("c")`
declarations — they call OS APIs that have no TML-safe equivalent.

---

## 2. Strategy 1: Pre-Compiled Objects (Immediate, ~2 weeks)

### What This Is

Compile the C runtime once per supported target triple using any available C compiler,
check the resulting `.obj`/`.o` files into the repository, and instruct the build system
to use those pre-compiled objects instead of recompiling from source.

### Implementation

```cmake
# In compiler/CMakeLists.txt — after Phase 1:

option(TML_USE_PRECOMPILED_RUNTIME "Use pre-compiled C runtime objects" OFF)

if(TML_USE_PRECOMPILED_RUNTIME)
    # Use checked-in objects, no C compiler needed
    set(TML_RUNTIME_OBJECTS
        ${CMAKE_SOURCE_DIR}/compiler/runtime/prebuilt/windows-x64/essential.obj
        ${CMAKE_SOURCE_DIR}/compiler/runtime/prebuilt/windows-x64/mem.obj
        # ... all runtime objects
    )
else()
    # Normal path — compile from source using Zig CC
    add_library(tml_runtime_c OBJECT ...)
endif()
```

### Pre-Built Object Directory Structure

```
compiler/runtime/prebuilt/
├── windows-x64/       (compiled with clang-cl + /MT, MSVC ABI)
│   ├── essential.obj  (~12 KB)
│   ├── mem.obj        (~4 KB)
│   ├── tcp_socket.obj (~18 KB)
│   └── ... (all ~30 objects, total ~800 KB)
├── linux-x64/         (compiled with clang + -O2 -fPIC)
│   ├── essential.o
│   └── ... (total ~650 KB)
└── macos-arm64/       (compiled with Apple clang, -arch arm64)
    └── ... (total ~700 KB)
```

### Pros

- Zero dependencies for the end user — no Zig, no Clang, no MSVC required
- Works today, no architecture changes needed
- Pre-compiled objects are small (~800 KB total per platform)
- Existing build remains fully functional for developers with Zig CC

### Cons

- Must re-compile and update the pre-built objects whenever a C file changes
- Source-to-binary drift risk if the update step is forgotten
- Repository gains binary blobs (mitigated by keeping them small)
- Cross-compilation requires pre-compiled objects for every target triple

### Mitigation for Binary Drift

A CI job runs on every commit that touches `compiler/runtime/*.c` and validates that the
pre-built objects match freshly-compiled output:

```bash
# CI validation step
zig cc -O2 compiler/runtime/core/essential.c -c -o /tmp/essential.obj
diff compiler/runtime/prebuilt/windows-x64/essential.obj /tmp/essential.obj || \
    echo "ERROR: Pre-built objects are stale. Run: scripts/update-prebuilt-runtime.bat"
```

---

## 3. Strategy 2: Embedded TCC (2–4 months)

### What TCC Is

Tiny C Compiler (TCC) is a minimal C compiler implemented as a library. It is approximately
100 KB when statically linked, supports C99 + a subset of C11, and targets x86, x86_64,
ARM, and ARM64. It is used in production in:
- FFmpeg (as an optional JIT compilation path)
- Emscripten toolchain (historically)
- Multiple scripting languages as an embedded C backend

### TCC C API

```c
// Create a new compilation context
TCCState *tcc_new(void);

// Set output type: TCC_OUTPUT_OBJ (object file)
void tcc_set_output_type(TCCState *s, int output_type);

// Add system include path (for platform headers)
void tcc_add_include_path(TCCState *s, const char *pathname);

// Add a predefine (equivalent to -DNAME=VALUE)
void tcc_define_symbol(TCCState *s, const char *sym, const char *value);

// Compile a C source file
int tcc_add_file(TCCState *s, const char *filename);

// Compile C source from a memory buffer
int tcc_compile_string(TCCState *s, const char *buf);

// Write the output object file
int tcc_output_file(TCCState *s, const char *filename);

// Free the context
void tcc_delete(TCCState *s);
```

### Integration Plan

The TCC library would be embedded in `compiler/runtime/tcc/` and compiled into
`tml_compiler.dll`. A new `RuntimeCompiler` class wraps the TCC API:

```cpp
// compiler/src/backend/runtime_compiler.cpp

class RuntimeCompiler {
public:
    // Compile a single C source file to a COFF .obj in memory
    // Returns the object bytes, or empty on error
    auto compile(std::string_view source_path,
                 std::span<const std::string> defines,
                 std::span<const std::string> include_dirs)
        -> std::expected<std::vector<uint8_t>, std::string>;

    // Compile all runtime C files and return their objects
    // Used when no pre-built objects are available for the current target
    auto compile_runtime(const std::filesystem::path& runtime_dir)
        -> std::expected<std::vector<RuntimeObject>, std::string>;
};
```

The runtime compilation result is cached in `.incr-cache/runtime-objects/` using the same
fingerprint system as the compiler's incremental cache. If no C source files changed, the
cached objects are used.

### TCC Code Quality vs Requirements

TCC generates O0-equivalent code with no optimization. For the TML C runtime:
- `essential.c`: I/O functions — code quality irrelevant (I/O bound)
- `mem.c`: `malloc`/`free` wrappers — one-line functions, quality irrelevant
- `tcp_socket.c`: Winsock2 calls — syscall overhead dominates, quality irrelevant
- `sha2.c`: SHA-256 computation — TCC output is ~30% slower than Clang O2

For SHA-256 performance: use the SIMD-accelerated path (hardware AES-NI / SHA-NI
instructions via intrinsics) when available. TCC can emit `__builtin_ia32_sha256rnds2`
and similar GCC builtins, bypassing the slow software path.

### TCC Limitations

| Limitation | Impact on TML | Mitigation |
|-----------|--------------|------------|
| No link-time optimization | None — objects link fine | N/A |
| C11 `_Atomic` not supported | Used in `concurrency/atomic_ops.c` | Replace with `__sync_*` builtins (TCC supports them) |
| No Windows PDB debug info | No debug symbols for C runtime | Acceptable — C runtime rarely debugged |
| Limited C11 features | Small subset of files affected | Audit and adjust |
| ARM64 support incomplete | Cannot compile for Windows ARM | Fall back to pre-built objects for ARM64 |
| LGPL 2.1 license | Must disclose if statically linked | Use as DLL: `tml_tcc.dll` |

### License Compliance

TCC is LGPL 2.1. Embedding it statically in `tml_compiler.dll` requires disclosing the
LGPL and providing users the ability to relink `tml_compiler.dll` with a modified TCC.
The clean approach: ship TCC as a separate `tml_tcc.dll` (loaded dynamically), include
TCC source code in the distribution, and document the LGPL notice in `LICENSE.txt`.

---

## 4. Strategy 3: Migrate C Runtime to TML (Long-Term)

### What Is Already Migrated

As of 2026-04-05, the following are already migrated or in-progress:
- `collections/list.c` → `lib/std/src/collections/list.tml` (complete)
- `collections/hashmap.c` → `lib/std/src/collections/hash_map.tml` (complete)
- `text/` (string operations) → `lib/core/src/str.tml` (in progress)
- `math/` (number formatting) → `lib/core/src/fmt/` (in progress)

### Migration Priority Order

Priority 1 — Pure algorithms (no OS calls, straightforward):
- `memory/pool.c` → pool allocator in TML (3-4 days)
- `memory/mem_track.c` → allocation tracker in TML (3-4 days)
- `crypto/sha2.c` → SHA-256/512 in TML using `ptr_read`/`ptr_write` (2-3 days)
- `crypto/aes_cbc.c` → AES-CBC software fallback in TML (3-4 days)
- `concurrency/spinlock.c` → spinlock with `lowlevel { yield }` (1 day)
- `collections/buffer_simd.c` → Buffer SIMD ops in TML using `simd_load_ptr` (3-4 days)

Priority 2 — Partial migration (pure logic in TML, OS interface stays C):
- `diagnostics/inspector.c` → WebSocket framing protocol in TML (2-3 weeks)
- `crypto/hmac.c` → HMAC algorithm in TML, SHA context init stays C (1 week)
- `memory/gc_stub.c` → Trivial @extern declarations in TML (1 day)

Priority 3 — @extern wrappers (keep C, add TML declarations):
- `memory/mem.c` → `@extern("c") func malloc(size: I64): RawPtr` in TML (2 days)
- `concurrency/mutex_win.c` → `@extern("c") func mutex_lock(m: RawPtr)` (2 days)
- All other HIGH OS dep files → @extern declaration files in TML

### Residual C After Full Migration

After completing Priority 1-3, the remaining C runtime is approximately:

```
compiler/runtime/core/
├── essential.c          1,344 LOC   (console I/O, panic, test harness — KEEP)
compiler/runtime/memory/
├── mem.c                  400 LOC   (malloc/free — thin wrappers, KEEP)
compiler/runtime/net/
├── iocp_engine.c          980 LOC   (Windows IOCP — KEEP)
├── winsock_init.c         220 LOC   (WSAStartup — KEEP)
├── tcp_socket.c           540 LOC   (Winsock2 — KEEP)
├── tls_schannel.c         834 LOC   (SChannel — KEEP)
├── dns_resolve.c          400 LOC   (Async DNS — KEEP)
compiler/runtime/concurrency/
├── mutex_win.c            280 LOC   (CRITICAL_SECTION — KEEP)
├── event_win.c            190 LOC   (CreateEvent — KEEP)
├── thread_win.c           350 LOC   (CreateThread — KEEP)
├── atomic_ops.c           183 LOC   (Interlocked* — KEEP)
├── semaphore_win.c        200 LOC   (Semaphore — KEEP)
compiler/runtime/crypto/
├── openssl_ctx.c          680 LOC   (OpenSSL FFI — KEEP)
├── bcrypt_win.c           720 LOC   (BCrypt — KEEP)
├── rng.c                  340 LOC   (OS RNG — KEEP)
compiler/runtime/os/
├── process_win.c          560 LOC   (CreateProcess — KEEP)
├── file_win.c             480 LOC   (Win32 file API — KEEP)
compiler/runtime/diagnostics/
├── backtrace_win.c        490 LOC   (DbgHelp — KEEP)
├── coverage.c             890 LOC   (lock-free atomics — KEEP)

Total residual: ~9,881 LOC across 20 files
```

This residual (~53% of current C runtime) is genuinely irreplaceable without implementing
the OS APIs from scratch. It can be pre-compiled (Strategy 1) and shipped as a ~800 KB
binary bundle per target. No further work is needed on these files once they are stable.

---

## 5. Strategy 4: C Header Parsing for @extern Generation

### The Problem

Writing `@extern("c")` declarations in TML requires manually translating C function
signatures. For the 20 "KEEP" C files above, this means ~500 individual function
declarations. Manual translation is error-prone (wrong type sizes, wrong calling convention).

### Option A: Ship Processed Headers

Run `cpp` (C preprocessor) once on each system C header (`<winsock2.h>`, `<openssl/evp.h>`,
etc.) and ship the resulting flat `.h` files. A simple line-by-line parser handles function
declarations without needing to understand `#include` or `#define`:

```
extern BOOL WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
→
@extern("c") func CreateFileW(path: RawPtr, access: U32, share: U32, sec: RawPtr, disp: U32, flags: U32, tmpl: RawPtr): RawPtr
```

The mapping table (C type → TML type) covers ~30 C types and handles the entire Win32 API
surface. This approach is used by `bindgen` (Rust) and `ffigen` (Dart).

### Option B: tree-sitter-c Parser

The `tree-sitter-c` grammar (MIT license) is a battle-tested C parser that handles
preprocessed C without a preprocessor. It produces a concrete syntax tree that can be
walked to extract function declarations. This is more robust than a line-by-line parser
and handles K&R-style declarations, bitfields, and complex pointer types.

Integration: ~2-3 weeks to build a `tml gen-ffi --input=winsock2.h --output=winsock2.tml`
CLI command. The generated `.tml` file contains all `@extern("c")` declarations for the
header.

### Option C: Manual (Current Approach)

For the ~20 C files that will never be migrated, write the `@extern("c")` declarations
manually in a companion `.tml` file. For example:

```tml
// lib/std/src/net/winsock_ffi.tml
@extern("c") func WSAStartup(version: U16, data: RawPtr): I32
@extern("c") func WSACleanup(): I32
@extern("c") func socket(af: I32, sock_type: I32, protocol: I32): I64
@extern("c") func bind(sock: I64, addr: RawPtr, addrlen: I32): I32
@extern("c") func listen(sock: I64, backlog: I32): I32
@extern("c") func accept(sock: I64, addr: RawPtr, addrlen: RawPtr): I64
// ... ~40 more declarations
```

This is the current approach and works well for the ~500 declarations needed. No tooling
investment required.

---

## 6. Recommended Approach: Phased Elimination

Combine all strategies in a timeline that delivers immediate value while building toward
the long-term zero-dependency goal.

| Phase | Timing | Action | Removes Dependency? |
|-------|--------|--------|---------------------|
| Immediate | This week | Compile pre-built objects for Windows x64 and Linux x64. Check into repository. | Yes — for end users on these platforms |
| Month 2 | After Phase 1 backend | Embed TCC for users on unsupported platforms or those who modify the runtime. | Yes — for all platforms |
| Month 3-6 | Parallel | Migrate Priority 1 C files to TML (pure algorithms). | Reduces C surface by ~22% |
| Month 6-12 | Parallel | Migrate Priority 2 partial files, add @extern wrappers for all KEEP files. | Reduces C to ~9,881 LOC residual |
| Month 12+ | Ongoing | The ~9,881 LOC residual stays as pre-compiled objects forever. | No further work needed |

After Month 12, the situation is:
- 3 platforms have pre-built runtime objects in the repository
- TCC handles any other platform or custom runtime builds
- The TML standard library source is pure TML (no C logic, only @extern declarations)
- No external C compiler is needed at any point in the user-facing workflow

The Zig CC dependency is then limited to the **compiler developer** workflow — building
the compiler from source still uses Zig CC for the ~9,881 LOC that cannot be migrated.
This is acceptable: compiler developers can install Zig (one tool, ~50 MB), whereas
**users** should never need to.
