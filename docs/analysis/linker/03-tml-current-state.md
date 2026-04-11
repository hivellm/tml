# TML Current Linking Pipeline --- State Analysis

## 1. Full Pipeline Overview

```
Source (.tml)
  → TML compiler (parser, type checker, HIR/MIR lowering)
  → LLVM IR text (std::string, generated in-process)
  → LLVMParseIRInContext  (IR text → LLVMModuleRef)
  → LLVMRunPasses         (optional: O1/O2/O3 optimization)
  → LLVMVerifyModule      (IR validation)
  → LLVMTargetMachineEmitToFile  → temp .obj file on disk
         OR
  → LLVMTargetMachineEmitToMemoryBuffer  → std::vector<uint8_t> in memory
  → lld::lldMain()        (in-process LLD, reads .obj from disk)
  → .exe / .dll / .lib    (output on disk)
```

The normal compilation path writes one `.obj` file per compilation unit to disk,
then passes those file paths to LLD. The in-memory path (`compile_ir_to_buffer`)
already exists in the codebase but is not wired into the default build flow.

---

## 2. LLVM Backend (`compiler/src/backend/llvm_backend.cpp`)

### 2.1 Initialization

Target initialization runs exactly once across all threads via `std::call_once`:

```
LLVMInitializeX86TargetInfo / Target / TargetMC / AsmParser / AsmPrinter
LLVMInitializeAArch64TargetInfo / Target / TargetMC / AsmParser / AsmPrinter
```

Both X86 and AArch64 are always initialized regardless of the current host.
Each `LLVMBackend` instance creates its own `LLVMContextRef` via `LLVMContextCreate`,
making it safe to use multiple backends on separate threads simultaneously.

### 2.2 `compile_ir_to_object()` --- Disk Path

Steps executed in order:

1. `LLVMCreateMemoryBufferWithMemoryRangeCopy` --- wraps the IR `std::string` in an LLVM buffer
2. `LLVMParseIRInContext` --- parses textual LLVM IR into an `LLVMModuleRef`
3. `LLVMGetDefaultTargetTriple` (when no explicit triple is given)
4. `LLVMGetTargetFromTriple` --- looks up the target by triple string
5. `LLVMGetHostCPUName` + `LLVMGetHostCPUFeatures` --- used when cpu/features are empty
6. `LLVMCreateTargetMachine` --- creates the target machine for the resolved target
7. `LLVMCreateTargetDataLayout` + `LLVMSetDataLayout` --- applies data layout to the module
8. `LLVMMDStringInContext2` + `LLVMAddNamedMetadataOperand` --- embeds `!llvm.ident "tml version X.Y.Z"`
9. `LLVMRunPasses("default<O0|O1|O2|O3>")` --- runs the new pass manager (only when opt level > 0)
10. `LLVMVerifyModule` --- runs IR verification; failures are non-fatal warnings
11. `LLVMTargetMachineEmitToFile(..., LLVMObjectFile)` --- writes COFF `.obj` to disk

**Optimization level mapping:**

| `options.optimization_level` | LLVM codegen level | Pass pipeline |
|------------------------------|-------------------|---------------|
| 0 | `LLVMCodeGenLevelNone` | no passes run |
| 1 | `LLVMCodeGenLevelLess` | `default<O1>` |
| 2 | `LLVMCodeGenLevelDefault` | `default<O2>` |
| 3 | `LLVMCodeGenLevelAggressive` | `default<O3>` |

**Windows file locking retry:** When `LLVMTargetMachineEmitToFile` fails with "user-mapped
section" or "being used by another process", the backend retries up to 3 times with
delays of 100ms, 200ms, 300ms (100ms * attempt number). This handles transient Windows
file lock contention during concurrent test suite compilation.

**Error codes:** K001 (IR parse failure), K002 (verify warning), K003 (target machine),
K004 (emit file), K005 (target lookup), K006 (memory buffer), K007 (open IR file),
K008 (IR file not found), K009 (not initialized), K010 (context create), K011 (emit to memory).

