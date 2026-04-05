# Zig Self-Hosted Linker — Architecture Deep Dive

**Date**: 2026-04-05
**Scope**: Zig's self-hosted linker across all format backends (ELF, PE/COFF, Mach-O, Wasm, Plan 9)
**Source**: Zig 0.13/0.14 compiler source (`src/link/`, ~60K lines of Zig)
**Purpose**: Inform TML custom linker design — adopt what works, skip what doesn't apply

---

## 1. Overview

Zig replaced GNU ld, LLD, and MSVC link.exe with a single self-hosted linker written entirely
in Zig. The linker ships as part of the `zig` binary — no external linker dependency at
runtime. It handles five output formats from one unified codebase:

| Format   | Target OS      | Backend file         | Lines (approx) |
|----------|----------------|----------------------|----------------|
| ELF      | Linux, FreeBSD | `src/link/Elf.zig`   | ~20,000        |
| PE/COFF  | Windows        | `src/link/Coff.zig`  | ~15,000        |
| Mach-O   | macOS, iOS     | `src/link/MachO.zig` | ~15,000        |
| Wasm     | WebAssembly    | `src/link/Wasm.zig`  | ~5,000         |
| Plan 9   | Plan 9         | `src/link/Plan9.zig` | ~2,000         |

The headline feature is **incremental linking**: when a function is recompiled, the linker
patches its machine code in-place into the existing binary in milliseconds. Full relinks
only occur when symbols move or sections overflow their padding budgets.

Zig's linker is architecturally distinct from LLD in three important ways:

1. **Compiler-linker integration**: The Zig compiler drives the linker directly, passing
   typed in-memory objects rather than writing `.o` files to disk and invoking a subprocess.
2. **Incremental-first design**: The data structures for symbol tables, sections, and
   relocations are designed from the start to support in-place patching.
3. **No intermediate object files**: For the common case (single compilation unit), Zig
   emits directly into the output binary without any `.o` intermediate.

---

## 2. The `link.File` Abstraction

Every format backend implements the `link.File` interface defined in `src/link.zig`. This
interface defines the contract between the compiler front-end and the format-specific writer.

### Core Interface Methods

```zig
pub const File = struct {
    tag: Tag,
    options: *const link.Options,
    file: ?fs.File,
    allocator: Allocator,

    // Called once at startup — create or open the output binary
    pub fn openPath(allocator: Allocator, options: *const link.Options) !*File

    // Called when a function or global is updated (incremental path)
    pub fn updateDecl(base: *File, module: *Module, decl: *Module.Decl) !void

    // Called when a decl is deleted
    pub fn freeDecl(base: *File, decl: *Module.Decl) void

    // Called at the end of compilation — write final output
    pub fn flush(base: *File, comp: *Compilation, prog_node: *std.Progress.Node) !void

    // Called to deinit and close
    pub fn destroy(base: *File) void
};
```

The `updateDecl` / `freeDecl` / `flush` separation is the key to incremental linking:
- `updateDecl` is called immediately as each function finishes compilation
- It can patch the output binary in-place without waiting for all functions
- `flush` finalizes section headers, string tables, and export directories

### Backend Selection

The compiler selects a backend based on the target triple:

```zig
pub fn createFile(options: *const Options) !*File {
    return switch (options.object_format) {
        .elf   => &(try Elf.openPath(options)).base,
        .coff  => &(try Coff.openPath(options)).base,
        .macho => &(try MachO.openPath(options)).base,
        .wasm  => &(try Wasm.openPath(options)).base,
        .plan9 => &(try Plan9.openPath(options)).base,
        .c     => &(try C.openPath(options)).base,  // C backend (transpile mode)
    };
}
```

The `C` backend is a special case: it generates C source code rather than a binary, enabling
Zig to bootstrap on platforms without a native backend.

---

## 3. Incremental Linking — Zig's Headline Feature

Incremental linking is the most architecturally significant aspect of Zig's linker. The goal
is to make edit-compile-run cycles feel instantaneous: change one function, rebuild in
milliseconds.

