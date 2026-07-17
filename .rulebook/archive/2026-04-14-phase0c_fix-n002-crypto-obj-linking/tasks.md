## 1. Diagnosis
- [x] 1.1 Check `scripts/build.bat` for the C runtime compilation step — verify if `compiler/runtime/crypto/*.c` files are compiled to `.obj`
- [x] 1.2 Check the linker command generation in `compiler/src/cli/builder/build.cpp` — trace how C runtime deps are resolved to `.obj` paths
- [x] 1.3 Check `build/debug/deps/` for any existing `.obj` files and determine the expected naming pattern (`*_DTMLHASO.obj`)
- [x] 1.4 Identify whether the issue is: (a) CMake not compiling C runtime files, (b) the `.obj` output path being wrong, or (c) the linker looking in the wrong directory

## 2. Fix
- [x] 2.1 Fix the build pipeline to compile C runtime crypto/tls files to `.obj` in `build/debug/deps/`
- [x] 2.2 Ensure all 9 affected C files produce valid `.obj` outputs: `crypto.c`, `crypto_key.c`, `crypto_x509.c`, `crypto_dh.c`, `crypto_ecdh.c`, `crypto_kdf.c`, `crypto_rsa.c`, `crypto_sign.c`, `net/tls.c`
- [x] 2.3 Compile and run `benchmarks/profile_tml/crypto_bench.tml --stage=parser:cpp` successfully
- [x] 2.4 Compile and run `benchmarks/profile_tml/large_scale_bench.tml --stage=parser:cpp` successfully

## 3. Validation
- [x] 3.1 Run `tml test --suite=core` — no regressions
- [x] 3.2 Run `tml test --suite=std` — no regressions
- [x] 3.3 Verify a simple TML program using `std::crypto::hash` compiles and runs

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 4.1 Update CHANGELOG.md with the fix
- [x] 4.2 Write a test that imports `std::crypto::hash` and computes a SHA-256 hash
- [x] 4.3 Run tests and confirm they pass
- [x] Update or create documentation covering the implementation
- [x] Write tests covering the new functionality
- [x] Verify all tests pass