### 2.3 `compile_ir_to_buffer()` --- In-Memory Path

Follows the same steps as `compile_ir_to_object()` through target machine creation and
optimization, then diverges at the emit step:

```
LLVMTargetMachineEmitToMemoryBuffer(..., LLVMObjectFile, &error, &obj_buffer)
LLVMGetBufferStart + LLVMGetBufferSize
result.object_data.assign(...)  → std::vector<uint8_t>
```

The resulting `object_data` field on `LLVMCompileResult` holds a complete COFF `.obj`
in memory. **This path is fully implemented and functional.** It is not used by the
default `tml build` flow today, but it is the foundation for Phase 2 of the custom
linker roadmap (in-memory object passing to LLD).

Note: `compile_ir_to_buffer()` does not run `LLVMVerifyModule` or embed `!llvm.ident`
metadata, unlike the disk path.

### 2.4 `compile_ir_file_to_object()` --- File-to-File Helper

Reads an `.ll` file from disk, passes it to `compile_ir_to_object()`, and defaults the
output extension to `.obj` on Windows, `.o` on other platforms. Not used in the main
compilation pipeline; exists for CLI tools that operate on IR files directly.

---

## 3. LLD Integration (`compiler/src/backend/lld_linker.cpp`)

### 3.1 Global State and Thread Safety

LLD uses global mutable state internally (`CommonLinkerContext`). The integration
manages this with two globals:

```cpp
static std::atomic<bool> g_lld_poisoned{false};
static std::mutex g_lld_mutex;
```

All in-process LLD calls are serialized through `g_lld_mutex`. This means concurrent
compilation units cannot link in parallel --- they queue at the mutex.

### 3.2 In-Process Linking (`link_in_process`)

Enabled when `TML_HAS_LLD_EMBEDDED` is defined (set by CMake when LLD headers and
libraries are found). Steps:

1. Check `g_lld_poisoned` --- if true, refuse and return error `[N003]`
2. Acquire `g_lld_mutex` (serializes all LLD calls)
3. Build `std::vector<const char*> argv` from the argument list
4. Capture stdout/stderr via `llvm::raw_string_ostream`
5. Select drivers by platform: `lld::WinLink + lld::coff::link` on Windows;
   `lld::Gnu + lld::elf::link` and `lld::Darwin + lld::macho::link` on Unix
6. Allocate `LldState` on the heap and spawn a detached `std::thread` to call `lld::lldMain()`
7. Poll `state->done` every 100ms until the thread completes or 15 seconds elapse
8. On timeout: set `g_lld_poisoned = true`, return error
9. On `lld_result.canRunAgain == false`: set `g_lld_poisoned = true`, log warning
10. On non-zero exit code: return error `[N002]`

The detached thread pattern exists because `condition_variable::wait_for` fails to
wake on MSVC when `lldMain` deadlocks internally. Polling solves this but prevents
the OS from reclaiming the thread stack until LLD eventually unblocks.

### 3.3 Subprocess Fallback

When `g_lld_poisoned` is true or `options.force_subprocess` is set, the linker falls
back to executing the LLD binary as a subprocess via `std::system()`. The subprocess
path is also used when `TML_HAS_LLD_EMBEDDED` is not defined.

LLD binary search order:
1. `build/llvm/Release/bin/` (local LLVM build)
2. `src/llvm-install/bin/` (from `scripts/build_llvm.bat`)
3. `F:/LLVM/bin`, `C:/Program Files/LLVM/bin`, `C:/LLVM/bin` (fixed paths)
4. `/usr/bin`, `/usr/local/bin`, `/usr/lib/llvm-18/bin`, `/usr/lib/llvm-17/bin` (Unix)
5. All directories from the `PATH` environment variable
6. The directory named by `LLVM_DIR` environment variable (checked first if set)

### 3.4 Windows Linker Arguments (COFF/PE)