### Function Padding

When a function is first emitted, the linker allocates a padded slot:

```
[  actual machine code  ][  padding  ]
 ← function size →        ← budget →
```

The padding size is configurable (typically 32–128 bytes depending on function size). The
padding is filled with `0xCC` (INT3 on x86) to catch runaway execution.

When the function is recompiled:
- **Still fits in slot**: new machine code is written in-place, padding adjusted
- **No longer fits**: function is relocated to a new larger slot elsewhere in `.text`

### Jump Tables for Relocation

When a function is relocated to a new slot, existing callers may still point to the old
address. Zig handles this with a **jump table** (also called a "stubs" table):

```
Old slot (original address, now a stub):
    JMP [new_address]     ; 5 bytes — fits in any function padding

New slot (actual code):
    push rbp
    mov rbp, rsp
    ...
```

All internal calls go through the jump table from the start, so relocating a function only
requires updating the jump target — no caller patching needed.

### Dirty Tracking

The compiler informs the linker which declarations changed:

```zig
// In Module.Decl:
pub const Analysis = enum {
    unreferenced,
    in_progress,
    complete,
    dependency_failure,
};
generation: u32,  // incremented on each change
```

The linker keeps a `generation` counter. When `updateDecl` is called with a new generation,
the linker knows the function's machine code must be re-emitted.

### Section Overflow Handling

Each section (`.text`, `.data`, `.rodata`) is allocated with an initial capacity. If the
section fills up:

1. The linker marks the section as "needs growth"
2. On the next `flush`, the section is moved to a new file offset
3. All references to that section's contents are re-patched via base relocations

This is expensive (essentially a full relink for the affected section) but rare. Zig's
empirical observation is that most programs stabilize: functions stop growing after a few
iterations.

### Incremental Metadata

To support incremental linking across sessions (restarting the compiler), Zig stores
incremental metadata alongside the binary:

- For ELF: stored in a custom `.zdebug_incremental` section
- For PE/COFF: stored in a debug directory entry
- Metadata includes: function-to-offset mapping, padding budgets, generation counters

When the compiler starts, it reads this metadata and only recompiles functions whose source
files have changed (via mtime or content hash).

---

## 4. ELF Backend (Linux Primary)

The ELF backend (`src/link/Elf.zig`, ~20K lines) is the most mature and feature-complete.
It handles all Linux targets and serves as the reference implementation for the incremental
linking design.

### Internal Data Structures

```zig
pub const Elf = struct {
    base: File,
    // Symbol table — indexed by symbol index
    symtab: std.ArrayListUnmanaged(elf.Elf64_Sym),
    // String table for symbol names — interned strings
    shstrtab: std.ArrayListUnmanaged(u8),
    strtab: std.ArrayListUnmanaged(u8),
    // Section headers
    sections: std.ArrayListUnmanaged(elf.Elf64_Shdr),
    // Input atoms (one per function/global)
    atoms: std.AutoHashMapUnmanaged(u32, *Atom),
    // GOT entries for dynamic linking
    got_entries: std.ArrayListUnmanaged(GotEntry),
    // PLT stubs for external function calls
    plt_entries: std.ArrayListUnmanaged(PltEntry),
};
```

### Atom-Based Organization

Each compiled function or global becomes an `Atom` — the linker's unit of incremental work:

```zig
pub const Atom = struct {
    // Offset within its section
    offset: u64,
    // Allocated size (code + padding)
    size: u32,
    // Actual code size (without padding)
    code_size: u32,
    // Relocations pending for this atom
    relocs: std.ArrayListUnmanaged(Reloc),
    // Symbol index in symtab
    sym_index: u32,
    // Section index
    shndx: u32,
    // Generation at last write
    generation: u32,
};
```

### GOT and PLT Generation

For shared libraries and external symbols, the ELF backend generates:

- **GOT (Global Offset Table)**: One 8-byte slot per external symbol. At load time the
  dynamic linker fills in the resolved address.
- **PLT (Procedure Linkage Table)**: One stub per external function. The first call triggers
  lazy binding (resolves via `dl_runtime_resolve`); subsequent calls jump directly.

