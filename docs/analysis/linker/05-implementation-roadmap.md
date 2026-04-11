# Implementation Roadmap — Phased Plan

## Overview

This document defines the phased plan for eliminating TML's Zig CC dependency and
building a custom high-performance linker (tml-link). Each phase is independently
deliverable and tested before the next begins.

**Current state:** TML compiles its own C++ source using Zig CC (a Clang 20 wrapper).
TML programs are linked in-process using embedded LLD, called via `lld::lldMain()` in
`compiler/src/backend/lld_linker.cpp`. No Zig is involved at TML program link time.

**End state:** TML programs linked by a custom PE/COFF linker (tml-link) in <10ms,
with incremental relinking in <5ms for single-function changes. No Zig required anywhere.

---

## Phase 1: Eliminate Zig CC Dependency (1-2 weeks)

### Objective

Remove the hard dependency on `zig.exe` from the TML build system. TML already has
`--clang` and `--msvc` flags in `scripts/build.bat`; this phase validates and promotes
the `--clang` path as the primary non-Zig option.

### Background

`zig-cc.bat` and `zig-cxx.bat` are thin wrappers that delegate to:

```
zig cc  -target x86_64-windows-msvc [clang args]
zig c++ -target x86_64-windows-msvc -nostdlib++ [clang args]
```

Zig bundles Clang 20 with portable MSVC-ABI support, which eliminates the need for a
Visual Studio installation. The `--clang` flag in `build.bat` already invokes system
Clang directly via `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`. The only
gap is that `--clang` is not validated to produce an identical, fully passing build.

### Tasks

1. **Validate `--clang` end-to-end.** Run `scripts/build.bat --clang`, then run the
   full test suite. Confirm all tests pass. Document any failures and fix them.

2. **Create `cmake/toolchains/tml-clang.cmake`.** Mirrors `cmake/toolchains/zig.cmake`
   but targets the system Clang installation. Sets `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`,
   `CMAKE_AR`, `CMAKE_RANLIB`, and `CMAKE_LINKER` to Clang toolchain binaries.

3. **Add Clang detection logic in `build.bat`.** When `--clang` is passed, locate
   `clang.exe` and `clang++.exe` via `where`. Emit a clear error if not found, with
   install instructions (`winget install LLVM.LLVM`).

4. **Validate `--msvc` as an alternative.** Run `scripts/build.bat --msvc` in a Visual
   Studio developer environment. Confirm the build succeeds and tests pass. This provides
   a second Zig-free path for developers who already have MSVC.

5. **Update `build.bat` default detection order.** Change the auto-detection fallback to:
   (a) Zig CC if `zig.exe` found, (b) Clang if `clang.exe` found, (c) MSVC. Currently
   Zig CC is required when `USE_ZIG_CC=1`; it should fall back gracefully.

6. **Remove hard exit on missing Zig.** The current code in `build.bat` does
   `exit /b 1` if `zig.exe` is not in PATH. Change this to fall through to the Clang
   or MSVC path with a warning.

7. **Update `docs/user/building.md` (or equivalent).** Document the three build modes,
   their requirements, and the recommended path for each environment type (CI, developer
   without VS, developer with VS).

### Acceptance Criteria

- `scripts/build.bat --clang` produces a working `tml.exe` without Zig installed.
- The full test suite passes with `--clang` (zero regressions vs Zig CC build).
- `cmake/toolchains/tml-clang.cmake` exists and is used by `--clang` flag.
- No hard `exit /b 1` on missing `zig.exe`; Zig CC is optional, not required.
- `scripts/build.bat` help text reflects all three modes.

### Risk: LOW

The `--clang` flag already exists and has been used in testing. This is purely a
build-system change with no impact on the TML compiler or generated code.

---

## Phase 1b: Bundle Clang (tml-cc / tml-cxx) (2-4 weeks)

### Objective

Package Clang 20 as `tml-cc.exe` and `tml-cxx.exe` — the C/C++ compiler frontend for
TML's toolchain. After this phase, no externally installed compiler (Zig, MSVC, or system
Clang) is required to build TML or TML programs.

### Why

- Eliminates the dependency on any external compiler (Zig, MSVC, or system Clang)
- TML already links against LLVM; the Clang frontend is the same codebase, so adding it
  is an incremental dependency rather than a new technology
- Users get a complete toolchain in one download with no installation prerequisites

### Tasks

1. Add Clang frontend libraries to `CMakeLists.txt`:
   `clangDriver`, `clangFrontend`, `clangCodeGen`, `clangSerialization`,
   `clangTooling`, and their transitive dependencies.
2. Create `compiler/src/launcher/cc_main.cpp`: thin driver that invokes Clang's
   `CompilerInvocation` API with sysroot pointing to TML's bundled headers.
3. Create `tml-cc.exe` build target: the C compiler binary.
4. Create `tml-cxx.exe` build target: same driver with C++ defaults (`-std=c++20`,
   `-stdlib=libc++`, `-nostdinc++` pointing to bundled libc++ headers).
5. Bundle UCRT headers from Windows SDK (or detect system SDK path and copy at install
   time). Target directory: `<tml-root>/include/ucrt/` and `<tml-root>/include/win32/`.
6. Bundle libc++ headers from the LLVM project source tree (already present in
   `src/llvm-project/libcxx/include/`).
7. Create the sysroot directory structure: `<tml-root>/include/`, `<tml-root>/lib/x64/`.
8. Set the default target triple based on host OS detection at startup:
   `x86_64-windows-msvc` (Windows), `x86_64-linux-gnu` (Linux).
