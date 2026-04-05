# tml-link: Custom Linker Architecture

**Date**: 2026-04-05
**Status**: Design (pre-implementation)
**Depends on**: [01-zig-linker-architecture.md](01-zig-linker-architecture.md),
               [02-lld-comparison.md](02-lld-comparison.md),
               [03-tml-current-state.md](03-tml-current-state.md)

---

## 1. Design Philosophy

tml-link is a purpose-built linker for the TML toolchain. Every design decision is driven
by one question: **what does TML need to ship fast, correct, native binaries with sub-10ms
iteration time?** While designed primarily for TML's needs, tml-link handles the full scope
of C and C++ linking to serve as a complete toolchain replacement.

### Core Tenets

**TML-specific, optimized for TML workloads.** tml-link targets the output of TML's LLVM
backend and the TML compiler's C++ source. TML programs produce predictable output (no
Objective-C runtime metadata, no .NET MSIL), enabling aggressive optimization of the
common path. However, tml-link also handles the full C++ object file feature set — exception
tables, COMDAT, TLS, static constructors — because it must link the TML compiler itself,
which is ~100,000 lines of C++.

**CMake-compatible CLI.** tml-link accepts the full MSVC link.exe command-line interface on
Windows and the GNU ld interface on Linux. Any CMake project that sets `CMAKE_LINKER` can
use tml-link as a drop-in replacement. This is the primary integration path for projects
that mix TML and C/C++.

**In-memory first.** TML's LLVM backend already has `compile_ir_to_buffer()` which returns
a `std::vector<uint8_t>`. tml-link accepts that buffer directly via its in-process API.
Zero temp files, zero I/O overhead for TML-generated code. External libraries (`.lib`,
`.a`) still read from disk; this is unavoidable and acceptable.

**Incremental by default.** The linker maintains a link-state file alongside the output
binary. On rebuild, only changed functions are patched into the existing binary. Cold link
produces the state file; warm relink reads it and performs targeted patches. The target
is under 5ms for single-function changes.

**Single binary distribution.** tml-link.exe ships as part of the TML distribution alongside
tml.exe, tml-cc.exe, and tml-cxx.exe. It requires no Visual Studio installation, no
MSVC redistributable, and no separate linker download.

---

## 1b. Full Toolchain Architecture

### Overview

tml-link is one component of TML's self-contained toolchain. The complete distribution
includes:

| Binary | Purpose | Wraps/Implements |
|--------|---------|-----------------|
| `tml.exe` | TML compiler | Query-based pipeline + LLVM backend |
| `tml-cc.exe` | C compiler | Bundled Clang 20 with TML sysroot |
| `tml-cxx.exe` | C++ compiler | Bundled Clang 20 with C++ stdlib |
| `tml-link.exe` | Linker | Custom PE/COFF, ELF, Mach-O |
| `tml-ar.exe` | Archiver | COFF/ELF archive creation |

### Bundled Clang Approach

Like Zig, TML bundles Clang as its C/C++ compiler frontend. The approach:

1. **Ship Clang as a library** (libclang-cpp.dll or static link)
   - Clang's driver API allows invoking compilation without subprocess
   - Same approach Zig uses: Zig embeds a Clang 20 frontend
   - TML already links against LLVM; adding Clang frontend is incremental