Zig generates minimal PLT entries (16 bytes each) and combines the GOT and PLT sections
so that PLT stubs and their GOT entries are adjacent for cache locality.

### Relocation Types Handled

| Relocation            | Meaning                                      |
|-----------------------|----------------------------------------------|
| `R_X86_64_64`         | Absolute 64-bit address                      |
| `R_X86_64_PC32`       | PC-relative 32-bit (call, branch)            |
| `R_X86_64_PLT32`      | PC-relative via PLT (external function call) |
| `R_X86_64_GOTPCREL`   | PC-relative to GOT entry                    |
| `R_X86_64_GOTPCRELX`  | Relaxable GOTPCREL (can be converted to PC32)|
| `R_X86_64_COPY`       | Copy relocation for extern data              |
| `R_X86_64_TPOFF32`    | Thread-local storage offset                  |
| `R_AARCH64_CALL26`    | AArch64 26-bit call offset                  |
| `R_AARCH64_ADR_PREL`  | AArch64 PC-relative ADR instruction         |

Relaxation is implemented for `GOTPCRELX`: if the linker can prove a GOT reference is
within 2GB of the use site, it rewrites the instruction sequence to a direct PC-relative
access, eliminating one memory load.

---

## 5. PE/COFF Backend (Windows)

The PE/COFF backend (`src/link/Coff.zig`, ~15K lines) targets Windows. It produces
Portable Executable files compatible with the Windows loader.

### Section Layout

PE files use a fixed section order:

```
PE Header (IMAGE_NT_HEADERS)
  Section Table (one IMAGE_SECTION_HEADER per section)
  .text   — executable code (READ | EXECUTE)
  .rdata  — read-only data, import tables, exception tables (READ)
  .data   — initialized read-write data (READ | WRITE)
  .bss    — uninitialized data (READ | WRITE) — often merged into .data
  .pdata  — exception handler data for SEH (READ)
  .xdata  — unwind info for SEH (READ)
  .idata  — import directory (READ | WRITE)
  .edata  — export directory (READ)
  .reloc  — base relocations for ASLR (READ | DISCARD)
  .debug  — debug directory (READ | DISCARD)
```

Zig generates all sections that are needed, skipping empty ones. The section alignment is
4096 bytes (page size) in the file and 512 bytes on disk (FileAlignment).

### Import Table Construction

The import table (`.idata`) is generated from all `@extern` declarations that reference
Windows DLLs:

```
IMAGE_IMPORT_DESCRIPTOR array (one per DLL)
  ├── OriginalFirstThunk → IMAGE_THUNK_DATA array (import names)
  ├── Name → DLL name string
  └── FirstThunk → IAT (Import Address Table, filled by loader)
```

At load time, the Windows loader walks the import descriptor array and patches each IAT
entry with the resolved function address. Zig pre-fills the IAT with the import name
pointers so the binary is valid before loading.

### Export Table

For DLLs, Zig constructs the export directory from `export` declarations:

```
IMAGE_EXPORT_DIRECTORY
  ├── AddressOfFunctions (EAT — Export Address Table)
  ├── AddressOfNames (name strings, sorted for binary search)
  └── AddressOfNameOrdinals (name-to-ordinal mapping)
```

The EAT entries are sorted by ordinal. The name array is sorted lexicographically to allow
`GetProcAddress` to use binary search.

### Base Relocations for ASLR

PE/COFF files loaded at non-preferred base addresses (all modern Windows with ASLR) need
base relocations. Zig generates the `.reloc` section with `IMAGE_BASE_RELOCATION` blocks:

```
Each block covers a 4KB page:
  VirtualAddress: page base
  SizeOfBlock: size in bytes of this block
  TypeOffset[]: array of (type:4 | offset:12) entries
    type = IMAGE_REL_BASED_DIR64 (3) for 64-bit absolute addresses
    type = IMAGE_REL_BASED_HIGHLOW (3) for 32-bit absolute addresses
    type = IMAGE_REL_BASED_ABSOLUTE (0) for padding
```