Arguments passed to `lld::coff::link` (or `lld-link.exe` subprocess):

| Argument | Value | Condition |
|----------|-------|-----------|
| `argv[0]` | `lld-link` | always (LLD requires it even in-process) |
| `/OUT:` | output path | always |
| `/SUBSYSTEM:` | `console` or custom | when `options.subsystem` is set |
| `/DEBUG` | | when `options.debug_info` |
| `/DLL` | | when output type is SharedLib |
| `/IMPLIB:` | `output.lib` | when SharedLib + `generate_import_lib` |
| `/ENTRY:` | custom or `mainCRTStartup` | always for executables |
| `/LIBPATH:` | each path in `options.library_paths` | one per path |
| `/DEFAULTLIB:libcmt` | | always |
| `/DEFAULTLIB:oldnames` | | always |
| `/WHOLEARCHIVE:` | each `.lib` file | for non-external `.lib` inputs |
| `/NOLOGO` | | always |

Unix-style `-lname` flags in `options.extra_flags` are translated to `/DEFAULTLIB:name`.

External library detection (excludes from `/WHOLEARCHIVE`): paths containing
`x64-windows`, `vcpkg`, `zstd.lib`, `brotli`, or `zlib.lib`.

### 3.5 Unix Linker Arguments (ELF)

Standard `ld.lld` argument form: `-o output`, `-shared`, `--export-dynamic`,
`-e entry_point`, `-L paths`, `-l libraries`, object files, then `-lc`.

### 3.6 Static Library Creation (Subprocess Only)

LLD does not create archives. Static lib output type uses `llvm-ar` (or `lib.exe`
on Windows, `ar` on Unix as fallback) via `std::system()`.

**Error codes:** N001 (subprocess link failed), N002 (in-process link failed),
N003 (LLD poisoned), N004 (not initialized), N005 (no object files), N006 (object
file not found), N007 (output not created), N008 (static lib creation failed).

---

## 4. Zig CC Usage and Build Alternatives

Zig CC is used exclusively to compile TML's own C++ compiler source. It is **not**
involved in linking TML programs at runtime.

### 4.1 Wrapper Scripts

`scripts/zig-cc.bat`:
```
zig cc -target x86_64-windows-msvc %CMD%
```
Strips `-Xlinker /MANIFEST:EMBED` and `-Xlinker /version:0.0` (unsupported by Zig's
linker pass-through when targeting MSVC ABI).

`scripts/zig-cxx.bat`:
```
zig c++ -target x86_64-windows-msvc -nostdlib++ -Wno-unused-command-line-argument %CMD%
```
`-nostdlib++` prevents Zig from linking its bundled libc++; MSVC's C++ stdlib is used
instead. `-Wno-unused-command-line-argument` suppresses the warning that `-nostdlib++`
triggers when combined with `-nostdinc++`.

### 4.2 CMake Toolchain File (`cmake/toolchains/zig.cmake`)

| CMake variable | Value |
|----------------|-------|
| `CMAKE_C_COMPILER` | `scripts/zig-cc.bat` |
| `CMAKE_CXX_COMPILER` | `scripts/zig-cxx.bat` |
| `CMAKE_C_COMPILER_ID` | `Clang` (forced) |
| `CMAKE_C_COMPILER_VERSION` | `20.1.2` (forced) |
| `CMAKE_C_COMPILER_WORKS` | `TRUE` (skips compiler probe) |
| `CMAKE_MSVC_RUNTIME_LIBRARY` | `MultiThreadedDLL` |
| `CMAKE_C_FLAGS_INIT` | `-D_DLL -D_MT -Xclang --dependent-lib=msvcrt -fno-sanitize=undefined` |
| `CMAKE_RC_COMPILER` | `rc` (MSVC resource compiler) |

The dynamic CRT flags (`-D_DLL -D_MT --dependent-lib=msvcrt`) are required to match
the LLVM static libraries, which are built with `/MD`. Mixing `/MD` and `/MT` causes
`LNK2038` CRT mismatch errors at link time.

