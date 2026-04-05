# LLD vs Zig Linker vs mold vs Custom Approach --- Comparison

**Date**: 2026-04-05
**Scope**: Linker strategy for TML --- performance, correctness, integration overhead
**Reference**: TML current integration in `compiler/src/backend/lld_linker.cpp`

---

## 1. LLD Architecture Overview

LLD is LLVM's production linker, developed since 2014 and shipped with every LLVM release since
15.0. It is the default linker for Android NDK, Fuchsia, ChromeOS, and most Linux distributions
that use Clang as their compiler.

LLD is organized around a **driver dispatch model**. A single `lld::lldMain()` entry point
examines `argv[0]` (or the leading argument) and routes to one of five independent driver
implementations:

| Driver | Format | Platform |
|--------|--------|----------|
| `lld::coff::link` | PE/COFF | Windows (`.exe`, `.dll`) |
| `lld::elf::link` | ELF | Linux, FreeBSD, Fuchsia |
| `lld::macho::link` | Mach-O | macOS, iOS |
| `lld::mingw::link` | PE/COFF (GNU ABI) | MinGW cross-compilation |
| `lld::wasm::link` | WebAssembly | WASM targets |

These five drivers share almost no code. Each is a self-contained linker. The LLD repository
is approximately 150,000 lines of C++ total across all drivers.

**Critical architectural constraint**: LLD uses a `CommonLinkerContext` stored in a global
thread-local pointer. This global state is initialized on first use and torn down at link
completion. The teardown is not always clean --- some LLVM subsystems (MemoryBufferRef, global
string internment tables) leave residual state. As a result, `lld::lldMain()` returns a
`lld::Result` with a boolean field `canRunAgain`. When `canRunAgain == false`, subsequent
in-process calls to LLD on the same driver will access freed or corrupted state and crash.

---

## 2. LLD-COFF Internals

Understanding what LLD-COFF does informs both TML's current integration and the gap that a
custom linker must close.

### Input Processing

LLD-COFF reads four kinds of input files:

- **ObjFile** --- COFF `.obj` from a compiler. Contains sections, symbols, and relocations.
- **ArchiveFile** --- `.lib` or `.a` static archive. Members extracted on-demand when their
  symbols are referenced.
- **BitcodeFile** --- LLVM bitcode `.bc`. LLD calls LLVM LTO to produce object code.
- **ImportFile** --- synthesized from `.lib` import stubs for DLL imports.

### Symbol Resolution

Symbol resolution is a global hash table (`SymbolTable`) keyed by decorated name. Symbols
exist in three states:

1. **Undefined** --- referenced but not yet provided
2. **Lazy** --- available in an archive but not yet loaded
3. **Defined** --- fully resolved to a section + offset

Resolution order is deterministic: command-line objects first, then libraries in order. COMDAT
deduplication happens during resolution --- when two `Defined` symbols with the same COMDAT key
both claim to be the canonical definition, LLD keeps the first and discards the second.

### PE File Construction

After resolution, the Writer generates the final PE binary in memory then flushes to disk:

```
DOS header (64 bytes)
PE signature ("PE\0\0")
COFF header (20 bytes) --- Machine, NumberOfSections, TimeDateStamp
Optional header (224 bytes) --- ImageBase, SectionAlignment, FileAlignment, ...
Section table --- one entry per output section
.text  --- merged from all input .text sections, sorted by address
.rdata --- read-only data: string literals, vtables, debug info references
.data  --- initialized read-write data
.bss   --- zero-initialized (size only, no file bytes)
.reloc --- base relocations for ASLR (32-bit absolute addresses that need fixup)
Import directory --- DLL names, thunks, IAT (Import Address Table)
Export directory --- exported symbol names and ordinals (for DLLs)
Debug directory --- pointer to embedded PDB path
TLS directory --- thread-local storage callbacks and raw data
```

Section layout uses a two-pass algorithm: the first pass computes virtual addresses (RVAs),
the second pass applies all relocations now that addresses are known. Parallelism exists at the
section level --- sections can be processed concurrently once their addresses are fixed.

---

## 3. LLD Performance Profile

LLD's benchmark position versus other linkers (2024 measurements, linking a ~100 MB binary,
x86-64 Linux unless noted):

| Linker | Time | vs LLD |
|--------|------|--------|
| GNU ld 2.42 | 12.4 s | 5.2x slower |
| gold 1.16 | 4.8 s | 2.0x slower |
| LLD 18 | 2.4 s | baseline |
| mold 2.4 | 0.7 s | 3.4x faster |
| Zig linker (ELF) | ~1.5 s | ~1.6x faster |

