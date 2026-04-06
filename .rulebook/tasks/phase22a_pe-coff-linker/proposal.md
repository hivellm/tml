# Proposal: PE/COFF Linker — Windows Linker in TML

**Task**: phase22a_pe-coff-linker
**Status**: Planned
**Priority**: P0
**Estimated effort**: 5–6 weeks
**Risk**: Medium
**LOC estimate**: ~8,500 LOC TML

## Problem

The TML compiler currently depends on LLVM's LLD for linking on Windows. LLD is a 350 MB
C++ binary embedded in `tml.exe`. It cannot be removed until TML can produce working
Windows PE executables using its own linker. This is the first step of ERA 3: eliminating
the LLD dependency entirely.

Phase 18c established that the TML x86-64 backend can emit correct COFF `.obj` files.
Phase 22a takes those `.obj` files and links them into a working `.exe` — without LLD.

## Proposed Solution

Implement a `tml-link` binary in pure TML. The linker processes COFF object files and
import libraries, resolves symbols, applies relocations, and writes a valid PE32+ executable.

**COFF object parser** — The Microsoft PE/COFF specification (pecoff_v83.docx) defines
COFF object files as a file header, section table, raw section data, symbol table, and
per-section relocation tables. The parser reads each of these structures into TML types
using `Buffer` for byte-level access, so no C code is needed.

**Symbol table construction** — Symbols from all input `.obj` files are collected into a
`HashMap[Str, SymbolEntry]`. Strong/weak resolution follows the standard rules: a strong
definition beats any number of weak definitions; two strong definitions for the same name
is a linker error. Undefined references (symbols referenced in relocations but never
defined) are collected and reported together rather than failing on the first one.

**Relocation processing** — x86-64 Windows uses three relocations in practice:
`ADDR64` (absolute 64-bit address), `REL32` (PC-relative 32-bit offset for calls and
jumps), and `SECREL` (section-relative offset, used by debug info). These cover all output
from the TML backend. The relocation engine patches the raw section `Buffer` in place,
computing each target RVA from the symbol table.

**PE output** — The writer constructs the PE32+ layout: DOS stub (64 bytes), PE signature,
COFF file header, Optional Header (240 bytes for PE32+), section headers, and section data.
Sections are aligned to 4 KB (RVA) and 512 bytes (file). The PE checksum is computed using
the standard algorithm and written into the optional header.

**Import libraries** — The `.lib` format is a POSIX archive (`!<arch>\n` magic) containing
short import descriptors. The linker parses these to discover which DLL exports which symbol
at which ordinal/hint. For each imported DLL it constructs an `IMAGE_IMPORT_DESCRIPTOR` in
the `.idata` section plus IAT/ILT arrays. Each imported symbol gets a 6-byte thunk
(`jmp [rip+IAT_offset]`) in `.text` so relocations from user code land in the thunk.

Target: ~8,500 LOC TML across six files.

## Key Decisions

- **Pure TML, no lowlevel except Buffer byte access** — The PE/COFF format is a sequence
  of packed structs. `Buffer` provides `read_u16_le`, `read_u32_le`, `read_u64_le`,
  `write_u32_le`, etc., which is all that is needed. No raw pointer arithmetic required.

- **Spec reference: pecoff_v83.docx** — The Microsoft PE/COFF specification is the
  authoritative source. Section numbers cited in comments should reference it (e.g.,
  "§3.1 Machine Types" for the Machine field). The spec is freely available from Microsoft.

- **Only AMD64 relocations initially** — The TML backend currently targets x86-64.
  ARM64 relocations can be added in phase 22c (macOS) or as a follow-up.

- **No position-independent executables initially** — PIC requires a Global Offset Table
  and is only needed for `.dll` output. The initial target is static `.exe` linking.
  DLL output can be added as a follow-up once `.exe` is working.

- **PE checksum is required** — Windows does not require it for user-mode executables,
  but drivers and system DLLs do. Computing it correctly from the start avoids having to
  retrofit it later.

- **Import by name, not ordinal** — The `.lib` files from the Windows SDK all use
  named imports. Ordinal-only imports (used by some legacy DLLs) can be added later.

## Files to Create

| File | LOC | Purpose |
|------|-----|---------|
| `compiler-tml/src/link/pe/coff_parser.tml` | ~1,400 | Parse COFF object files |
| `compiler-tml/src/link/pe/symbol_table.tml` | ~900 | Symbol collection and resolution |
| `compiler-tml/src/link/pe/relocations.tml` | ~700 | Apply COFF relocations |
| `compiler-tml/src/link/pe/pe_writer.tml` | ~1,800 | Emit PE32+ binary |
| `compiler-tml/src/link/pe/import_lib.tml` | ~1,200 | Parse .lib, build IAT/ILT |
| `compiler-tml/src/link/pe/mod.tml` | ~500 | Module root, CLI entry point |
| `compiler-tml/src/link/pe/checksum.tml` | ~300 | PE checksum algorithm |
| `compiler-tml/src/link/pe/error.tml` | ~400 | Typed linker errors |

## Integration Point

`compiler/src/backend/lld_linker.cpp` currently calls `lld::coff::link()`. After this
phase, it will instead invoke `tml-link` as a subprocess (or as a TML library call once
the TML→TML FFI is available). The command-line interface is:

```
tml-link <objects...> [<libs...>] [-o <output>] [-subsystem console|windows] [-entry <func>]
```

## Success Criteria

- `tml-link hello.obj -o hello.exe` produces a PE32+ binary that runs on Windows 10+.
- `tml-link` correctly links a binary that calls `GetStdHandle` and `WriteFile` from
  `kernel32.lib` — the same DLL imports used by TML's `essential.c` runtime.
- The TML test suite passes when `lld_linker.cpp` is redirected to call `tml-link` instead
  of LLD on Windows. LLD is no longer invoked on Windows after this phase.

## Dependencies

- **Depends on**: phase18c — COFF `.obj` file emission from the TML x86-64 backend.
- **Blocks**: phase22d — the incremental linker builds on top of this linker's symbol
  table and section layout data structures.
- **Reference**: Microsoft PE/COFF Specification v8.3 (pecoff_v83.docx), freely available.
- **Prior art**: mold linker (C++), lld-coff (C++), zld (Zig) — all open source and
  readable for cross-checking correctness of relocation formulas.