9. Update `cmake/tml-toolchain.cmake` to point `CMAKE_C_COMPILER` and
   `CMAKE_CXX_COMPILER` to `tml-cc.exe` and `tml-cxx.exe` respectively, and set
   `CMAKE_LINKER` to the system lld-link initially (replaced by tml-link in Phase 3).
10. Test: build the TML compiler using `tml-cc`/`tml-cxx` via the toolchain file.
    All tests must pass with zero regressions.

### Acceptance Criteria

- `tml-cc hello.c -o hello.obj` produces a valid COFF object file without any external
  compiler installed.
- `tml-cxx main.cpp -o main.obj` compiles C++20 code using bundled libc++ headers.
- TML compiler builds successfully using `tml-cc`/`tml-cxx` (all ~100,000 lines of C++).
- `scripts/build.bat --tml-cc` uses the new toolchain path.
- No Zig, no MSVC, no system Clang required on the build machine.

### Risk: MEDIUM

- The Clang library API (`CompilerInvocation`) is not as stable as the command-line
  interface; version skew between headers requires careful pinning.
- Header bundling increases the distribution size (~50MB compressed, ~200MB uncompressed).
- Some Windows SDK headers may have licensing restrictions that affect redistribution;
  legal review required before shipping.
- Fallback: if library API proves too unstable, implement `tml-cc.exe` as a thin process
  launcher for the vendored `clang.exe` binary instead of an in-process API call.

---

## Phase 2: In-Memory Object Passing (2-4 weeks)

### Objective

Eliminate the round-trip through temporary `.obj` files between LLVM codegen and LLD.
Today, `LLVMBackend::compile_ir_to_object()` writes a `.obj` file to disk; LLD then
reads it back. `compile_ir_to_buffer()` already exists and returns the object bytes as
`std::vector<uint8_t>`. This phase wires that buffer directly into LLD.

### Background

`LLVMBackend::compile_ir_to_buffer()` (line 338 of `llvm_backend.cpp`) calls
`LLVMTargetMachineEmitToMemoryBuffer`, converts the result to a `std::vector<uint8_t>`,
and returns it. LLD accepts `llvm::MemoryBufferRef` as input via its internal
`InputFile` abstractions. The gap is a thin glue layer between the two.

LLD's COFF driver (`lld::coff::link`) accepts its input via `ArrayRef<const char*>` of
argument strings, with `--input-file=<path>` for each object. For in-memory passing,
LLD provides `lld::coff::InMemoryFile` (or equivalent `MemoryBufferRef` wrapping) to
inject a buffer without a backing file.

### Tasks

1. **Audit LLD's in-memory buffer API.** In `src/llvm-project/lld/`, locate the COFF
   driver's input file handling. Determine whether `MemoryBufferRef` can be passed
   alongside file-path inputs, and which version of the API TML's vendored LLD exposes.

2. **Create `LLVMBackend::compile_ir_to_lld_buffer()`.** A new method that wraps
   `compile_ir_to_buffer()` and returns an `llvm::MemoryBufferRef` (or an owning
   wrapper) suitable for passing to LLD directly.

3. **Modify `lld_linker.cpp` to accept in-memory buffers.** Add an overload or new
   method to `LldLinker` that accepts `std::vector<std::pair<std::string, std::vector<uint8_t>>>`
   — a list of (logical name, object bytes) pairs — alongside the existing file-path inputs
   for external libraries.

4. **Wire the buffer through the builder pipeline.** In `compiler/src/cli/builder/`,
   where the build pipeline calls `compile_ir_to_object()` then feeds paths to the
   linker, switch to `compile_ir_to_buffer()` and feed the buffer to the new linker
   overload.

5. **Keep file-based path for external inputs.** C runtime object files
   (`compiler/runtime/`), Windows SDK `.lib` files, and any pre-built libraries must
   still be passed as file paths. Only TML-generated COFF objects go through the buffer.

6. **Benchmark before and after.** Measure link time for a representative workload
   (hello world, the TML test suite DLL) with and without temp files. Target: 30%
   link-time reduction from eliminating I/O.

7. **Fallback path.** If LLD's in-memory API is not cleanly accessible in the vendored
   version, implement a named-pipe or RAM-disk fallback to avoid writing to the regular
   filesystem while retaining the buffer-first design.

### Acceptance Criteria

- Zero temporary `.obj` files created for TML-generated code during a normal build.
- External `.lib` and C runtime `.obj` files continue to link correctly via file paths.
- Link times improve by at least 25% for a representative workload (measured).
- Full test suite passes with zero regressions.
- `compile_ir_to_buffer()` is the primary codegen output path; `compile_ir_to_object()`
  remains as a fallback for diagnostics and `--emit-obj` mode.

### Risk: MEDIUM

LLD's in-memory input API is not the primary documented interface. The vendored LLD
version in `src/llvm-project/` may differ from upstream. Plan for a fallback: if direct
`MemoryBufferRef` injection is not feasible, use a temporary file in a system temp
directory (faster than the build output directory on most systems) and remove it after
linking.

---

## Phase 3: Custom PE/COFF Linker — tml-link (4-8 weeks)

### Objective

Replace LLD-COFF with a custom PE/COFF linker for TML program output. The custom linker
is purpose-built for TML's workload: moderate number of objects, predictable section
layout, and deep integration with the compiler's in-memory object buffers.

This unlocks 10x link-time improvement over LLD (target: 10ms for hello world vs 100ms
today), because the custom linker avoids LLD's generality overhead and operates entirely
in memory with no subprocess or file I/O for intermediate objects.