Only absolute address references need base relocations. PC-relative references (calls,
branches, RIP-relative data) are unaffected by load address and do not need entries.

### PE/COFF Relocation Types

| Relocation                      | Meaning                              |
|---------------------------------|--------------------------------------|
| `IMAGE_REL_AMD64_ADDR64`        | Absolute 64-bit address              |
| `IMAGE_REL_AMD64_ADDR32NB`      | 32-bit address relative to image base|
| `IMAGE_REL_AMD64_REL32`         | PC-relative 32-bit (call/branch)     |
| `IMAGE_REL_AMD64_REL32_1..5`    | PC-relative with 1–5 byte offset     |
| `IMAGE_REL_AMD64_SECTION`       | 16-bit section index (for PDB)       |
| `IMAGE_REL_AMD64_SECREL`        | 32-bit section-relative offset       |

---

## 6. Mach-O Backend (macOS)

The Mach-O backend (`src/link/MachO.zig`, ~15K lines) handles macOS and iOS targets.
Mach-O has a more complex load command system than ELF or PE/COFF.

### Load Command Architecture

Mach-O files begin with a header followed by an array of load commands. Zig generates:

```
mach_header_64
  LC_SEGMENT_64 __TEXT (rx)
    __text       — executable code
    __stubs      — PLT stubs for lazy binding
    __stub_helper — lazy binding helper code
    __cstring    — C string literals
    __const      — read-only data
  LC_SEGMENT_64 __DATA_CONST (r)
    __got        — non-lazy GOT entries
  LC_SEGMENT_64 __DATA (rw)
    __la_symbol_ptr — lazy symbol pointers (IAT equivalent)
    __data       — initialized data
    __bss        — zero-initialized data
  LC_DYLD_INFO_ONLY — dyld bind/lazy bind/export opcodes
  LC_SYMTAB       — symbol table (nlist_64 array + string table)
  LC_DYSYMTAB     — dynamic symbol table indices
  LC_LOAD_DYLIB   — shared library dependencies
  LC_UUID         — unique build ID
  LC_CODE_SIGNATURE — ad-hoc or notarized code signature
```

### dyld Bind Opcodes

Unlike ELF (which uses flat GOT/PLT tables) and PE/COFF (which uses flat IAT arrays),
Mach-O uses a bytecode format for dynamic binding. The `LC_DYLD_INFO_ONLY` load command
points to three opcode streams:

- **Bind opcodes**: Non-lazy symbols bound at load time (function pointers, data)
- **Lazy bind opcodes**: Functions bound on first call (standard PLT behavior)
- **Export trie**: Exported symbol names stored as a compressed trie for fast lookup

Zig generates these opcode streams by encoding each external reference as a sequence of
dyld opcodes: `BIND_OPCODE_SET_DYLIB_ORDINAL`, `BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM`,
`BIND_OPCODE_SET_ADDEND_SLEB`, `BIND_OPCODE_DO_BIND`.

### Code Signing

macOS requires all executables to be signed. Zig implements ad-hoc signing (sufficient for
running locally) by:

1. Computing the SHA-256 hash of each 4KB page of the `__TEXT` segment
2. Writing a `LC_CODE_SIGNATURE` load command pointing to a `CS_SuperBlob` structure
3. The `CS_SuperBlob` contains a `CS_CodeDirectory` with the page hash array

The signature is embedded in the binary before execution. No external `codesign` tool is
needed for ad-hoc signatures, though distribution signatures still require Apple's toolchain.

### Mach-O Relocation Types

| Relocation                    | Meaning                                   |
|-------------------------------|-------------------------------------------|
| `X86_64_RELOC_BRANCH`         | PC-relative call/jmp (32-bit)            |
| `X86_64_RELOC_GOT_LOAD`       | PC-relative GOT load (MOVQ instruction)  |
| `X86_64_RELOC_GOT`            | PC-relative GOT reference                |
| `X86_64_RELOC_SIGNED`         | PC-relative signed 32-bit               |
| `X86_64_RELOC_UNSIGNED`       | Absolute address (64-bit)               |
| `ARM64_RELOC_BRANCH26`        | 26-bit B/BL offset                       |
| `ARM64_RELOC_PAGE21`          | 21-bit ADRP page offset                  |
| `ARM64_RELOC_PAGEOFF12`       | 12-bit ADD/LDR page offset               |
| `ARM64_RELOC_POINTER_TO_GOT`  | PC-relative pointer to GOT              |