For PE/COFF on Windows (linking a 30 MB `.exe`):

| Linker | Time | vs LLD |
|--------|------|--------|
| MSVC link.exe 17.9 | 1.8 s | 3.6x slower |
| LLD-COFF 18 | 0.5 s | baseline |
| Zig linker (COFF) | ~0.4 s | ~1.25x faster |

Source: Rui Ueyama's 2024 mold benchmarks, LLVM blog (LLVM 18 release), Zig 0.13 release
notes. Small binary linking (hello world, ~100 KB) is dominated by process startup and I/O;
all linkers complete in under 50 ms at that scale.

**Where LLD parallelism helps**: section merging and relocation processing are parallelized
with LLVM thread pools. Symbol resolution is mostly serial because the hash table is shared.
Speedup from parallelism is approximately 1.5-2x on 8-core machines for large binaries;
single-threaded performance dominates for small TML programs.

**Memory**: LLD's peak RSS during linking is lower than link.exe for equivalent inputs ---
link.exe keeps its full PDB state in memory, while LLD produces a smaller `.pdb` by default.

---

## 4. LLD Limitations That Drive Custom Linker Work

### 4.1 No Incremental Linking

LLD performs a full relink every time, consuming all input `.obj` files. For a 100-function
TML program where 1 function changed, LLD re-reads and re-processes all 100 objects. There is
no facility to patch only the changed sections.

This is architectural: LLD's section layout algorithm assigns virtual addresses in a single
pass. Patching would require all addresses to be stable across builds, which LLD does not
guarantee.

### 4.2 Global State — Not Re-entrant

As described above, `CommonLinkerContext` uses global state. LLD cannot be called from two
threads simultaneously, and calling it a second time after a `canRunAgain=false` result is
undefined behavior in practice (use-after-free or double-free in LLVM's global allocators).

For a compiler that wants to run multiple parallel link operations (e.g., linking test suite
executables concurrently), LLD's lack of re-entrancy is a hard blocker.

### 4.3 No In-Memory Object Support

LLD reads all input from the filesystem via `MemoryBuffer::getFile()`. There is a
`MemoryBuffer::getMemBuffer()` path for in-memory data, but it requires the caller to keep the
buffer alive for the entire link --- and the API is not well-tested for the COFF driver.

TML's compiler generates `.obj` files by writing LLVM IR text, parsing it with
`LLVMParseIRInContext`, then emitting to disk via `LLVMTargetMachineEmitToFile`. The
filesystem round-trip (write `.obj` → LLD reads `.obj`) is wasted I/O. A custom linker that
accepts `llvm::Module*` or raw COFF bytes in memory would eliminate this.

### 4.4 Static Library Creation Is Separate

LLD has no archiver. Creating `.lib` files requires a subprocess to `llvm-ar` or `lib.exe`.
TML's `lld_linker.cpp` implements this as a separate code path using `build_static_lib_command()`.

### 4.5 Binary Size and Link-Time Overhead

LLD links against approximately 40 LLVM libraries. In TML's monolithic build (`tml.exe`,
~100 MB), LLD accounts for a significant fraction of binary size. A purpose-built custom linker
for TML-sized programs (typically 50-2,000 functions) would be far smaller.

---

## 5. TML's Current LLD Integration

TML embeds LLD directly via the `TML_HAS_LLD_EMBEDDED` compile flag, which is set in all
production builds. The integration in `compiler/src/backend/lld_linker.cpp` implements
several layers of safety around LLD's global-state problem.

### 5.1 Initialization and Discovery

`LLDLinker::initialize()` searches for the LLD executable and `llvm-ar` in a priority-ordered
list: local build output, local LLVM install, system paths, `LLVM_DIR` environment variable,
and `PATH`. The executable is used only as a subprocess fallback; in-process linking does not
require the executable to exist.

### 5.2 Argument Construction

Two argument builders exist:

- `build_windows_args()` --- constructs `lld-link` style arguments for PE/COFF:
  - `/OUT:` output path
  - `/SUBSYSTEM:console` (or the specified subsystem)
  - `/ENTRY:mainCRTStartup` (default) or custom entry point
  - `/LIBPATH:` entries for each library search directory
  - `/DEFAULTLIB:libcmt` and `/DEFAULTLIB:oldnames` for Windows CRT
  - `/WHOLEARCHIVE:` for TML-produced `.lib` files (ensures all object members are included)
  - `/DLL` and `/IMPLIB:` for shared library output
  - `/DEBUG` for debug info
  - `/NOLOGO`