### 4.3 Build Alternatives

`scripts/build.bat` supports three compilers via auto-detection and flags:

| Compiler | Selection | Notes |
|----------|-----------|-------|
| Zig CC | default (when `zig.exe` found in PATH) | Bundles Clang 20, no VS needed |
| Clang | `--clang` flag | Requires Clang + Ninja in PATH |
| MSVC | `--msvc` flag | Requires `vcvars64` environment |

---

## 5. CMake Build System --- LLVM/LLD Library Linking

### 5.1 LLVM Static Libraries (tml_backend)

When individual static LLVM libraries are found, `tml_backend` links approximately
40 libraries across five groups:

**X86 target (6 libs):**
`LLVMX86CodeGen`, `LLVMX86AsmParser`, `LLVMX86Desc`, `LLVMX86Disassembler`,
`LLVMX86Info`, `LLVMX86TargetMCA`

**AArch64 target (6 libs):**
`LLVMAArch64CodeGen`, `LLVMAArch64AsmParser`, `LLVMAArch64Desc`,
`LLVMAArch64Disassembler`, `LLVMAArch64Info`, `LLVMAArch64Utils`

**Optimization passes (9 libs):**
`LLVMPasses`, `LLVMCoroutines`, `LLVMipo`, `LLVMVectorize`, `LLVMSandboxIR`,
`LLVMAggressiveInstCombine`, `LLVMInstCombine`, `LLVMScalarOpts`,
`LLVMObjCARCOpts`, `LLVMInstrumentation`, `LLVMCFGuard`

**Code generation (5 libs):**
`LLVMCodeGen`, `LLVMCGData`, `LLVMGlobalISel`, `LLVMSelectionDAG`, `LLVMAsmPrinter`

**Debug info (5 libs):**
`LLVMDebugInfoDWARF`, `LLVMDebugInfoDWARFLowLevel`, `LLVMDebugInfoCodeView`,
`LLVMDebugInfoMSF`, `LLVMDebugInfoPDB`

**Frontend and profiling (8 libs):**
`LLVMProfileData`, `LLVMCoverage`, `LLVMFrontendOpenMP`, `LLVMFrontendAtomic`,
`LLVMFrontendOffloading`, `LLVMFrontendDriver`, `LLVMFrontendHLSL`, `LLVMObjectYAML`, `LLVMHipStdPar`

**Core (11 libs):**
`LLVMLinker`, `LLVMTransformUtils`, `LLVMAnalysis`, `LLVMIRReader`, `LLVMIRPrinter`,
`LLVMAsmParser`, `LLVMBitWriter`, `LLVMBitReader`, `LLVMTarget`, `LLVMCodeGenTypes`,
`LLVMCore`, `LLVMRemarks`, `LLVMBitstreamReader`

**Low-level (6 libs):**
`LLVMObject`, `LLVMMCParser`, `LLVMMCDisassembler`, `LLVMMC`, `LLVMBinaryFormat`,
`LLVMTargetParser`, `LLVMTextAPI`, `LLVMSupport`, `LLVMDemangle`

**Windows system libraries:** `ntdll`, `advapi32`, `ole32`, `shell32`, `uuid`

When static libraries are not found, fallback is `LLVM-C` (Windows) or `LLVM` (Unix)
as a single shared library.

### 5.2 LLD Static Libraries (tml_lld)

**LLD flavor libraries (6):**
`lldCOFF`, `lldCommon`, `lldELF`, `lldMachO`, `lldMinGW`, `lldWasm`

**LLD-specific LLVM dependencies (5+):**
`LLVMLTO`, `LLVMDTLTO`, `LLVMOption`, `LLVMLibDriver`, `LLVMPlugins`,
`LLVMWindowsManifest`, `LLVMWindowsDriver`

