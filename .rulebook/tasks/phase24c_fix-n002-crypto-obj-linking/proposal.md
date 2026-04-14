# Proposal: phase24c_fix-n002-crypto-obj-linking

## Why

Programs that use `std::crypto`, `std::net::tls`, or any module that transitively depends on the C runtime crypto files fail to link. The linker receives `.c` source files instead of pre-compiled `.obj` files, and LLD cannot process raw C source.

**Error reproduced by**:
```
tml run benchmarks/profile_tml/crypto_bench.tml --stage=parser:cpp
tml run benchmarks/profile_tml/large_scale_bench.tml --stage=parser:cpp
```

**Error message**:
```
error: failed to open object crypto_DTMLHASO.obj: FileNotFound
lld-link: error: crypto.c: unknown file type
```

The build pipeline expects pre-compiled `.obj` files at `build/debug/deps/crypto_DTMLHASO.obj` etc., but they don't exist. Instead, the linker receives the raw `.c` source paths (`compiler/runtime/crypto/crypto.c`).

**Benchmark impact**: Blocks 100+ modules: all of `std::crypto` (16 files), `std::net` (16 files), `std::http` (68 files), and the large-scale socket benchmark. This represents ~42% of the standard library.

## What Changes

1. Investigate why the C runtime files (`compiler/runtime/crypto/*.c`, `compiler/runtime/net/tls.c`) are not being compiled to `.obj` during the build process
2. Fix the build pipeline to compile these C files with the system C compiler (Zig CC, MSVC, or Clang) and produce the expected `.obj` files in `build/debug/deps/`
3. Alternatively, fix the linker command generation to invoke the C compiler on-the-fly for `.c` inputs (as a fallback)
4. Verify crypto_bench.tml and large_scale_bench.tml compile and link successfully

## Impact
- Affected specs: std::crypto, std::net, std::http, std::net::tls
- Affected code: `scripts/build.bat` (CMake configuration), `compiler/src/cli/builder/build.cpp` (linker command generation), possibly `compiler/src/codegen/linker.cpp`
- Breaking change: NO
- User benefit: Crypto, TLS, networking, and HTTP modules work in compiled TML programs