- `build_unix_args()` --- constructs `ld.lld` style arguments for ELF:
  - `-o` output, `-shared` for shared libs, `-e` entry point
  - `-L` library paths, `-l` libraries, `-lc` C library

Unix-style `-l` flags in `extra_flags` are translated to `/DEFAULTLIB:` when building Windows
arguments.

### 5.3 In-Process Execution with Poisoning Detection

```
link() called
  │
  ├─ g_lld_poisoned? → return error (no attempt)
  │
  ├─ acquire g_lld_mutex  (serialize — LLD not re-entrant)
  │
  ├─ spawn detached thread → lld::lldMain(argv, stdout_os, stderr_os, drivers)
  │
  ├─ poll state->done every 100ms for up to 15 seconds
  │    timeout → set g_lld_poisoned = true, return error
  │
  ├─ lld_result.canRunAgain == false → set g_lld_poisoned = true, warn
  │
  ├─ retCode != 0 → return error with stderr
  │
  └─ success → verify output file exists, extract import lib path
```

The detached thread pattern is used specifically because `std::condition_variable::wait_for`
fails to wake on MSVC when `lldMain` deadlocks internally. Polling at 100 ms intervals catches
the 15-second deadline reliably.

The `LldState` struct is heap-allocated and shared via `std::shared_ptr` so that if the
timeout fires and `link()` returns, the background thread can continue running safely until
LLD finishes (or the process exits). The thread is detached rather than joined to avoid
blocking the caller on timeout.

### 5.4 Subprocess Fallback

When `g_lld_poisoned` is true, the in-process path is skipped. If `lld_path_` was discovered
during initialization and the file still exists, TML falls back to spawning a subprocess:
the `args` vector is adjusted to use the real executable path and executed via `std::system()`.

Static library creation always uses the subprocess path (via `llvm-ar` or `lib.exe`), because
LLD has no archiver API.

### 5.5 Driver Registration

On Windows, only the COFF driver is registered:

```cpp
std::vector<lld::DriverDef> drivers = {
    {lld::WinLink, &lld::coff::link},
};
```

On non-Windows, both ELF and Mach-O are registered:

```cpp
std::vector<lld::DriverDef> drivers = {
    {lld::Gnu, &lld::elf::link},
    {lld::Darwin, &lld::macho::link},
};
```

The MinGW and Wasm drivers are compiled in (`LLD_HAS_DRIVER(mingw)`, `LLD_HAS_DRIVER(wasm)`)
but not included in the runtime dispatch vector --- they are available if TML adds them.

---

## 6. mold: The Speed Champion

mold was created by Rui Ueyama, who also created LLD. It is a ground-up redesign focused on
link speed for ELF targets.

### 6.1 Architecture

mold's core insight is that the bottleneck in modern linkers is not the linking algorithm but
the sequential processing of input. LLD processes symbols and sections mostly serially. mold
parallelizes every phase:

| Phase | LLD threading | mold threading |
|-------|--------------|----------------|
| Input file parsing | Serial | Parallel per file |
| Symbol resolution | Serial (global hash table) | Parallel with per-shard locks |
| Section merging | Parallel per section | Parallel per section |
| Relocation application | Parallel per section | Parallel per section |
| Output file creation | Serial (writev) | Parallel with memory-mapped output |

For the output file, mold memory-maps the output at the start with the computed final size,
then writes sections in parallel from multiple threads. This eliminates the intermediate
buffering step that LLD uses.

mold also uses O(1) algorithms where LLD uses O(n log n). COMDAT deduplication, for example,
uses a hash set in mold rather than a sorted vector comparison.

### 6.2 Performance Data

mold v2.4 benchmark (linking Chromium on Linux, 4-core machine, 2024):

| Linker | Wall time | Peak RSS |
|--------|-----------|---------|
| GNU ld 2.42 | 70 s | 7.2 GB |
| gold 1.16 | 27 s | 4.4 GB |
| LLD 18 | 12 s | 3.1 GB |
| mold 2.4 | 2.7 s | 2.7 GB |

For a medium Rust project (~5 MB output, Linux):

| Linker | Time |
|--------|------|
| LLD 18 | 0.32 s |
| mold 2.4 | 0.11 s |

### 6.3 Limitations