**Shared deps with tml_backend (reused):**
`LLVMPasses`, `LLVMipo`, `LLVMBitWriter`, `LLVMLinker`, `LLVMTransformUtils`,
`LLVMCodeGen`, `LLVMTarget`, `LLVMAnalysis`, `LLVMObject`, `LLVMCore`,
`LLVMIRReader`, `LLVMAsmParser`, `LLVMBitReader`, `LLVMProfileData`,
`LLVMMC`, `LLVMMCParser`, `LLVMMCDisassembler`, `LLVMBinaryFormat`,
`LLVMDebugInfoDWARF`, `LLVMDebugInfoCodeView`, `LLVMDebugInfoMSF`, `LLVMDebugInfoPDB`,
`LLVMSupport`, `LLVMDemangle`, `LLVMTargetParser`, `LLVMRemarks`

When `TML_HAS_LLD_EMBEDDED=1` is defined (set by the CMakeLists.txt when headers and
libraries are present), the in-process `lld::lldMain()` path is compiled in. Otherwise
only the subprocess fallback is compiled.

### 5.3 Build Token Enforcement

`compiler/CMakeLists.txt` enforces that builds go through the build scripts by checking:

```cmake
if(NOT DEFINED TML_BUILD_TOKEN OR NOT TML_BUILD_TOKEN STREQUAL "tml_script_build_2026")
    message(FATAL_ERROR ...)
endif()
```

`scripts/build.bat` passes `-DTML_BUILD_TOKEN=tml_script_build_2026` to CMake.
Direct `cmake` invocations fail at configure time with an explicit error message.

### 5.4 CRT Mismatch Prevention

The LLVM static libraries are built in Release mode with `/MD`
(`_ITERATOR_DEBUG_LEVEL=0`). A TML Debug build defaults to `/MDd`
(`_ITERATOR_DEBUG_LEVEL=2`), which causes `LNK2038` at link time.