The linker is implemented in C++ within `compiler/src/link/` and exposed as:
- `tml-link.exe`: a standalone CLI with MSVC `link.exe`-compatible flags
- A library API consumed directly by the TML compiler (no subprocess)

### Sub-phases

#### 3a: COFF Object Parser (1 week)

Parse COFF files produced by LLVM's object emitter. Support both file-backed and
in-memory (buffer) input. This is the foundation for all downstream phases.

**Tasks:**

1. Create `compiler/src/link/coff/` directory.
2. Implement `CoffParser` class: parse COFF header, optional header, section table,
   symbol table, string table, and per-section relocation entries.
3. Support `IMAGE_FILE_MACHINE_AMD64` (0x8664) exclusively for this phase.
4. Support file-backed input (`std::filesystem::path`) and buffer input
   (`std::span<const uint8_t>`). Both produce the same `CoffObject` data structure.
5. Implement string table lookup for long symbol names (names > 8 bytes stored as
   `/offset` references into the string table at the end of the symbol table).
6. Parse COMDAT sections (selection type, associated section index) — needed for
   template instantiations and inline functions that may appear in multiple objects.
7. Write unit tests covering: minimal COFF (no sections), multi-section COFF, COFF with
   relocations, COFF with COMDAT, COFF with long symbol names in the string table.
8. Validate by parsing COFF output from TML's own `compile_ir_to_buffer()` and
   checking that section counts and symbol counts match `dumpbin /all` output.

**Key COFF structures to parse:**

```
IMAGE_FILE_HEADER        (20 bytes, at offset 0)
IMAGE_SECTION_HEADER     (40 bytes each, after file header)
IMAGE_SYMBOL             (18 bytes each, after all section data)
IMAGE_RELOCATION         (10 bytes each, at offset named in section header)
```

#### 3b: Symbol Resolution (1 week)

Build a global symbol table across all input objects and resolve all undefined references.
This includes archive (`.lib`) lazy extraction.

**Tasks:**

1. Implement `SymbolTable` class: flat hash map from symbol name to `SymbolDef`
   (which object, which section, which offset, which type: defined/undefined/common/weak).
2. First pass: register all defined symbols from all explicitly named object inputs.
3. Second pass: for each undefined symbol, search archive (`.lib`) members lazily.
   Extract only the archive members that define needed symbols. Re-run pass 2 until
   no new members are extracted (fixpoint).
4. Implement `LibraryParser`: parse `.lib` files (MSVC archive format). Extract the
   first-linker-member (symbol index) and second-linker-member (long names) for O(1)
   symbol lookup without scanning all members.
5. Handle COMDAT deduplication: when multiple objects define the same COMDAT section
   (e.g., `__zcoff$` sections for template instantiations), retain one per selection
   type (`IMAGE_COMDAT_SELECT_LARGEST`, `_ANY`, `_EXACT_MATCH`, etc.).
6. Handle weak symbols: a weak symbol is overridden by any strong definition of the
   same name; if no strong definition exists, the weak definition is used.
7. Implement undefined symbol error reporting: collect ALL undefined symbols, then
   report them together (not one at a time) with the object file that references each.
8. Unit tests: single-object resolution, archive extraction (verify correct member
   selected), COMDAT deduplication, undefined symbol error format.

#### 3c: Section Layout (1 week)

Merge input sections from all objects into output sections, assign virtual addresses
and file offsets.

**Tasks:**

1. Implement `SectionMerger`: group input sections by output section name.
   Standard mappings: `.text` → `.text`, `.rdata` → `.rdata`, `.data` → `.data`,
   `.bss` → `.bss`. Debug sections (`.debug$S`, `.debug$T`) → `.debug` (or dropped
   if `/DEBUG` not specified).
2. Calculate merged section sizes respecting alignment. Each input section has an
   alignment requirement (power of 2, up to 8192 bytes); the merged section must pad
   between inputs to satisfy all alignment requirements.
3. Assign virtual addresses (RVAs). The PE image layout:
   - Headers (DOS stub + PE header + section table): 0x1000 (4KB page, aligned)
   - `.text`: base RVA = 0x1000 (typical)
   - `.rdata`: follows `.text`, rounded up to section alignment (default 0x1000)
   - `.data`: follows `.rdata`
   - `.bss`: follows `.data` (zero-initialized; exists in VA space, not in file)
4. Assign file offsets. Section data is stored in the file at file offsets, separately
   from virtual addresses. File alignment is typically 0x200 (512 bytes).
5. Build a table: for each input section, record its output section, its offset within
   that output section, and its base RVA. This table drives relocation resolution.
6. Unit tests: single section, multi-section with alignment gaps, `.bss` (no file data),
   verify that output section sizes and RVAs are consistent.

#### 3d: Relocation Engine (1 week)

Apply relocations: for each relocation entry in each input section, patch the target
bytes in the merged output buffer. Generate base relocations (`.reloc`) for ASLR.

**Tasks:**

1. Implement `RelocationApplier`. For each input section, iterate its relocation entries.
   For each entry: resolve the symbol name to its final VA using the symbol table and
   section layout. Apply the relocation by patching bytes at the specified offset within
   the section's output buffer.