mold is ELF-only in the open-source version. The commercial `sold` product adds Mach-O
support; PE/COFF is not supported in any version. For TML on Windows (primary platform),
mold is not applicable.

The key lessons from mold for a custom TML linker:

1. **Memory-map the output file before writing** --- avoid buffering
2. **Shard the symbol table** --- use N independent hash tables, one per name-hash shard
3. **Parallelize input parsing** --- each `.obj` file is independent
4. **Compute sizes before layout** --- enables parallel section fill

---

## 7. Zig Self-Hosted Linker

Zig replaced its dependency on system linkers with a self-hosted linker written in Zig,
introduced incrementally from Zig 0.10 and production-ready from Zig 0.12.

### 7.1 Architecture

The Zig linker handles ELF, PE/COFF, Mach-O, WebAssembly, and Plan 9 a.out from a single
unified codebase (`src/link/` in the Zig compiler). Unlike LLD's separate drivers, Zig's
backends share a common `File` interface that abstracts symbol and section operations.

The central innovation is **incremental linking**. Zig allocates output sections with
extra padding (slack space). When a function grows, Zig updates only its section entry and
adjusts the size in the section header. When a function shrinks, the padding absorbs the
difference. When a function grows beyond the padding, the section is moved and all
references updated --- but this is the slow path.

### 7.2 Incremental Linking Design

Zig's incremental linking relies on stable virtual addresses. The linker assigns a VA to each
function at first link and records it in an `.zon` (Zig Object Notation) cache file alongside
source fingerprints. On rebuild:

1. Recompile changed functions to object code
2. Compare object code size to the existing allocation
3. If fits: write new bytes to existing file position, update `.debug_info` only
4. If too large: reallocate (slow path --- triggers full section relayout)
5. Rerun relocations for the changed function only

This gives Zig sub-100ms relinks for typical incremental changes in a medium project. The
tradeoff is that the output binary has slack padding (typically 10-20% overhead) and is not
bit-for-bit reproducible.

### 7.3 PE/COFF Specifics

Zig's COFF backend handles the PE header, section table, import directory, and base
relocations. It does not generate PDB debug information for PE targets; it emits DWARF in the
`.debug_*` sections instead (non-standard for Windows, but functional with LLDB). This is a
known limitation that Zig plans to address.

### 7.4 Limitations Relevant to TML

- **DWARF-only debug info on Windows** --- no PDB generation limits debugger compatibility
- **Incremental cache sensitivity** --- the `.zon` cache must be invalidated correctly; stale
  caches cause incorrect output without error
- **Written in Zig** --- TML's compiler is C++; adopting Zig's linker as a library would
  require C ABI bindings or a subprocess
- **Young codebase** --- the PE/COFF backend has seen less production use than LLD-COFF

---

## 8. Comparison Matrix

| Feature | LLD (embedded) | Zig linker | mold | TML custom (target) |
|---------|----------------|-----------|------|---------------------|
| PE/COFF (Windows) | Yes | Yes | No | Yes |
| ELF (Linux) | Yes | Yes | Yes | Planned Phase 5 |
| Mach-O (macOS) | Yes | Yes | No (open) | Planned Phase 5 |
| Incremental linking | No | Yes | No | Planned Phase 4 |
| In-memory object input | Partial (untested) | Yes | No | Planned Phase 2 |
| Re-entrant (parallel links) | No | Yes | N/A (ELF only) | Yes |
| Parallel symbol resolution | Partial | Partial | Yes | Planned |
| PDB debug info (Windows) | Yes | No | N/A | Planned |
| Written in | C++ | Zig | C++ | C++ |
| Integration with TML | Embedded | Subprocess | Subprocess | Embedded |
| Static library creation | Subprocess (ar) | Built-in | N/A | Built-in (Phase 3) |
| License | Apache 2.0 | MIT | MIT | N/A |
| Active development | Yes (LLVM team) | Yes (Zig team) | Yes (Ueyama) | N/A |

---

## 9. Gap Analysis: What LLD Cannot Give TML

Comparing TML's requirements against LLD's capabilities reveals five gaps that motivate
custom linker development.

### Gap 1: Parallel Test Suite Linking

TML's test system compiles each test suite to a separate executable and runs them in parallel.
With embedded LLD, these link operations must be serialized via `g_lld_mutex`. On a project
with 50 test suites, this means 50 sequential link operations even on a 16-core machine. A
re-entrant linker would saturate all cores.