The fix is applied in two places:
1. `cmake/toolchains/zig.cmake` sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL`
   and adds `-D_DLL -D_MT --dependent-lib=msvcrt` to all compile flags.
2. CMakeLists.txt documents that `tml_backend` must use Release CRT regardless of build
   configuration, because it only calls the LLVM C API through opaque handles with no
   STL types crossing the boundary.

---

## 6. Key Discovery: In-Memory Object Path Already Exists

`compile_ir_to_buffer()` is a complete, functional implementation of in-memory
LLVM IR to COFF object compilation. The method:

- Takes IR as a `std::string`
- Runs the full LLVM pipeline (parse, target machine, optional optimization, emit)
- Returns the object bytes as `std::vector<uint8_t>` in `LLVMCompileResult::object_data`
- Uses `LLVMTargetMachineEmitToMemoryBuffer` --- the LLVM C API equivalent of emitting
  to a file but directing output to an `LLVMMemoryBufferRef`

This means Phase 2 of the custom linker roadmap (eliminating the disk round-trip for
object files) is **half complete**. What remains is wiring `object_data` into LLD's
input path, which requires either:

- Writing the buffer to a named pipe or temp file that LLD can open (low effort, partial win)
- Using LLD's `MemoryBuffer` input API if exposed through `lld::coff::link` (higher effort,
  requires LLD internals access beyond the `lldMain` facade)
- Bypassing LLD entirely with a custom COFF writer that accepts in-memory objects (Phase 3)

---

## 7. Performance Baseline

Exact measurements are not yet instrumented in the codebase. Estimates based on
the architecture:

| Operation | Estimated time | Notes |
|-----------|---------------|-------|
| IR parse (`LLVMParseIRInContext`) | 5-20ms | scales with IR size |
| Target machine creation | 1-5ms | one-time per `LLVMBackend` instance |
| O0 emit to file | 10-50ms | varies by CGU size |
| O2/O3 optimization | 50-500ms | varies heavily by IR complexity |
| LLD in-process link (hello world) | 50-150ms | includes mutex acquire |
| LLD subprocess fallback | 200-500ms | process spawn overhead |
| Windows file lock retry delay | 100-300ms | only on contention |
| LLD deadlock timeout | 15,000ms | worst case, poisons LLD state |

Concurrent compilation is parallel at the LLVM IR-to-object stage (each `LLVMBackend`
instance is independent with its own context). Linking is serialized through `g_lld_mutex`,
making the link step a global bottleneck for test suites that compile many binaries.

---

## 8. Capability Matrix

| Feature | Status | Details |
|---------|--------|---------|
| `.exe` output | Working | PE/COFF via LLD-COFF in-process |
| `.dll` output | Working | `/DLL /IMPLIB:` flags to LLD-COFF |
| `.lib` output | Working | Via `llvm-ar` or `lib.exe` subprocess |
| Debug info | Working | `/DEBUG` flag; PDB generated by LLD |
| In-memory IR | Working | IR text always held in `std::string` |
| In-memory `.obj` | Code exists | `compile_ir_to_buffer()` implemented, not wired |
| In-memory link | Not implemented | LLD requires file paths for input |
| Incremental link | Not implemented | Full relink every build |
| Parallel linking | Not implemented | `g_lld_mutex` serializes all LLD calls |
| X86 codegen | Working | Full target init + AsmPrinter |
| AArch64 codegen | Working | Target init done; cross-compilation possible |
| Cross-compile to Linux | Partial | AArch64 target inits; ELF driver present in LLD |
| Cross-compile to macOS | Partial | Mach-O driver present in LLD |
| LLD poisoning recovery | Partial | Falls back to subprocess; subprocess may also fail |
| Deadlock protection | Working | 15-second timeout with detached thread polling |
| Static lib archiving | Subprocess | `llvm-ar` or `lib.exe`; no in-process path |
| JIT (ORC) | Optional | Compiled in when `TML_USE_JIT=ON` and libs found |

---

## 9. Gap Analysis --- What Needs to Change for a Custom Linker

The following gaps exist between the current state and a custom PE/COFF linker:

**Gap 1: Disk round-trip for object files.**
`compile_ir_to_object()` writes `.obj` to disk; `compile_ir_to_buffer()` exists but
is not wired to the link step. LLD's `lldMain` interface only accepts file paths, so
the buffer must be written to disk anyway. A custom linker accepting `std::vector<uint8_t>`
directly eliminates this entirely.

**Gap 2: Serialized linking.**
`g_lld_mutex` ensures only one link runs at a time. Test suites that produce many
binaries (each test suite compiles to its own `.exe`) are bottlenecked here. A custom
linker with no global state could link in parallel.

**Gap 3: LLD global state leaks.**
`canRunAgain=false` permanently poisons in-process LLD for the process lifetime.
Recovery requires subprocess fallback, which adds 200-500ms process spawn overhead.
A clean-room linker has no global state to corrupt.

**Gap 4: No incremental linking.**
Every rebuild relinks the full binary from all object files. For the test runner,
which rebuilds `.exe` files frequently during development, incremental patching of
changed functions (as Zig's linker does) would reduce relink time from ~100ms to ~5ms.

**Gap 5: Static archive creation requires subprocess.**
`llvm-ar` or `lib.exe` is called via `std::system()` for `.lib` output. An in-process
COFF archive writer would close this gap.

**Gap 6: LLD timeout risk.**
The 15-second deadlock timeout is a correctness-vs-availability tradeoff. Timing out
poisons LLD state, forces subprocess fallback, and logs a warning but does not surface
the root cause. A custom linker with deterministic behavior eliminates this risk class.

**Gap 7: Zig CC build dependency.**
Not a runtime gap, but the default build requires `zig.exe` in PATH. The `--clang`
and `--msvc` alternatives exist but are not the documented default. Eliminating the
Zig CC wrapper simplifies CI setup and removes the dependency on Zig's bundled Clang
version matching what TML expects.