2. Implement the four common AMD64 COFF relocation types:

   - `IMAGE_REL_AMD64_ADDR64` (0x0001): write absolute 64-bit VA of the symbol.
     Used for pointers in `.data` and `.rdata`.
   - `IMAGE_REL_AMD64_ADDR32NB` (0x0003): write 32-bit RVA (image-relative address,
     i.e., VA minus image base). Used in debug info and exception tables.
   - `IMAGE_REL_AMD64_REL32` (0x0004): write 32-bit PC-relative offset.
     Value = symbol_VA - (section_base_VA + reloc_offset + 4). Used for call/jmp
     instructions. The +4 accounts for the instruction's 4-byte immediate field.
   - `IMAGE_REL_AMD64_REL32_1` through `REL32_5` (0x0005–0x0009): same as REL32
     but with an additional subtraction of 1–5. Used for instructions where the
     PC-relative target starts at different offsets.

3. Generate the `.reloc` section (base relocation table). For each absolute pointer in
   the image that will be incorrect if the image loads at a non-default base address,
   emit a `IMAGE_BASE_RELOCATION` block + `IMAGE_REL_BASED_DIR64` entries. This is
   required for ASLR-compatible executables.

4. Implement overflow checking: `ADDR32NB` and `REL32` relocations fail if the result
   does not fit in 32 bits. Report the symbol name, source location, and target VA.

5. Unit tests: apply a known relocation and verify the patched bytes; verify base
   relocation table structure with `dumpbin /relocations`; verify REL32 for a call
   instruction produces correct target address at runtime.

#### 3e: PE Writer (1-2 weeks)

Write the final PE32+ executable or DLL. This is the output stage: takes merged sections,
symbol table, import table, and relocation table and writes a valid PE file.

**Tasks:**

1. Implement `PeWriter`. Construct the PE image in memory as a contiguous buffer,
   then write it to disk.

2. Write the PE headers in order:
   - DOS stub: 64-byte MZ header with `e_lfanew` pointing to the PE signature.
     Use a minimal stub (just the header + `This program cannot be run in DOS mode.`).
   - PE signature: `PE\0\0` (4 bytes) at the offset given by `e_lfanew`.
   - COFF file header (`IMAGE_FILE_HEADER`): machine type `0x8664`, section count,
     timestamp, symbol table offset (0, not used in PE), optional header size, flags.
   - Optional header (`IMAGE_OPTIONAL_HEADER64`): magic `0x020B` (PE32+), entry point
     RVA, code/data sizes, image base (default `0x140000000`), section and file
     alignment, OS/subsystem version, image size (rounded to section alignment),
     header size (rounded to file alignment), subsystem (CUI or GUI), DLL flags,
     stack/heap reserve/commit sizes, data directory count and entries.
   - Section table: one `IMAGE_SECTION_HEADER` per output section (`.text`, `.rdata`,
     `.data`, `.bss`, `.reloc`, `.idata` for imports, `.edata` for exports, `.pdata`
     for exception handlers).

3. Build the import directory (`.idata`). For each DLL imported by the TML program:
   - `IMAGE_IMPORT_DESCRIPTOR` entry: `OriginalFirstThunk` (INT), `TimeDateStamp` (0),
     `ForwarderChain` (0xFFFFFFFF), `Name` (RVA of DLL name string), `FirstThunk` (IAT).
   - Import Name Table (INT): array of RVAs to `IMAGE_IMPORT_BY_NAME` structures.
   - Import Address Table (IAT): initially identical to INT; the loader overwrites each
     entry with the resolved function address at load time.
   - `IMAGE_IMPORT_BY_NAME`: 2-byte hint followed by null-terminated function name.
   - Null `IMAGE_IMPORT_DESCRIPTOR` terminates the directory.

4. Build the export directory (`.edata`) for DLL output.
   - `IMAGE_EXPORT_DIRECTORY`: flags, timestamp, version, DLL name RVA, ordinal base,
     function count, name count, functions RVA, names RVA, name ordinals RVA.
   - Export address table: RVA of each exported function.
   - Export name pointer table: RVAs of exported function name strings, sorted ascending.
   - Export ordinal table: ordinal for each named export (relative to ordinal base).