---

## 7. Symbol Resolution

Symbol resolution follows a two-pass model consistent across all backends.

### Pass 1: Symbol Discovery

During `updateDecl`, each atom registers its defined symbols:

```zig
// For each function or global
const sym = elf.symtab.addOneAssumeCapacity();
sym.* = .{
    .st_name = try elf.strtab.insert(name),
    .st_value = atom.offset,
    .st_size = atom.code_size,
    .st_info = elf.symbolInfo(visibility, kind),
    .st_shndx = atom.shndx,
};
```

Undefined symbol references are collected into a pending-resolution set.

### Pass 2: Reference Resolution

During `flush`, all pending undefined symbols are resolved:

1. **Local symbols**: resolved by direct offset in symtab
2. **Archive members**: lazy extraction — `.a` files are scanned for members that define
   undefined symbols; only matching members are extracted and linked
3. **Shared library symbols**: entered into the GOT/PLT as dynamic relocations
4. **Weak symbols**: if a strong definition exists, the weak definition is discarded; if
   only weak definitions exist, the first one wins

### COMDAT Groups (Deduplication)

C++ template instantiations and inline functions can appear in multiple object files.
COMDAT groups (ELF) and COMDAT sections (PE/COFF) ensure only one copy is kept:

```
ELF: SHF_GROUP flag + separate SHT_GROUP section listing members
PE:  Section name suffix: .text$mn (dollar sign + sorted suffix)
```

Zig handles COMDAT by tracking a `comdat_groups` map from group signature to winning atom.
When a duplicate is encountered, the new atom is discarded and its references redirected.

### Symbol Versioning (ELF)

ELF supports symbol versioning via `.gnu.version` and `.gnu.version_r` sections. Zig
generates these sections when linking against versioned libraries (e.g., `GLIBC_2.17`
versions of standard library symbols). This enables precise control over minimum glibc
version requirements.

---

## 8. Section Merging and Layout

### Input-to-Output Section Mapping

Input atoms are mapped to output sections by name and type:

| Input atom type       | Output section | Flags              |
|-----------------------|----------------|--------------------|
| Function code         | `.text`        | ALLOC + EXEC       |
| String literals       | `.rodata`      | ALLOC              |
| Const global data     | `.rodata`      | ALLOC              |
| Mutable global data   | `.data`        | ALLOC + WRITE      |
| Zero-init globals     | `.bss`         | ALLOC + WRITE      |
| Exception tables      | `.eh_frame`    | ALLOC              |
| Unwind info           | `.pdata`       | ALLOC (PE only)    |

### Alignment Handling

Each atom has an alignment requirement. The linker tracks the maximum alignment of all atoms
in each section and uses that as the section alignment:

```zig
// When adding atom to section
if (atom.alignment > section.alignment) {
    section.alignment = atom.alignment;
}
// Offset of next atom
const padded_offset = std.mem.alignForward(current_offset, atom.alignment);
```

### Dead Section Elimination

Sections referenced only by dead code are eliminated. The linker performs a mark-and-sweep:

1. Mark all sections referenced by exported symbols and entry points as live
2. Follow cross-section references, marking targets live
3. Discard all sections that remain unmarked

This is equivalent to LLD's `--gc-sections` and is enabled by default in Zig's release builds.

---

## 9. DWARF Debug Info

### Incremental DWARF

Zig's most ambitious feature is incremental debug info: when a function is recompiled, only
its DWARF entries need updating — not the entire `.debug_info` section.

The standard DWARF approach (used by GCC, Clang, LLD) concatenates all `.debug_info`
contributions from each compilation unit. Updating one function requires rewriting the
entire section.