2. **Bundle system headers**
   - Windows: UCRT headers + Windows SDK headers (subset needed for compilation)
   - Linux: musl libc headers (like Zig) or glibc headers
   - C++ stdlib: libc++ headers (LLVM's C++ standard library)
   - These are ~50MB compressed, ~200MB uncompressed

3. **Bundle system libraries**
   - Windows: UCRT import libs, vcruntime import libs, kernel32.lib, etc.
   - OR: generate import libraries from .dll files (like Zig does with `zig dlltool`)
   - Linux: bundle crt1.o, crti.o, crtn.o + libc.a/libc.so stubs

### How tml-cc / tml-cxx Work

```
tml-cc hello.c -o hello.obj
```

Internally:
1. Invokes Clang driver with TML's bundled sysroot
2. Sets target triple (x86_64-windows-msvc or x86_64-linux-gnu)
3. Adds bundled include paths (UCRT headers, Windows SDK headers)
4. Compiles to .obj/.o (COFF on Windows, ELF on Linux)
5. If linking requested (-o hello.exe), invokes tml-link

### C++ Object File Requirements

When linking C++ objects (e.g., building the TML compiler), tml-link must handle:

| Feature | Section(s) | Complexity |
|---------|-----------|------------|
| Virtual tables (vtables) | .rdata | Low — just data |
| RTTI (type_info) | .rdata | Low — just data |
| Exception handling (SEH) | .pdata, .xdata | HIGH — unwind tables |
| COMDAT (templates) | .text$name, .rdata$name | Medium — group selection |
| Thread-local storage (TLS) | .tls, .tls$ | Medium — TLS directory |
| Static constructors | .CRT$XCU | Medium — CRT init order |
| Static destructors | .CRT$XTU | Medium — CRT term order |
| Debug info (PDB) | .debug$S, .debug$T | HIGH — CodeView format |
| Stack cookies (/GS) | Referenced via __security_cookie | Low — just a symbol |
| Control Flow Guard | .gfids, .giats | Medium — CFG tables |

### Self-Bootstrapping Path

**Stage 1: tml-cc builds, system links** (current state, with Zig)
- C++ compilation: Zig CC (Clang wrapper)
- Linking: embedded LLD
- TML programs: LLVM backend + embedded LLD

**Stage 2: tml-cc builds, tml-link links** (target)
- C++ compilation: tml-cc (bundled Clang)
- Linking: tml-link (custom)
- TML programs: LLVM backend + tml-link
- No external dependencies except OS

**Stage 3: TML builds itself** (future — self-hosting)
- TML compiler rewritten in TML
- TML compiles itself using tml-link
- C++ only needed for LLVM backend
- Full bootstrap: tml.exe → new tml.exe

---

## 2. CMake Integration

This is the primary user-facing feature. CMake projects need exactly one setting to adopt
tml-link. The design follows the same pattern Zig established with `zig cc`.

### How Zig Does It

Zig provides `zig cc` and `zig c++` as drop-in replacements for gcc/clang. The CMake
integration is:

```cmake
cmake -DCMAKE_C_COMPILER="zig;cc" -DCMAKE_CXX_COMPILER="zig;c++"
```

Or via a toolchain file that sets `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`,
`CMAKE_LINKER`, and `CMAKE_AR`. Zig's linker understands LLD-compatible flags and
translates them to its self-hosted linker's internal API.

The key insight: Zig bundles Clang as its C/C++ frontend, so the toolchain file can
point both compiler and linker to the same `zig` binary using different subcommands.
TML takes the same approach but with separate binaries (`tml-cc.exe`, `tml-link.exe`).

### tml-toolchain.cmake

TML ships a ready-to-use CMake toolchain file at `cmake/tml-toolchain.cmake` (relative
to the TML installation directory):

```cmake
# TML Toolchain File for CMake
# Usage: cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/tml/cmake/tml-toolchain.cmake

cmake_minimum_required(VERSION 3.20)

# Detect TML installation directory (this file lives in <tml>/cmake/)
get_filename_component(TML_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# C and C++ compilers (TML-bundled Clang)
set(CMAKE_C_COMPILER
    "${TML_ROOT}/bin/tml-cc.exe"
    CACHE FILEPATH "TML bundled C compiler" FORCE)
set(CMAKE_CXX_COMPILER
    "${TML_ROOT}/bin/tml-cxx.exe"
    CACHE FILEPATH "TML bundled C++ compiler" FORCE)

# Linker
set(CMAKE_LINKER
    "${TML_ROOT}/bin/tml-link.exe"
    CACHE FILEPATH "TML custom linker" FORCE)

# Archiver (for static libraries)
set(CMAKE_AR
    "${TML_ROOT}/bin/tml-ar.exe"
    CACHE FILEPATH "TML bundled archiver" FORCE)

# Ranlib (no-op; tml-ar handles index internally)
set(CMAKE_RANLIB
    "${TML_ROOT}/bin/tml-ranlib.exe"
    CACHE FILEPATH "TML ranlib stub" FORCE)

# Tell CMake these are Clang-compatible compilers
set(CMAKE_C_COMPILER_ID   "Clang" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_ID "Clang" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_VERSION   "20.0.0" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_VERSION "20.0.0" CACHE STRING "" FORCE)

# Skip compiler probe (avoids link.exe discovery on Windows)
set(CMAKE_C_COMPILER_WORKS   TRUE CACHE BOOL "" FORCE)
set(CMAKE_CXX_COMPILER_WORKS TRUE CACHE BOOL "" FORCE)

# CRT selection: MultiThreadedDLL = /MD = links against vcruntime140.dll + ucrtbase.dll
# Change to MultiThreaded for static CRT (/MT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)

# Linker flags: force tml-link even if CMake tries to drive it via the compiler
set(CMAKE_EXE_LINKER_FLAGS_INIT    "/NOLOGO")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/NOLOGO /DLL")
set(CMAKE_STATIC_LINKER_FLAGS_INIT "")

# Include directories bundled with TML (Windows SDK headers from Zig/Clang)
set(CMAKE_SYSTEM_INCLUDE_PATH "${TML_ROOT}/include")
set(CMAKE_SYSTEM_LIBRARY_PATH "${TML_ROOT}/lib/x64")
```

### CMake Usage Examples

```bash
# Recommended: toolchain file handles everything
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/opt/tml/cmake/tml-toolchain.cmake
cmake --build build

# Minimal: just replace the linker, keep your existing compiler
cmake -B build -DCMAKE_LINKER=/opt/tml/bin/tml-link.exe

# Cross-compilation: Windows → Linux (Phase 5)
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/opt/tml/cmake/tml-toolchain-linux-x64.cmake \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=x86_64

# Debug build with TML incremental linking
cmake -B build-debug \
  -DCMAKE_TOOLCHAIN_FILE=/opt/tml/cmake/tml-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXE_LINKER_FLAGS="--incremental"
```

### How CMake Invokes the Linker

CMake drives linking by invoking the linker directly or by asking the compiler to link.
The relevant CMake variables:

| Variable | Role |
|----------|------|
| `CMAKE_LINKER` | Direct linker executable path |
| `CMAKE_<LANG>_LINK_EXECUTABLE` | Template for linking executables |
| `CMAKE_<LANG>_CREATE_SHARED_LIBRARY` | Template for linking shared libraries |
| `CMAKE_<LANG>_LINK_FLAGS` | Extra flags appended to every link |

When `CMAKE_LINKER` is set and the compiler is Clang-compatible, CMake generates link
commands of the form:

```
tml-link.exe /OUT:hello.exe /SUBSYSTEM:CONSOLE /MACHINE:X64 \
  /LIBPATH:C:\tml\lib\x64 \
  hello.obj other.obj \
  /DEFAULTLIB:libcmt /DEFAULTLIB:oldnames \
  /DEBUG /PDB:hello.pdb
```

tml-link must parse and execute this command correctly.

### Flag Compatibility Matrix

tml-link must accept all flags that MSVC link.exe emits in a typical CMake build:

**Output control:**

| Flag | Meaning |
|------|---------|
| `/OUT:<file>` | Output file path (EXE or DLL) |
| `/PDB:<file>` | Program database path (debug info) |
| `/IMPLIB:<file>` | Import library output (for DLLs) |
| `/MAP[:<file>]` | Produce a map file |

**Subsystem and entry:**

| Flag | Meaning |
|------|---------|
| `/SUBSYSTEM:CONSOLE` | PE subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI (3) |
| `/SUBSYSTEM:WINDOWS` | PE subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI (2) |
| `/SUBSYSTEM:EFI_APPLICATION` | PE subsystem = IMAGE_SUBSYSTEM_EFI_APPLICATION (10) |
| `/ENTRY:<symbol>` | Override entry point (default: mainCRTStartup) |
| `/NOENTRY` | No entry point (resource-only DLL) |

**Library search:**

| Flag | Meaning |
|------|---------|
| `/LIBPATH:<dir>` | Add directory to library search path |
| `/DEFAULTLIB:<lib>` | Add library to default link set |
| `/NODEFAULTLIB[:<lib>]` | Remove library from default link set |
| `/WHOLEARCHIVE[:<lib>]` | Include all members from archive |

**DLL output:**

| Flag | Meaning |
|------|---------|
| `/DLL` | Produce DLL instead of EXE |
| `/DEF:<file>` | Module definition file (.def) |
| `/EXPORT:<sym>[,@<ord>][,NONAME][,DATA]` | Export a symbol |

**Machine and architecture:**

| Flag | Meaning |
|------|---------|
| `/MACHINE:X64` | Target AMD64 (default) |
| `/MACHINE:ARM64` | Target AArch64 |
| `/MACHINE:X86` | Target 32-bit x86 (for compatibility; not optimized) |

**Optimization:**

| Flag | Meaning |
|------|---------|
| `/OPT:REF` | Remove unreferenced code/data (ICF precondition) |
| `/OPT:ICF[=n]` | Identical COMDAT folding, n iterations |
| `/OPT:NOREF` | Keep all sections (debug builds) |
| `/OPT:NOICF` | Disable ICF |

**Debug:**

| Flag | Meaning |
|------|---------|
| `/DEBUG` | Generate PDB (full debug info) |
| `/DEBUG:FASTLINK` | Reference objects instead of copying (faster PDB) |
| `/INCREMENTAL` | Hint: build supports incremental linking |
| `/INCREMENTAL:NO` | Disable incremental linking state file |

**Miscellaneous:**

| Flag | Meaning |
|------|---------|
| `/NOLOGO` | Suppress banner (always accepted, silently ignored) |
| `/MANIFEST:NO` | Do not embed or produce a manifest |
| `/MANIFEST:EMBED` | Embed manifest in PE section (.rsrc or .manifest) |
| `/LTCG` | Link-time code generation (accepted; triggers LTO pass) |
| `/FORCE:MULTIPLE` | Allow multiple symbol definitions (last wins) |
| `/VERBOSE` | Print symbol resolution steps to stderr |

**TML-specific extensions (only recognized by tml-link):**

| Flag | Meaning |
|------|---------|
| `--incremental` | Enable TML incremental link-state file |
| `--in-memory` | Accept object data from stdin (pipe from compiler) |
| `--profile` | Write `tml-link-profile.json` with per-phase timings |
| `--parallel=N` | Worker thread count (default: CPU count) |
| `--emit-map` | Write `<out>.tml-map.json` — symbol → VA + section |
| `--no-gc-sections` | Keep all sections even if unreferenced |

---

## 3. Command-Line Interface

### MSVC-Compatible Mode (Windows default)

The default mode on Windows. Activated when the executable is named `tml-link.exe` or
when the first argument looks like a `/` flag.

```
# Link an executable
tml-link.exe hello.obj /OUT:hello.exe /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup \
             /LIBPATH:C:\tml\lib\x64 /DEFAULTLIB:libcmt /NOLOGO

# Link a DLL
tml-link.exe mylib.obj /OUT:mylib.dll /DLL /IMPLIB:mylib.lib \
             /EXPORT:my_function /LIBPATH:C:\tml\lib\x64

# Link with debug info
tml-link.exe app.obj /OUT:app.exe /DEBUG /PDB:app.pdb \
             /SUBSYSTEM:CONSOLE /LIBPATH:C:\tml\lib\x64

# Incremental mode (TML extension)
tml-link.exe app.obj /OUT:app.exe /SUBSYSTEM:CONSOLE --incremental
```

### GNU-Compatible Mode (Linux/macOS)

Activated when the executable is named `tml-link` (no `.exe`) or when the first argument
starts with `-` rather than `/`.

```
# Link an executable
tml-link -o hello hello.o -lc -lm

# Link a shared library
tml-link -shared -o libmy.so my.o

# With rpath
tml-link -o app app.o -L/usr/local/lib -lssl -lcrypto \
         -Wl,-rpath,/usr/local/lib

# Position-independent executable
tml-link -pie -o app app.o
```

### Response Files

Both modes accept response files (prefixed with `@`):

```
tml-link.exe @link.rsp
```

Where `link.rsp` contains one flag per line:

```
hello.obj
/OUT:hello.exe
/SUBSYSTEM:CONSOLE
/LIBPATH:C:\tml\lib\x64
/DEFAULTLIB:libcmt
```

Response files are what CMake uses for very long link commands on Windows (the 32KB
command-line limit). tml-link must handle them before any other argument processing.

### In-Process API

When called from the TML compiler, tml-link bypasses the CLI entirely:

```cpp
// compiler/include/linker/tml_linker.hpp

namespace tml::link {

struct LinkerConfig {
    std::string              output_path;
    std::string              entry_point   = "mainCRTStartup";
    PESubsystem              subsystem     = PESubsystem::Console;
    OutputKind               kind          = OutputKind::Executable;
    std::string              implib_path;        // DLL only
    std::vector<std::string> library_dirs;
    std::vector<std::string> default_libs;
    bool                     debug         = false;
    bool                     incremental   = true;
    int                      parallel      = 0;   // 0 = auto
};

class TmlLinker {
public:
    explicit TmlLinker(LinkerConfig config);

    // Add an in-memory object (from compile_ir_to_buffer)
    void add_object_buffer(std::span<const uint8_t> data, std::string_view name);

    // Add an object file from disk
    void add_object_file(std::string_view path);

    // Add a .lib archive from disk
    void add_library(std::string_view name);

    // Perform the link; returns true on success
    bool link();

    // Error and warning messages, if link() returned false
    const std::vector<std::string>& diagnostics() const;
};

} // namespace tml::link
```

---

## 4. Core Data Structures

### ObjectFile

Represents one parsed COFF object. May be loaded from disk or from an in-memory buffer.
The buffer is NOT copied --- the ObjectFile holds a reference to the original allocation
(either the compiler's output buffer or a memory-mapped file region).

```cpp
struct ObjectFile {
    std::string              name;         // For error messages ("hello.obj")
    std::span<const uint8_t> data;         // Non-owning view of raw bytes
    bool                     owns_data;    // True if data was malloc'd (disk case)

    std::vector<Section>     sections;
    std::vector<Symbol>      symbols;
    StringTable              string_table;
    ArchiveMember*           archive;      // Non-null if extracted from .lib
};
```

### Symbol

```cpp
enum class SymbolKind  { Undefined, Defined, Common, Weak };
enum class SymbolScope { Local, Global };

struct Symbol {
    uint32_t    name_offset;    // Into ObjectFile::string_table
    uint16_t    section_index;  // 1-based; 0 = undefined, 0xFFFE = absolute
    uint32_t    value;          // Offset within section (defined symbols)
                                // or alignment (COMMON symbols)
    SymbolKind  kind;
    SymbolScope scope;
    uint8_t     storage_class;  // Raw COFF storage class (for .pdb)
    bool        is_comdat;      // Part of a COMDAT group?
    Symbol*     comdat_leader;  // For COMDAT: the "selected" definition
    OutputSection* output;      // Resolved during layout (nullptr until then)
    uint64_t    virtual_address; // Final VA, set during relocation
};
```

### Section

```cpp
struct Section {
    std::string              name;           // ".text", ".rdata", etc.
    std::span<const uint8_t> data;           // Raw bytes (view into ObjectFile::data)
    std::vector<Relocation>  relocations;
    uint32_t                 alignment;      // Must be power of 2; from COFF Characteristics
    uint32_t                 characteristics; // IMAGE_SCN_* flags
    bool                     is_comdat;
    std::string              comdat_name;    // Associated symbol for COMDAT
    OutputSection*           output;         // Assigned during section grouping
    uint64_t                 output_offset;  // Offset within OutputSection::data
};
```

### OutputSection

After grouping, input sections are merged into OutputSections. The final PE has one
`IMAGE_SECTION_HEADER` per OutputSection.

```cpp
struct OutputSection {
    std::string                            name;        // ".text", ".rdata", etc.
    std::vector<std::pair<ObjectFile*, Section*>> inputs;
    uint32_t                               characteristics;
    uint32_t                               alignment;   // Max of all input alignments
    uint64_t                               virtual_address;   // RVA; set during layout
    uint64_t                               file_offset;       // In output file
    std::vector<uint8_t>                   data;              // Merged contents
    uint64_t                               virtual_size;      // May exceed data.size() (.bss)
    uint64_t                               raw_size;          // Rounded to FileAlignment
};
```

### Relocation

```cpp
struct Relocation {
    uint32_t offset;        // Byte offset within Section::data to patch
    uint32_t symbol_index;  // Index into ObjectFile::symbols
    uint16_t type;          // IMAGE_REL_AMD64_* on Windows, R_X86_64_* on Linux
    int64_t  addend;        // For ELF RELA; always 0 for COFF
};
```

COFF relocation types used by TML output:

| Type | Value | Meaning |
|------|-------|---------|
| `IMAGE_REL_AMD64_ADDR64`   | 1 | 64-bit VA of target |
| `IMAGE_REL_AMD64_ADDR32NB` | 3 | 32-bit RVA of target (relative to image base) |
| `IMAGE_REL_AMD64_REL32`    | 4 | 32-bit relative offset from next instruction |
| `IMAGE_REL_AMD64_REL32_1`  | 5 | REL32 with addend -1 |
| `IMAGE_REL_AMD64_REL32_2`  | 6 | REL32 with addend -2 |
| `IMAGE_REL_AMD64_REL32_3`  | 7 | REL32 with addend -3 |
| `IMAGE_REL_AMD64_REL32_4`  | 8 | REL32 with addend -4 |
| `IMAGE_REL_AMD64_REL32_5`  | 9 | REL32 with addend -5 |
| `IMAGE_REL_AMD64_SECTION`  | 10 | 16-bit section index (for .pdb) |
| `IMAGE_REL_AMD64_SECREL`   | 11 | 32-bit offset from section start (for .pdb) |

### SymbolTable

```cpp
class SymbolTable {
    // Primary lookup: exact name → symbol
    absl::flat_hash_map<std::string_view, Symbol*> defined_;
    absl::flat_hash_map<std::string_view, Symbol*> undefined_;
    absl::flat_hash_map<std::string_view, Symbol*> weak_;

    // COMDAT groups: group name → selected leader
    absl::flat_hash_map<std::string_view, Symbol*> comdat_leaders_;

    // Interned string storage (all symbol names stored here)
    std::vector<std::string> strings_;
    absl::flat_hash_map<std::string_view, uint32_t> string_index_;

public:
    // Returns nullptr if not found
    Symbol* lookup(std::string_view name) const;

    // Registers a defined symbol; handles COMDAT selection
    // Returns nullptr if rejected (duplicate non-COMDAT, COMDAT not selected)
    Symbol* define(std::string_view name, Section* section,
                   uint32_t offset, bool is_comdat);

    // Registers an undefined reference
    void reference(std::string_view name, ObjectFile* from);

    // Returns all symbols that remain undefined after full input processing
    std::vector<std::string_view> unresolved() const;
};
```

---

## 5. In-Memory Object Pipeline

The key optimization: TML's LLVM backend produces a `std::vector<uint8_t>` containing a
valid COFF object. Today, this is written to a temp file and LLD reads it back. With
tml-link, the buffer goes directly to the linker.

```
Today (with LLD):

  LLVM backend
    │
    ├─ compile_ir_to_buffer()  → vector<uint8_t>
    │                                │
    │                         write to temp file
    │                                │
    │                    C:\Users\...\AppData\Local\Temp\tml-XXXXX.obj
    │                                │
    └─ LLD reads file ───────────────┘
       lld::lldMain({"lld-link", "C:\...\tml-XXXXX.obj", "/OUT:..."})

With tml-link:

  LLVM backend
    │
    ├─ compile_ir_to_buffer()  → vector<uint8_t>
    │                                │
    └─ linker.add_object_buffer() ───┘
       Parses COFF headers in-place, no disk I/O
```

The COFF parser reads from the buffer using pointer arithmetic, never copying:

```cpp
void TmlLinker::add_object_buffer(std::span<const uint8_t> data,
                                   std::string_view name) {
    auto obj = std::make_unique<ObjectFile>();
    obj->name      = name;
    obj->data      = data;   // Non-owning span: caller keeps buffer alive
    obj->owns_data = false;

    parse_coff(*obj);        // Fills obj->sections, obj->symbols, etc.
    register_symbols(*obj);  // Inserts into SymbolTable
    objects_.push_back(std::move(obj));
}
```

For the incremental case, the compiler already keeps the output buffer alive in its
`CompilationUnit` until the link step completes, so the lifetime is safe.

---

## 6. PE/COFF Writer

### PE File Layout

A PE32+ (64-bit) executable has this physical layout:

```
Offset      Size    Field
─────────────────────────────────────────────────────────────────
0x0000        64    DOS Header (IMAGE_DOS_HEADER)
0x0000         2      e_magic         = 0x5A4D  ("MZ")
0x003C         4      e_lfanew        = offset of PE signature (typically 0x40 or 0x80)

<e_lfanew>     4    PE Signature = 0x00004550 ("PE\0\0")

+0x04         20    COFF File Header (IMAGE_FILE_HEADER)
+0x04          2      Machine         = 0x8664 (IMAGE_FILE_MACHINE_AMD64)
+0x06          2      NumberOfSections
+0x08          4      TimeDateStamp   (Unix time; 0 for reproducible builds)
+0x0C          4      PointerToSymbolTable = 0 (no COFF debug symbols in output)
+0x10          4      NumberOfSymbols = 0
+0x14          2      SizeOfOptionalHeader = 240 (PE32+)
+0x16          2      Characteristics
                         IMAGE_FILE_EXECUTABLE_IMAGE    = 0x0002
                         IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020
                         IMAGE_FILE_DLL                 = 0x2000 (DLL only)

+0x18        240    Optional Header (IMAGE_OPTIONAL_HEADER64)
+0x18          2      Magic           = 0x020B (PE32+)
+0x1A          1      MajorLinkerVersion = 14
+0x1B          1      MinorLinkerVersion = 0
+0x1C          4      SizeOfCode      (sum of executable section raw sizes)
+0x20          4      SizeOfInitializedData
+0x24          4      SizeOfUninitializedData (.bss)
+0x28          4      AddressOfEntryPoint  (RVA of entry function)
+0x2C          4      BaseOfCode      (RVA of first .text section)
+0x30          8      ImageBase       = 0x0000000140000000 (preferred load VA)
+0x38          4      SectionAlignment = 0x1000 (4 KB pages)
+0x3C          4      FileAlignment    = 0x0200 (512 bytes)
+0x40          2      MajorOSVersion  = 6  (Windows Vista minimum)
+0x42          2      MinorOSVersion  = 0
+0x44          2      MajorImageVersion = 0
+0x46          2      MinorImageVersion = 0
+0x48          2      MajorSubsystemVersion = 6
+0x4A          2      MinorSubsystemVersion = 0
+0x4C          4      Win32VersionValue = 0
+0x50          4      SizeOfImage     (RVA of end of last section, rounded to SectionAlignment)
+0x54          4      SizeOfHeaders   (size of DOS+COFF+Optional+SectionTable, rounded to FileAlignment)
+0x58          4      CheckSum        = 0 (not required for EXEs; required for kernel drivers)
+0x5C          2      Subsystem       (IMAGE_SUBSYSTEM_WINDOWS_CUI = 3, GUI = 2)
+0x5E          2      DllCharacteristics
                         IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA    = 0x0020
                         IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE       = 0x0040  (ASLR)
                         IMAGE_DLLCHARACTERISTICS_NX_COMPAT          = 0x0100
                         IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE = 0x8000
+0x60          8      SizeOfStackReserve = 0x100000  (1 MB)
+0x68          8      SizeOfStackCommit  = 0x001000  (4 KB)
+0x70          8      SizeOfHeapReserve  = 0x100000
+0x78          8      SizeOfHeapCommit   = 0x001000
+0x7C          4      LoaderFlags     = 0
+0x80          4      NumberOfRvaAndSizes = 16

+0x84        128    Data Directory (16 × IMAGE_DATA_DIRECTORY, 8 bytes each)
                    [0]  Export Table     (for DLLs)
                    [1]  Import Table     (IMAGE_IMPORT_DESCRIPTOR array)
                    [2]  Resource Table   (not used by TML)
                    [3]  Exception Table  (.pdata; stack unwind for 64-bit)
                    [4]  Certificate Table (Authenticode; post-link signing)
                    [5]  Base Relocation  (.reloc; required for ASLR)
                    [6]  Debug            (CodeView PDB reference)
                    [7]  Architecture     (reserved, must be 0)
                    [8]  Global Ptr       (reserved, must be 0)
                    [9]  TLS Table        (Thread Local Storage)
                    [10] Load Config      (CFG, SEH, stack cookies)
                    [11] Bound Import     (obsolete)
                    [12] IAT              (Import Address Table; separate entry for fast loader lookup)
                    [13] Delay Import     (delay-loaded DLLs)
                    [14] CLR Runtime     (.NET; not used)
                    [15] Reserved         (must be 0)

+0x104    N×40    Section Table (IMAGE_SECTION_HEADER × N)
                  Each entry (40 bytes):
                    +0x00   8   Name (null-padded ASCII, or "/offset" for long names)
                    +0x08   4   VirtualSize   (actual data size; may be < SizeOfRawData)
                    +0x0C   4   VirtualAddress (RVA)
                    +0x10   4   SizeOfRawData  (rounded to FileAlignment)
                    +0x14   4   PointerToRawData (file offset)
                    +0x18   4   PointerToRelocations = 0 (in output PE)
                    +0x1C   4   PointerToLinenumbers = 0
                    +0x20   2   NumberOfRelocations = 0
                    +0x22   2   NumberOfLinenumbers = 0
                    +0x24   4   Characteristics (IMAGE_SCN_* flags)

<file offset>       Section data (one block per section, each padded to FileAlignment)
```

### Standard Section Mapping

TML output uses a fixed set of input section names that map to PE output sections:

| Input name | Output name | Characteristics |
|------------|-------------|-----------------|
| `.text`    | `.text`     | `0x60000020` (CNT_CODE + MEM_EXECUTE + MEM_READ) |
| `.text$mn` | `.text`     | merged into `.text` (MSVC ordering suffix) |
| `.rdata`   | `.rdata`    | `0x40000040` (CNT_INITIALIZED_DATA + MEM_READ) |
| `.rdata$vc60` | `.rdata` | MSVC RTTI; merged into `.rdata` |
| `.data`    | `.data`     | `0xC0000040` (CNT_INITIALIZED_DATA + MEM_READ + MEM_WRITE) |
| `.bss`     | `.bss`      | `0xC0000080` (CNT_UNINITIALIZED_DATA + MEM_READ + MEM_WRITE) |
| `.idata`   | `.idata`    | `0xC0000040` (import tables; read-write for IAT patching) |
| `.reloc`   | `.reloc`    | `0x42000040` (base relocations; MEM_DISCARDABLE + READ) |
| `.pdata`   | `.pdata`    | `0x40000040` (exception unwind; required for 64-bit SEH) |
| `.xdata`   | `.xdata`    | `0x40000040` (unwind info; referenced by .pdata) |
| `.debug`   | discarded in release | Debug info (separate PDB path) |

### Import Table Generation

The import table allows the PE loader to resolve DLL function addresses at load time.
It consists of three parallel arrays per DLL plus a name table:

```
.idata section layout:

  Import Directory Table
  ├── DLL #1: IMAGE_IMPORT_DESCRIPTOR (20 bytes)
  │   ├── OriginalFirstThunk  → RVA of Import Lookup Table for DLL #1
  │   ├── TimeDateStamp       = 0 (unbound)
  │   ├── ForwarderChain      = 0xFFFFFFFF
  │   ├── Name                → RVA of DLL name string ("kernel32.dll\0")
  │   └── FirstThunk          → RVA of Import Address Table for DLL #1
  ├── DLL #2: IMAGE_IMPORT_DESCRIPTOR
  │   └── ...
  └── Terminator: all-zeros IMAGE_IMPORT_DESCRIPTOR

  Import Lookup Tables (one per DLL, read-only)
  ├── DLL #1 ILT:
  │   ├── Entry #1 = RVA of Hint/Name for "GetProcessHeap" | 0 (ordinal bit = 0)
  │   ├── Entry #2 = RVA of Hint/Name for "HeapAlloc"
  │   └── 0 (terminator)
  └── DLL #2 ILT: ...

  Import Address Table (one per DLL, patched by loader)
  ├── DLL #1 IAT: (identical to ILT before loading; loader overwrites with VA)
  │   ├── [HeapAlloc slot]   → loader writes actual VA of HeapAlloc
  │   └── 0
  └── DLL #2 IAT: ...

  Hint/Name Table
  ├── "GetProcessHeap\0" with 2-byte hint ordinal prefix
  ├── "HeapAlloc\0"
  └── ...

  DLL Name Strings
  ├── "kernel32.dll\0"
  └── "ucrtbase.dll\0"
```

Code references to imported functions use the IAT pointer indirection:
```asm
; LLVM generates: call [rip + __imp_HeapAlloc]
; where __imp_HeapAlloc is a symbol in the IAT slot
; After loading: call [<VA of IAT slot>]  → dereferences to actual HeapAlloc VA
```

tml-link must:
1. Collect all `__imp_*` symbol references from input objects
2. Group by DLL name (from `/DEFAULTLIB:` and implicit imports)
3. Build the Import Directory, ILT, IAT, and Hint/Name table
4. Define `__imp_<name>` symbols pointing into the IAT
5. Define `<name>` symbols as thunks (for compatibility with non-`__imp_` references):
   ```asm
   ; thunk for HeapAlloc (in .text):
   HeapAlloc:
       jmp [rip + __imp_HeapAlloc]
   ```

### Export Table Generation (DLL mode)

When `/DLL` is set, tml-link builds an Export Directory:

```
Export Directory (IMAGE_EXPORT_DIRECTORY, 40 bytes):
  +0x00  4  Characteristics       = 0
  +0x04  4  TimeDateStamp         = 0
  +0x08  2  MajorVersion          = 0
  +0x0A  2  MinorVersion          = 0
  +0x0C  4  Name                  → RVA of DLL name string
  +0x10  4  Base                  = 1 (minimum ordinal)
  +0x14  4  NumberOfFunctions     (total exported)
  +0x18  4  NumberOfNames         (named exports)
  +0x1C  4  AddressOfFunctions    → RVA of Export Address Table
  +0x20  4  AddressOfNames        → RVA of Export Name Pointer Table
  +0x24  4  AddressOfNameOrdinals → RVA of Ordinal Table

Export Address Table (one 4-byte RVA per export, indexed by ordinal - Base):
  [0] → RVA of first exported function
  [1] → RVA of second exported function

Export Name Pointer Table (one 4-byte RVA per named export):
  [0] → RVA of name string for alphabetically first export
  ...

Ordinal Table (one 2-byte ordinal per named export):
  [0] → ordinal of alphabetically first export
```

Sources for exports: `/EXPORT:` flags, `.def` file, or `__declspec(dllexport)` annotations
in object symbol tables (storage class = IMAGE_SYM_CLASS_EXTERNAL + section defined).

### Base Relocation Generation

Base relocations tell the loader how to fix up absolute addresses if the image loads at
a different VA than `ImageBase`. Required for ASLR (`DYNAMIC_BASE`).

```
.reloc section layout:

  Base Relocation Block (per 4 KB page that contains absolute references):
  ├── VirtualAddress  (4 bytes) = page RVA (aligned to 0x1000)
  ├── SizeOfBlock     (4 bytes) = total block size including this header
  └── Entries (2 bytes each):
       High 4 bits = type:
         0x0 = IMAGE_REL_BASED_ABSOLUTE  (padding, no-op)
         0xA = IMAGE_REL_BASED_DIR64     (patch 8-byte absolute VA)
       Low 12 bits = offset within page (0x000 to 0xFFF)

  [next block for next page with absolute references]
  ...
  [terminating block: VirtualAddress=0, SizeOfBlock=8]
```

tml-link generates `IMAGE_REL_BASED_DIR64` entries for every `IMAGE_REL_AMD64_ADDR64`
relocation in the output. Code-relative references (`REL32`) do not need .reloc entries
because they are position-independent.

---

## 7. Symbol Resolution Engine

Resolution proceeds in three passes:

**Pass 1: Register all defined symbols.**
For each object file (in command-line order), for each symbol:
- If `scope == Local`: skip (local symbols never participate in resolution)
- If `is_comdat`: attempt COMDAT selection (see below)
- Otherwise: call `SymbolTable::define()`; if duplicate → error unless `/FORCE:MULTIPLE`

**Pass 2: Resolve undefined symbols from archives.**
For each undefined symbol in `SymbolTable::undefined_`:
1. Search each `.lib` archive (in order they were specified)
2. An archive member is extracted if it defines the needed symbol
3. Extracting a member may introduce new undefined symbols → repeat until stable
4. If a symbol remains undefined after all archives → linker error

**Pass 3: Weak symbol fallback.**
For each undefined symbol that has a corresponding weak definition:
- Use the weak definition
- Emit a warning if `--verbose`

**COMDAT Selection.**
COMDAT sections (`.text$<name>`, common in C++ templates and inline functions) follow the
"pick any" rule: the first definition seen wins, all others are discarded. tml-link
implements this with `SymbolTable::comdat_leaders_`:

```cpp
Symbol* SymbolTable::define(std::string_view name, Section* section,
                             uint32_t offset, bool is_comdat) {
    if (is_comdat) {
        auto [it, inserted] = comdat_leaders_.emplace(name, nullptr);
        if (inserted) {
            // First definition: this one wins
            it->second = create_symbol(name, section, offset, SymbolKind::Defined);
            return it->second;
        } else {
            // Duplicate: discard this section
            section->output = nullptr;  // Mark as "do not include"
            return nullptr;
        }
    }
    // Non-COMDAT: duplicate is an error
    if (defined_.contains(name)) {
        diag_.error("duplicate symbol: {}", name);
        return nullptr;
    }
    auto* sym = create_symbol(name, section, offset, SymbolKind::Defined);
    defined_.emplace(name, sym);
    return sym;
}
```

---

## 8. Relocation Engine

After symbol resolution and section layout, apply relocations to the output data:

```cpp
void apply_relocations(OutputSection& out, const SymbolTable& syms,
                        uint64_t image_base) {
    for (auto& [obj, sec] : out.inputs) {
        uint8_t* base = out.data.data() + sec->output_offset;

        for (const Relocation& rel : sec->relocations) {
            Symbol* target = &obj->symbols[rel.symbol_index];
            uint64_t S = target->virtual_address;   // Target VA
            uint64_t P = out.virtual_address         // Patch site VA
                       + sec->output_offset
                       + rel.offset;
            uint8_t* patch = base + rel.offset;

            switch (rel.type) {
            case IMAGE_REL_AMD64_ADDR64:
                // Absolute 64-bit VA: write S + addend
                write_u64(patch, S + rel.addend);
                break;

            case IMAGE_REL_AMD64_ADDR32NB:
                // 32-bit RVA: write (S - image_base) + addend
                // Must fit in 32 bits (image < 4 GB)
                write_u32(patch, (uint32_t)(S - image_base + rel.addend));
                break;

            case IMAGE_REL_AMD64_REL32:
            case IMAGE_REL_AMD64_REL32_1:
            case IMAGE_REL_AMD64_REL32_2:
            case IMAGE_REL_AMD64_REL32_3:
            case IMAGE_REL_AMD64_REL32_4:
            case IMAGE_REL_AMD64_REL32_5: {
                // PC-relative 32-bit: S - (P + 4) + addend
                // REL32_N variants have implicit addend of -N (for multi-byte instructions)
                int implicit = rel.type - IMAGE_REL_AMD64_REL32;
                int64_t delta = (int64_t)S - (int64_t)(P + 4) + rel.addend - implicit;
                if (delta < INT32_MIN || delta > INT32_MAX) {
                    diag_.error("relocation overflow: {} → {} (delta = {})",
                                sec->name, target->name_str(), delta);
                }
                write_i32(patch, (int32_t)delta);
                break;
            }

            case IMAGE_REL_AMD64_SECTION:
                write_u16(patch, (uint16_t)target->section_index);
                break;

            case IMAGE_REL_AMD64_SECREL:
                write_u32(patch, (uint32_t)(S - target->output->virtual_address));
                break;
            }
        }
    }
}
```

---

## 9. Incremental Linking

### State Model

The incremental linker maintains a state file (`<output>.tml-state`) that persists
between link operations. It records the layout of every function and data symbol:

```json
{
  "version": 1,
  "image_base": "0x140000000",
  "sections": {
    ".text": { "rva": "0x1000", "size": "0x4A00" }
  },
  "functions": {
    "tml_main": {
      "rva": "0x1020",
      "size": 48,
      "padded_size": 64,
      "hash": "a3f9e2b1"
    },
    "tml_add": {
      "rva": "0x1060",
      "size": 12,
      "padded_size": 64,
      "hash": "c7d401ee"
    }
  },
  "jump_table_rva": "0x1000",
  "jump_table_entries": {
    "tml_main": 0,
    "tml_add":  1
  }
}
```

### Function Padding

On cold link (first build), each function in `.text` is written with extra padding:
- Default: 64 bytes per function (enough for most changes)
- Configurable: `--pad-functions=N`
- Padding bytes: `0xCC` (INT3 — will crash if executed, catches bugs)

```
.text layout (cold link):

  [jump table: N × 6 bytes]
  ├── entry 0: ff 25 XX XX XX XX  (jmp [rip + offset_to_tml_main_slot])
  ├── entry 1: ff 25 XX XX XX XX  (jmp [rip + offset_to_tml_add_slot])
  └── ...

  [tml_main: up to 64 bytes]
  ├── <actual code: 48 bytes>
  └── <padding: 0xCC × 16 bytes>

  [tml_add: up to 64 bytes]
  ├── <actual code: 12 bytes>
  └── <padding: 0xCC × 52 bytes>
```

All call sites in TML-generated code call through the jump table, never directly to the
function. This ensures that if a function moves (too large for its padded slot), only
the jump table entry needs updating.

LLVM generates `call tml_main` which the linker resolves to the jump table entry for
`tml_main`, not directly to `tml_main`'s code.

### Warm Relink Algorithm

```
Input: new object buffers (only changed compilation units)

1. Load state file → previous layout
2. For each new object, diff its symbols against state:
   a. New symbol not in state → full relink required (new function needs slot)
   b. Symbol hash unchanged → skip (no code change)
   c. Symbol hash changed → patch candidate

3. For each patch candidate F:
   a. Compute new code size
   b. If new_size ≤ padded_size[F]:
      → patch in-place: overwrite F's slot in the binary
      → update jump table if address changed (it won't for in-place patch)
      → write 0xCC padding for unused tail bytes
      → update state file: new hash, new size
   c. If new_size > padded_size[F]:
      → allocate new slot at end of .text (append to binary)
      → update jump table entry for F to point to new slot
      → old slot becomes dead (filled with 0xCC)
      → if dead space > threshold (e.g., 20%) → trigger full relink

4. Write patched pages to disk (only modified 4 KB pages)
5. Update state file
```

### Binary Patching

Patching writes only the changed pages of the output binary:

```cpp
void patch_function(FileMapping& binary, const FunctionSlot& slot,
                    std::span<const uint8_t> new_code) {
    assert(new_code.size() <= slot.padded_size);

    uint8_t* dest = binary.data() + slot.file_offset;

    // Write new code
    std::memcpy(dest, new_code.data(), new_code.size());

    // Fill tail with INT3 (0xCC) to catch accidental execution of padding
    std::memset(dest + new_code.size(), 0xCC,
                slot.padded_size - new_code.size());

    // Mark pages as dirty so WriteFile flushes them
    binary.mark_dirty(slot.file_offset, slot.padded_size);
}
```

---

## 10. CRT Integration (Windows)

TML programs on Windows need the C Runtime. tml-link handles CRT selection automatically
based on the presence of certain default libraries in the link command.

### CRT Detection

| Library in command | CRT mode | Import |
|-------------------|----------|--------|
| `libcmt.lib`      | Static MT CRT | Links `libcmt.lib` (statically linked runtime) |
| `msvcrt.lib`      | Dynamic MD CRT | Links against `vcruntime140.dll` + `ucrtbase.dll` |
| `libcmtd.lib`     | Static MTd (debug) | Links `libcmtd.lib` |
| `msvcrtd.lib`     | Dynamic MDd (debug) | Links against `vcruntime140d.dll` |
| None of the above | No CRT | Bare metal (unusual; user handles CRT themselves) |

### Windows SDK Location

tml-link searches for the Windows SDK in this order:

1. `WINDOWSSDKDIR` environment variable
2. `WINDOWSSDKVERDIR` environment variable
3. Registry: `HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots\KitsRoot10`
4. Default path: `C:\Program Files (x86)\Windows Kits\10\`

Once found, the SDK version is determined by listing `Lib\` subdirectories and picking
the newest (sorted lexicographically descending — `10.0.26100.0` > `10.0.22621.0`).

Library search paths added automatically:
- `<SDK>\Lib\<version>\ucrt\x64\` — UCRT (C standard library: malloc, printf, etc.)
- `<SDK>\Lib\<version>\um\x64\` — Win32 APIs (kernel32.lib, user32.lib, etc.)

### Default Libraries

When no `/NODEFAULTLIB` is specified, tml-link adds:

```
kernel32.lib    — CreateFile, ReadFile, GetProcessHeap, ExitProcess, ...
ntdll.lib       — NT native API (LdrLoadDll, NtAllocateVirtualMemory, ...)
libcmt.lib      — C runtime (or msvcrt.lib if /MD detected)
oldnames.lib    — POSIX name aliases (open→_open, read→_read, etc.)
uuid.lib        — COM GUID definitions
```

---

## 11. Architecture Diagram

```
                         tml-link
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                  │
     CLI Parser       In-Process API      CMake Toolchain
    (MSVC/GNU)       (TmlLinker class)    (tml-toolchain.cmake)
          │                 │                  │
          └─────────────────┼──────────────────┘
                            │
                    ArgParser / Config
                            │
                ┌───────────┴───────────┐
                │                       │
          COFF Parser              ELF Parser
          (Windows)                (Linux)
                │                       │
                └───────────┬───────────┘
                            │
                     Input Objects
                    ┌────────┴────────┐
                    │                 │
              Archive (.lib)   In-Memory Buffer
              Extractor        (TML compiler)
                    │                 │
                    └────────┬────────┘
                             │
                       Symbol Table
                    (flat_hash_map, O(1))
                             │
              ┌──────────────┼──────────────┐
              │              │               │
        Pass 1:          Pass 2:         Pass 3:
       Register          Archive         Weak
        Defined          Search         Symbols
              │              │               │
              └──────────────┼───────────────┘
                             │
                    Section Grouping
                  (merge .text + .text$*)
                             │
                      Layout Engine
                   (assign RVAs, VAs)
                             │
              ┌──────────────┼──────────────┐
              │              │               │
         Relocation     Import/Export     Base Reloc
          Engine        Table Builder     Generator
              │              │               │
              └──────────────┼───────────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
                PE Writer         ELF Writer
              (Windows)           (Linux)
                    │                 │
                    └────────┬────────┘
                             │
                       Output Binary
                             │
                    ┌────────┴────────┐
                    │                 │
              State File          PDB Writer
           (incremental)          (debug info)
```

### Threading Model

Section layout and relocation are the two CPU-intensive phases. tml-link parallelizes
them with a work-stealing thread pool:

```
Layout phase:
  Main thread: assign RVAs (sequential — dependencies exist between sections)

Relocation phase:
  Thread pool: each OutputSection dispatched as one work item
  └── No cross-section dependencies → embarrassingly parallel
  └── Results written to output_data in pre-allocated slots
  └── Join before PE header write

Import table build:
  Main thread (sequential — small, not worth parallelizing)

PE/ELF write:
  Single pwrite64 / WriteFile call per section (OS handles parallelism)
```

---

## 12. Phase Correspondence

tml-link is developed in phases aligned with the roadmap in `05-implementation-roadmap.md`:

| Phase | What ships | tml-link role |
|-------|-----------|---------------|
| Phase 2 | In-memory .obj passing | `add_object_buffer()` API only; LLD still does linking |
| Phase 3 | Custom PE/COFF writer | Full PE writer; replaces LLD-COFF for TML output |
| Phase 3b | CMake integration | CLI parser + toolchain file; enables mixed C/TML projects |
| Phase 4 | Incremental linking | State file + binary patching; sub-5ms relinks |
| Phase 5 | ELF/Mach-O backends | Linux and macOS targets |

Phase 3b (CMake integration) is designed to ship in parallel with Phase 3, not after.
The CLI parser is independent of the PE writer internals and can be developed and tested
with synthetic inputs before the full writer is ready.