5. Write `_pdata` (exception handling). For each `.pdata` entry emitted by LLVM (each
   function's unwind info), write a `RUNTIME_FUNCTION` record:
   `{ BeginAddress, EndAddress, UnwindInfoAddress }` (all RVAs).

6. Write the `.reloc` section produced by the relocation engine (Phase 3d).

7. Compute and fill in the PE checksum (`OptionalHeader.CheckSum`). The checksum
   algorithm is the same as the MSVC/LLD algorithm: 16-bit word sum with carries
   folded in, then add the image size.

8. Validate the output with `dumpbin /all /headers /imports /exports` and compare
   against LLD output for the same inputs. Differences should only be in timestamp,
   checksum (if deterministic mode differs), and linker version fields.

9. Unit tests: produce a minimal valid PE (no imports, single `.text` section with
   a `ret` instruction), load it with `LoadLibrary`, verify `DllMain` is called for
   DLL output. Verify `dumpbin /headers` shows correct values for all fields.

#### 3f: CRT and Windows SDK Integration (0.5 weeks)

Locate and link the C runtime libraries without requiring a Visual Studio installation.

**Tasks:**

1. Implement `WindowsSdkLocator`: search for Windows SDK in these locations, in order:
   - `WINDOWSSDKDIR` environment variable.
   - Registry: `HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots\KitsRoot10`.
   - Default path: `C:\Program Files (x86)\Windows Kits\10\`.
   - Bundled fallback: a minimal set of import `.lib` files for `kernel32`, `ntdll`,
     and `ucrt` shipped with TML (similar to what Zig bundles).

2. Implement `VcRuntimeLocator`: find `libcmt.lib`, `libvcruntime.lib`, `oldnames.lib`.
   - Check `VCTOOLSINSTALLDIR` environment variable.
   - Search Visual Studio installations via `vswhere.exe`.
   - Accept `--libpath` CLI flags to override.

3. Define the default library list for a TML executable:
   - `kernel32.lib` (always)
   - `ntdll.lib` (always)
   - `ucrt.lib` or `libucrt.lib` (UCRT — C runtime)
   - `vcruntime.lib` or `libvcruntime.lib`
   - `oldnames.lib` (name aliases for compatibility)
   - The TML runtime object files from `compiler/runtime/`.

4. Expose `--nodefaultlib` to suppress automatic library linking (for TML's own
   `#![no_std]`-equivalent mode).

#### 3g: CLI and CMake Toolchain (0.5 weeks)

Make `tml-link.exe` a drop-in replacement for MSVC `link.exe` and integrate it into
CMake builds.

**Tasks:**

1. Implement `tml-link.exe` CLI argument parsing. Accept a strict subset of MSVC
   `link.exe` flags needed for building TML programs and the TML compiler itself:
   - `/OUT:<file>` — output file path
   - `/SUBSYSTEM:CONSOLE` and `/SUBSYSTEM:WINDOWS`
   - `/DLL` — produce a DLL
   - `/EXPORT:<name>` — export a symbol by name
   - `/LIBPATH:<dir>` — add a library search directory
   - `/NODEFAULTLIB[:<lib>]` — suppress default libraries
   - `/DEBUG` — emit debug info (accept but produce no `.pdb` in Phase 3; full PDB in Phase 4)
   - `/INCREMENTAL:NO` — accepted and ignored (incremental is Phase 4)
   - `/MACHINE:X64` — accepted (only X64 supported in Phase 3)
   - Positional arguments: `.obj` and `.lib` files

2. Create `cmake/toolchains/tml-toolchain.cmake`. Defines:
   - `CMAKE_LINKER` = path to `tml-link.exe`
   - `CMAKE_C_COMPILER` = path to `clang.exe` (bundled or system)
   - `CMAKE_CXX_COMPILER` = path to `clang++.exe`
   - `CMAKE_AR` = path to `llvm-ar.exe`
   - `CMAKE_RANLIB` = path to `llvm-ranlib.exe`

3. Integration test: build the TML compiler itself using `tml-toolchain.cmake`. The
   resulting `tml.exe` must pass the full test suite.

#### 3h: C++ Object File Support (1-2 weeks)

Full support for C++ object files is required so that tml-link can link the TML compiler
itself (a ~100,000-line C++ codebase). Phase 3a–3g establish the core linker; this
sub-phase adds the C++-specific features that the earlier sub-phases defer.

**Tasks:**

1. **Exception handling (SEH unwind tables).** Parse `.pdata` and `.xdata` sections from
   input objects. Merge them into the output `.pdata` and `.xdata` sections. Ensure all
   `RUNTIME_FUNCTION` entries have correct RVAs after section layout. Verify with
   `dumpbin /unwindinfo` against LLD output.

2. **COMDAT group selection (full policy set).** Implement all five selection types:
   - `IMAGE_COMDAT_SELECT_ANY` — keep any one definition (first encountered)
   - `IMAGE_COMDAT_SELECT_LARGEST` — keep the largest definition
   - `IMAGE_COMDAT_SELECT_NEWEST` — keep the definition with the latest timestamp
   - `IMAGE_COMDAT_SELECT_EXACT_MATCH` — all definitions must be identical
   - `IMAGE_COMDAT_SELECT_ASSOCIATIVE` — follow the selection of another section
   Phase 3b implements basic COMDAT; this extends it to all five policies.

3. **`.CRT$` section ordering.** Collect all `.CRT$XCU` sections (static constructors)
   and `.CRT$XTU` sections (static destructors) from input objects. Sort by section name
   suffix (alphabetical) to determine initialization order. Merge into the output image
   so the CRT startup code correctly walks the function pointer arrays.

4. **Thread-local storage (TLS) directory.** Parse `.tls` and `.tls$` sections from
   input objects. Build the PE TLS directory (`IMAGE_TLS_DIRECTORY64`) with correct
   `StartAddressOfRawData`, `EndAddressOfRawData`, `AddressOfIndex`, and
   `AddressOfCallBacks` fields. Set the PE data directory entry for TLS.

5. **Integration test: link TML compiler with tml-link.** After steps 1–4, attempt to
   link the full TML compiler (`tml_compiler.dll` and `tml.exe`) using `tml-link` in
   place of LLD-COFF. All SEH frames, static constructors, and TLS must work correctly.

6. **Acceptance test.** The resulting `tml.exe` (linked by tml-link) must pass the full
   TML test suite — the same acceptance bar as building with LLD.

### Acceptance Criteria

- All TML tests pass when `tml-link.exe` replaces LLD-COFF.
- Link times for a representative workload (hello world, test suite DLL) are equal to or
  faster than LLD-COFF.
- `tml-link.exe` accepts MSVC-compatible flags sufficient to link the TML compiler itself.
- DLL output is correct: import/export tables are valid, `dumpbin /exports` matches expectations.
- Exception handling works: `.pdata` entries are correct, stack unwinding works through
  TML frames.
- `dumpbin /all` on `tml-link.exe` output shows no anomalies vs LLD output for the same inputs.

### Risk: HIGH

Relocation correctness is the most failure-prone aspect. One wrong byte in one relocation
produces a crash that may be silent or misleading. Mitigation: run both LLD and tml-link
for every test, byte-compare the `.text` sections, and verify that both executables produce
identical output for deterministic test programs before switching to tml-link exclusively.

---

## Phase 4: Incremental Linking (4-8 weeks)

### Objective

Relink only changed functions in milliseconds. A single-function change (common during
iterative development) should relink in under 10ms, compared to a full relink of
10-100ms in Phase 3. This requires the compiler and linker to cooperate.

### Design

Each function in a TML program is emitted with configurable padding at the end (default:
64 bytes of `int3` / `0xCC`). If the recompiled function still fits in the original slot
(including padding), tml-link patches it in place. If not, the function is relocated and
a jump stub is installed at the original address.

A jump table (`.tmlcall` section, or hijacking the existing IAT model) provides
indirection for all cross-function calls within TML-generated code. This means function
addresses never need to be embedded directly in other functions' code; they go through
the jump table. The jump table entries are updated during incremental relinks.

### Tasks

1. **Compiler: configurable function padding.** Add a codegen option to emit N bytes of
   `0xCC` (INT3) after each function. Default N = 64 in debug builds, 0 in release.
   This is controlled by a new MIR codegen flag, not a LLVM pass.

2. **Compiler: per-function IR emission.** Emit each function as a separate
   `LLVMTargetMachineEmitToMemoryBuffer` call (one COFF per function, or one COFF with
   one section per function). This enables the linker to track each function individually.

3. **Jump table emission.** In the initial full link, emit a `.tmlcall` section
   containing one 6-byte `jmp qword ptr [rip + offset]` sequence per exported/called
   function. All call sites call through the jump table. Store the jump table layout
   in the state file.

4. **State file.** Write `<output>.tml-link-state` after each full link. This file
   contains:
   - Per-function: VA of function body, size of function body (without padding),
     size of padding, VA of jump table entry.
   - Timestamp and hash of each input object for dirty detection.
   - Image base address of the linked binary.

5. **Dirty tracking.** Accept a list of changed function names from the compiler
   (passed via a file or pipe). For each changed function, determine whether it fits
   in the original slot:
   - New size ≤ old size + padding bytes: fits. Patch in place.
   - New size > old size + padding bytes: does not fit. Allocate new memory via
     `VirtualAlloc` in the same process (for in-process use) or write to a new section
     appended to the binary file.

6. **In-place patching.** Use `WriteProcessMemory` (if patching a running process, JIT
   use case) or direct file write at the known file offset. Flush the instruction cache
   after patching (`FlushInstructionCache`).

7. **Jump table update.** After patching or relocating a function, update the
   corresponding jump table entry to point to the new function address. The jump table
   is a flat writable section at a known offset in the binary file.

8. **Fallback to full relink.** Trigger a full relink when:
   - A changed function does not fit and there is no room to allocate a new slot
     (exhausted the incremental budget, typically 1MB of extra space reserved at link time).
   - New symbols are introduced (new function, new global).
   - The state file is missing or corrupt.

9. **Benchmark.** Measure: single-function change → incremental relink time. Target:
   under 10ms for a 10MB binary. Measure the full-relink fallback trigger rate across
   a representative development session.

### Acceptance Criteria

- Single-function change relinks in under 10ms for a 10MB binary (measured).
- Full-relink fallback works correctly and produces an identical binary to a fresh link.
- State file survives across build sessions; incremental state is correct after 100
  consecutive incremental relinks with no accumulated errors.
- Correct execution after incremental relink: no stale code, no IAT corruption.
- Incremental mode is disabled in release builds (full relink only).

### Risk: HIGH

In-place binary patching is one of the most subtle operations in systems programming.
Silent corruption is possible if any of the following are wrong: function boundary
detection, jump table address calculation, file offset vs. virtual address mapping,
or flush ordering. Mitigation: hash the patched bytes after writing and compare to
the expected new function bytes before releasing the binary for execution.

---

## Phase 5: Cross-Platform Backends (4-8 weeks)

### Objective

Port the custom linker to ELF (Linux/FreeBSD) and optionally Mach-O (macOS). The
PE/COFF writer from Phase 3 provides the template; the architecture is the same.
Symbol resolution (Phase 3b) and section layout (Phase 3c) are largely reusable;
only the output format writer changes.

### 5a: ELF Backend (3-5 weeks)

Produce valid ELF64 executables and shared libraries (`.so`) on Linux/x86-64.

**Tasks:**

1. Implement `ElfParser`: parse ELF64 object files produced by LLVM's ELF emitter.
   Parse the ELF header, program headers, section headers, `.symtab` / `.strtab`,
   `.dynsym` / `.dynstr`, and per-section relocation sections (`.rela.<name>`).

2. Implement `ElfWriter`. Produce a valid ELF64 file with:
   - ELF header: magic `\x7fELF`, class `ELFCLASS64`, data `ELFDATA2LSB`, type
     `ET_EXEC` (executable) or `ET_DYN` (shared library), machine `EM_X86_64`.
   - Program headers (PT_LOAD): one for `.text` + `.rodata` (RX), one for `.data` +
     `.bss` (RW). Alignment: 0x200000 (2MB) to match Linux huge-page defaults.
   - Section headers: `.text`, `.rodata`, `.data`, `.bss`, `.symtab`, `.strtab`,
     `.shstrtab` (section name string table).
   - Dynamic linking sections: `.dynamic`, `.dynsym`, `.dynstr`, `.plt`, `.got`,
     `.got.plt` — required for executables that import symbols from shared libraries.

3. Implement the AMD64 ELF relocation types:
   - `R_X86_64_64` (1): absolute 64-bit address. Used for pointer-sized data.
   - `R_X86_64_PC32` (2): 32-bit PC-relative. Used for `call` and `jmp` instructions
     within ±2GB. Value = symbol_VA + addend - reloc_VA.
   - `R_X86_64_PLT32` (4): like `PC32` but may use the PLT for inter-DSO calls.
   - `R_X86_64_GOTPCREL` (9): PC-relative to the GOT entry for a symbol. Used for
     PIC accesses to extern data.
   - `R_X86_64_COPY` (5), `R_X86_64_GLOB_DAT` (6), `R_X86_64_JUMP_SLOT` (7),
     `R_X86_64_RELATIVE` (8): dynamic relocation types written to `.rela.dyn` and
     `.rela.plt` for the dynamic linker.

4. Implement PLT stub generation. For each imported function, emit a 16-byte PLT entry:
   ```
   jmp  qword ptr [rip + GOT_entry]   ; 6 bytes
   push plt_index                     ; 5 bytes
   jmp  plt[0]                        ; 5 bytes (resolver)
   ```
   The first PLT entry (PLT[0]) calls the dynamic linker resolver.

5. Implement `.dynamic` section. Required entries: `DT_NEEDED` for each imported shared
   library, `DT_SONAME` for shared library output, `DT_RELA` / `DT_RELASZ` /
   `DT_RELAENT`, `DT_SYMTAB` / `DT_STRTAB`, `DT_PLTGOT`, `DT_PLTRELSZ`, `DT_JMPREL`,
   `DT_NULL` terminator.

6. Set `RPATH` or `RUNPATH` on output executables (controlled by `--rpath` flag) so
   the dynamic linker finds TML shared libraries at the expected path.

7. Integration test: build and run a TML "hello world" on Linux. Verify with `readelf -a`
   that all sections, program headers, and dynamic entries are correct. Verify with
   `objdump -d` that code sections match LLD-ELF output for the same inputs.

### 5b: Mach-O Backend (2-4 weeks, optional)

Produce valid Mach-O 64-bit executables and dylibs on macOS/arm64 and macOS/x86-64.

This phase is optional and lower priority than the ELF backend. Mach-O is more complex
than ELF in several areas (two-level namespace, code signing requirements, LC_DYLD_INFO
vs chained fixups). Only begin this phase if there is active demand for macOS support
without a dependency on the system linker.

**Tasks (abbreviated):**

1. Parse Mach-O 64-bit object files: mach header, load commands, sections, symbol table
   (`LC_SYMTAB`), relocation entries.
2. Implement Mach-O writer: `mach_header_64`, `LC_SEGMENT_64` load commands for
   `__TEXT` and `__DATA` segments, `LC_DYLD_INFO_ONLY` (or `LC_DYLD_CHAINED_FIXUPS`
   for arm64), `LC_SYMTAB`, `LC_DYSYMTAB`, `LC_LOAD_DYLIB`, `LC_MAIN`.
3. Handle arm64 relocation types: `ARM64_RELOC_UNSIGNED`, `ARM64_RELOC_BRANCH26`,
   `ARM64_RELOC_PAGE21`, `ARM64_RELOC_PAGEOFF12` (ADRP + ADD/LDR pattern).
4. Ad-hoc code signing: compute the `LC_CODE_SIGNATURE` load command with a SHA-256
   hash of each 4096-byte page. Required for arm64 macOS; without it, the kernel
   refuses to execute the binary.
5. Integration test: build and run a TML "hello world" on macOS. Verify with
   `otool -l` and `codesign -d --verbose`.

### Acceptance Criteria (Phase 5a — ELF)

- TML programs link and run correctly on Linux x86-64 with `tml-link --target elf`.
- Shared library (`.so`) output works; other TML programs can link against it.
- DWARF debug info is preserved in the output (pass-through of `.debug_*` sections).
- `readelf -a` on output shows no anomalies vs LLD-ELF output for the same inputs.

### Risk: MEDIUM (ELF), HIGH (Mach-O)

ELF is the most thoroughly documented object format, with extensive tooling for
validation (`readelf`, `objdump`, `nm`). The PLT/GOT mechanism is well-understood.
Mach-O on arm64 is substantially more complex due to mandatory code signing and the
chained fixups format used in recent macOS versions.

---

## Phase 6: Advanced Features (ongoing)

### 6a: PDB Debug Info Generation (4-6 weeks)

Full Windows debug info support for Visual Studio and WinDbg integration.

1. Parse CodeView debug sections (`.debug$S` for symbols, `.debug$T` for types)
   from each input object file.
2. Implement PDB Multi-Stream File (MSF) writer: SuperBlock, stream directory,
   FPM (Free Page Map), named streams.
3. Write TPI stream (Type Info): merge and deduplicate type records across
   compilation units using hash-based deduplication.
4. Write DBI stream (Debug Info): module info, section contributions, source
   file info, optional header data.
5. Write symbol records to module streams: `S_GPROC32`, `S_LPROC32`,
   `S_GDATA32`, `S_UDT`, etc.
6. Write Section Map and File Info substreams.
7. Generate public symbol hash table for fast address-to-symbol lookup.
8. Integration test: link a TML program with `/DEBUG`, open in Visual Studio,
   verify breakpoints and variable inspection work.

### 6b: Link-Time Optimization (2-4 weeks)

Whole-program optimization across compilation units.

1. Accept LLVM bitcode (`.bc`) inputs alongside COFF objects.
2. Run LLVM LTO pipeline: inter-procedural optimization, dead function
   elimination across CUs, cross-module inlining.
3. Merge bitcode modules → optimize → codegen → link as normal.
4. Support both Full LTO and Thin LTO modes.
5. Integration test: compile TML test suite with LTO, verify correctness
   and measure binary size reduction.

### 6c: Profile-Guided Optimization (2-4 weeks)

Runtime-informed layout optimization.

1. Accept `.profdata` files from instrumented runs (`llvm-profdata merge`).
2. Parse function execution counts and branch frequencies.
3. Reorder functions in `.text` by hotness (hot functions contiguous,
   cold functions at end of section).
4. Hot/cold splitting: split rarely-executed paths into `.text.cold`.
5. Integration test: profile a TML benchmark, relink with PGO, measure
   instruction cache miss reduction.

### 6d: Thin LTO (2-4 weeks)

Scalable LTO for large codebases.

1. Parallel per-module optimization (each CU optimized independently with
   cross-module summary information).
2. Summary-based whole-program analysis: import lists, type metadata,
   devirtualization candidates.
3. Parallel codegen after optimization.
4. Integration test: compile TML compiler itself with Thin LTO, verify
   build time improvement vs Full LTO.

---

## Dependencies and Critical Path

```
Phase 1 ─→ Phase 1b ─→ Phase 2 ─→ Phase 3 ─┬─→ Phase 4 (incremental)
(Zig CC)   (tml-cc)    (in-mem)   (tml-link) ├─→ Phase 5 (ELF/Mach-O)
                                              └─→ Phase 6 (PDB/LTO/PGO)
```

- **Phase 1** is prerequisite for all: without a Zig-free build, CI cannot validate later work.
- **Phase 1b** depends on Phase 1: tml-cc replaces the compiler that Phase 1 validates.
- **Phase 2** depends on Phase 1b: in-memory buffer pipeline is the interface tml-link consumes.
- **Phase 3** depends on Phase 2: custom linker consumes in-memory objects.
- **Phases 4, 5, 6** depend on Phase 3 and can be developed in parallel by separate engineers.

---

## Resource Requirements

| Phase | Engineers | Duration | Notes |
|-------|-----------|----------|-------|
| Phase 1 | 1 | 1-2 weeks | Build system only, no compiler changes |
| Phase 1b | 1 | 2-4 weeks | Clang library integration, header bundling |
| Phase 2 | 1 | 2-4 weeks | Requires LLVM/LLD API familiarity |
| Phase 3 | 1-2 | 4-8 weeks | Largest phase; sub-phases can parallelize |
| Phase 4 | 1 | 4-8 weeks | Requires Phase 3 complete |
| Phase 5a (ELF) | 1 | 3-5 weeks | Requires Phase 3 complete |
| Phase 5b (Mach-O) | 1 | 2-4 weeks | Optional; requires Phase 5a for reference |
| Phase 6a (PDB) | 1 | 4-6 weeks | Complex format; requires Phase 3 |
| Phase 6b (LTO) | 1 | 2-4 weeks | LLVM bitcode integration |
| Phase 6c (PGO) | 1 | 2-4 weeks | Profile data parsing + section reordering |
| Phase 6d (Thin LTO) | 1 | 2-4 weeks | Parallel optimization pipeline |

## Total Timeline

| Scenario | Duration |
|----------|----------|
| Core toolchain (Phases 1-3, tml-cc + tml-link) | 12-18 weeks |
| With incremental linking (+ Phase 4) | 16-26 weeks |
| Full cross-platform (+ Phase 5) | 20-34 weeks |
| Complete with advanced features (+ Phase 6) | 30-50 weeks |

Phase 3 is the dominant risk. A two-engineer team working Phase 3 sub-phases in
parallel (3a/3b in parallel, then 3c/3d in parallel, then 3e/3f/3g/3h) could compress
the Phase 3 timeline to 3-4 weeks rather than 4-8.

---

## Per-Phase Performance Targets

| Phase | Link Time (hello world) | Link Time (full test suite DLL) | vs Baseline |
|-------|------------------------|----------------------------------|-------------|
| Baseline (embedded LLD) | ~100ms | ~800ms | 1.0x |
| After Phase 2 (in-memory) | ~70ms | ~560ms | 1.4x |
| After Phase 3 (tml-link) | ~10ms | ~80ms | 10x |
| After Phase 4 (incremental, single-fn) | ~5ms | n/a (full relink) | 20x |

These targets are estimates based on removing I/O, reducing symbol table overhead,
and eliminating LLD's generality. Actual numbers must be measured after Phase 3 is
complete and benchmarked against LLD-COFF on the same inputs.

---

## Validation Strategy

Each phase must pass the following validation before the next begins:

1. **Full test suite.** `mcp__tml__test` with `no_cache=true` must show zero
   regressions vs the baseline (embedded LLD with file-based `.obj`).

2. **Binary diff.** For deterministic inputs (fixed timestamp, fixed seed), the output
   binary must be byte-identical to LLD output for all sections except the linker
   version and timestamp fields in the PE optional header.

3. **Tool validation.** `dumpbin /all`, `readelf -a` (Linux), or `otool -l` (macOS)
   on the output must show no anomalies, warnings, or missing sections.

4. **Runtime validation.** At minimum: hello world runs, the TML test suite DLL loads
   and all tests pass, and a TML DLL can be loaded by a TML executable at runtime.

5. **Performance measurement.** Link times must be measured for three inputs (hello
   world, test suite DLL, TML compiler itself) before and after each phase. Results are
   recorded in `docs/analyses/linker/06-performance-targets.md`.