Zig instead uses a **single-compilation-unit model**: all functions share one DWARF CU.
Within the CU, function DIEs (Debug Info Entries) are stored at predictable offsets. When
a function is recompiled, its DIE is updated in-place.

This requires:
- Pre-allocated slots in `.debug_info` for each function's DIE
- A separate `.debug_info.offsets` table mapping Decl → DIE offset
- Padding in DIEs to allow in-place updates without shifting subsequent DIEs

### DWARF Sections Generated

```
.debug_info    — type and variable DIEs, function DIEs
.debug_abbrev  — abbreviation tables for encoding DIEs
.debug_str     — string table for names (single pool, deduplicated)
.debug_line    — line number tables
.debug_loc     — location lists for variables in registers/memory
.debug_ranges  — address range sets for non-contiguous code
.debug_aranges — address-to-CU lookup accelerator
```

### PDB (Windows Debug Info)

Zig does not yet have a full self-hosted PDB writer. For Windows targets, it relies on
external tooling or skips PDB generation. PDB format is significantly more complex than
DWARF (it is a structured MSF container with stream multiplexing), and implementing it
fully is on Zig's roadmap but not yet complete as of Zig 0.14.

---

## 10. Performance Techniques

### Memory-Mapped I/O

Input object files (when Zig is used as a linker for external `.o` files) are mapped into
memory via `mmap`/`MapViewOfFile` rather than `read()`. The linker accesses input sections
directly through the mapped memory, avoiding copies.

Output files are also memory-mapped during writing, allowing random-access patching of
already-written sections without seeking.

### Arena Allocation

Each linker operation (one `flush` call) uses an arena allocator. All temporary allocations
for section layout, relocation processing, and string table construction are made from the
arena. At the end of `flush`, the arena is freed in one operation — no individual `free`
calls, no fragmentation.

Long-lived data (atoms, symbol tables) lives in the `gpa` (general-purpose allocator) and
persists across incremental updates.

### String Interning

All symbol names are interned into a `StringPool`:

```zig
pub const StringPool = struct {
    bytes: std.ArrayListUnmanaged(u8),
    table: std.HashMapUnmanaged(u32, void, StringContext, 80),

    pub fn insert(pool: *StringPool, s: []const u8) !u32 {
        // Return existing offset if string already present
        // Otherwise append to bytes and return new offset
    }
};
```

Symbol name comparisons use integer indices rather than string comparisons, reducing
symbol resolution from O(n × m) string comparisons to O(n × m) integer comparisons.

### Parallel Section Processing

Independent sections can be processed in parallel. Zig uses its `std.Thread.Pool` to
dispatch section layout and relocation processing across CPU cores. Sections with no
cross-section dependencies (e.g., `.text` and `.rodata`) are processed concurrently.

---

## 11. Zig CC and CMake Integration

### What `zig cc` Is

`zig cc` is not part of the linker — it is a wrapper around the bundled Clang compiler.
When invoked, it translates the command line into a Clang invocation with:

- Zig's bundled Clang 18/19 (no system Clang required)
- Zig's bundled libc headers for 40+ targets
- Zig's bundled musl, glibc, and MSVC-ABI compatible runtime libraries
- Target-specific sysroot (no system headers needed)

The linker step after Clang codegen uses either:
- **Zig's self-hosted linker** (when the target and format are fully supported)
- **LLD** (bundled inside `zig`, as fallback for unsupported configurations)

### CMake Integration

CMake projects can use `zig cc` as a drop-in compiler replacement:

```cmake
# Option 1: Inline toolchain variables
cmake -DCMAKE_C_COMPILER="zig;cc" \
      -DCMAKE_CXX_COMPILER="zig;c++" \
      -DCMAKE_AR="zig;ar" \
      -B build

# Option 2: Toolchain file
# zig-toolchain.cmake:
set(CMAKE_C_COMPILER   zig cc)
set(CMAKE_CXX_COMPILER zig c++)
set(CMAKE_AR           zig ar)
set(CMAKE_RANLIB       zig ranlib)
```