**Workaround today**: subprocess fallback allows parallel linking but loses the in-process
speed advantage.

### Gap 2: Incremental Relinks During Development

A developer editing one function triggers a full relink: TML re-reads all `.obj` files, merges
all sections, and writes the entire executable. For a 100-function program this is fast
(~100 ms); for a 10,000-function program it will be seconds.

The Zig model of patching a single function's bytes and updating only the affected debug info
entries would reduce this to milliseconds regardless of program size.

### Gap 3: In-Memory Object Elimination

TML's compilation pipeline for one translation unit is:

```
TML source → HIR → MIR → LLVM IR text (string) → parse to Module* → emit to .obj file on disk
```

LLD then reads that `.obj` file back from disk. For a 1,000-line TML file, the `.obj` is
maybe 200 KB; the write + read round-trip takes 1-3 ms on NVMe. For 100 compilation units
in parallel, this is 100-300 ms of pure I/O with no computation.

A custom linker that accepts COFF bytes in memory from the LLVM emitter (via
`LLVMTargetMachineEmitToMemoryBuffer` or equivalent) would eliminate this entirely.

### Gap 4: Tailored for TML Program Structure

LLD is designed for production C/C++ programs with hundreds of thousands of symbols and complex
COMDAT deduplication. TML programs are structured differently:

- Modules map cleanly to `.obj` files with no COMDAT
- Symbol names are predictable (TML mangling scheme)
- Import tables are known statically (no dynamic symbol lookup surprises)
- Debug info format is under TML's control

A custom linker can exploit these invariants to skip entire phases that LLD must run.

### Gap 5: Binary Size

LLD's 40 LLVM library dependencies account for a substantial fraction of `tml.exe`'s ~100 MB
size. A standalone COFF writer that processes only TML's output would be tens of kilobytes of
code, not megabytes.

---

## 10. Decision Matrix: When to Use Each Linker

| Scenario | Recommended linker | Reason |
|----------|-------------------|--------|
| Windows exe/dll, first-party TML code | TML custom (when ready) | Fastest, re-entrant, incremental |
| Windows exe/dll, current TML | Embedded LLD (COFF) | In-process, no subprocess overhead |
| Windows exe/dll, LLD poisoned | Subprocess LLD fallback | Reliability over speed |
| Linux exe/so | Embedded LLD (ELF) | No custom ELF linker yet |
| macOS dylib | Embedded LLD (Mach-O) | No custom Mach-O linker yet |
| Parallel test suite links | Subprocess LLD (multiple processes) | Bypass g_lld_mutex serialization |
| Static .lib/.a | llvm-ar subprocess | LLD has no archiver API |
| Large third-party C++ ELF binary | mold | Fastest ELF linker available |

The recommended migration path is not a big-bang replacement. LLD remains the fallback for
all targets while the custom linker is proven incrementally:

1. **Phase 2** (in-memory COFF): Custom `.obj` intake; LLD generates the final PE. Risk: medium.
2. **Phase 3** (custom PE writer): Replace LLD-COFF for TML programs; LLD as fallback. Risk: high.
3. **Phase 4** (incremental): Stable VAs, patch on rebuild. Risk: high.
4. **Phase 5** (ELF/Mach-O): Extend custom linker to non-Windows. Risk: medium.

At each phase, the previous LLD integration remains active for correctness comparison and
fallback. The custom linker is used in production only after its output is verified to be
bit-for-bit identical to LLD's for a representative test corpus.

---

## 11. Performance Targets

Based on current LLD timing and the gap analysis above:

| Configuration | Link time (hello world) | Link time (100-fn program) | Notes |
|---------------|------------------------|---------------------------|-------|
| Today: embedded LLD | ~20 ms | ~100 ms | includes mutex acquire |
| Phase 2: in-memory obj | ~15 ms | ~70 ms | eliminates write+read I/O |
| Phase 3: custom PE writer | ~3 ms | ~10 ms | no LLVM symbol table overhead |
| Phase 4: incremental | ~1 ms | ~2 ms (1 fn changed) | patch only changed function |
| Phase 4: cold build | ~3 ms | ~10 ms | same as Phase 3 |

Parallel test suite linking (50 suites, 16 cores):

| Configuration | Total wall time | Bottleneck |
|---------------|-----------------|-----------|
| Today: embedded LLD | ~5 s | g_lld_mutex serialization |
| Subprocess fallback | ~1.5 s | process spawn overhead |
| Phase 3: re-entrant custom | ~0.3 s | pure parallel computation |