The semicolon syntax is required by CMake for multi-word compilers. CMake translates
`"zig;cc"` into an invocation of `zig cc`, which Zig parses as a single compound command.

### Cross-Compilation

Cross-compilation with `zig cc` requires only a `-target` flag:

```bash
# Compile for AArch64 Linux from Windows
zig cc -target aarch64-linux-gnu -o hello hello.c

# Compile for ARM Cortex-M bare metal
zig cc -target thumb-freestanding-eabi -mcpu=cortex_m4 -o firmware.elf firmware.c
```

No sysroot setup, no cross-compiling toolchain installation, no `--sysroot` flags. Zig
bundles musl libc and glibc headers for all supported targets.

The TML project uses `zig cc` specifically for this capability: compiling TML's C++ compiler
source on Windows without requiring a full Visual Studio installation. The target is
`x86_64-windows-msvc`, and Zig provides MSVC-ABI compatible compilation via Clang's
`-fms-compatibility` mode.

---

## 12. Lessons for TML's Custom Linker

### What to Adopt

**1. `link.File` interface pattern**
Define a common interface (`link::File`) with `update_decl`, `free_decl`, and `flush`
methods. This enables future ELF/Mach-O backends without changing the compiler front-end.

**2. Atom-based incremental design**
Each function and global is an `Atom` with an offset, size, padding budget, and generation
counter. The linker's job is to manage atom placement and patching.

**3. In-memory object passing**
TML's LLVM backend already emits `.obj` to disk. Eliminate the disk round-trip: pass the
COFF object as an in-memory `MemoryBuffer` directly to the linker. This saves one file
write and one file read per compilation unit.

**4. String interning for symbol names**
Use integer-indexed string pools for symbol tables. Avoids repeated string comparisons
during symbol resolution.

**5. Arena allocation for flush**
Allocate all layout temporaries in an arena that is freed in one call after `flush`.

**6. Function padding for incremental**
When emitting functions in debug builds, add configurable padding (e.g., 64 bytes).
Track the padding budget per atom. In-place patching becomes possible when the recompiled
function fits within the budget.

### What to Skip

**Cross-compilation sysroot bundling**: TML targets Windows primarily. Bundling musl,
glibc, and 40+ sysroots is scope-creep. Use system linker libraries for non-Windows targets
until there is user demand.

**Plan 9 format**: Zero user demand for TML on Plan 9. Skip entirely.

**Full DWARF incremental**: Complex and fragile. Start with regenerating `.debug_info` from
scratch on each `flush`. Add incremental DWARF only after the basic linker is stable.

**Self-hosted PDB**: The PDB format is an underdocumented MSF container. Start with
generating a minimal PDB using LLD's PDB writer or Microsoft's DIA SDK. Write a native PDB
writer only when there is a clear performance need.

**dyld bind opcodes**: Mach-O specific. Only needed if TML targets macOS natively. The
complexity of dyld bind/lazy bind opcodes is not justified until macOS is a primary target.

### Key Architectural Decisions for TML

| Decision                     | Recommendation                                         |
|------------------------------|--------------------------------------------------------|
| First target format          | PE/COFF (Windows is primary)                           |
| Codegen integration          | In-memory COFF via LLVMTargetMachineEmitToMemoryBuffer |
| Symbol resolution            | Two-pass: collect then resolve                         |
| Import table                 | Build from `@extern("c")` declarations in AST          |
| Export table                 | Build from `pub` declarations at crate root            |
| Base relocations             | Generate for all absolute references                   |
| Incremental linking          | Function padding + jump table (Phase 4 work)           |
| Debug info                   | Regenerate DWARF from scratch initially                |
| ELF/Mach-O                   | Add after PE/COFF is stable and tested                 |

The clearest path: replace TML's current LLD-COFF invocation (which writes a temp `.obj`,
calls `lld::lldMain`, and reads the output `.exe`) with a direct in-memory pipeline that
eliminates the disk round-trip. That alone yields a measurable speedup. A custom PE/COFF
writer comes after.
