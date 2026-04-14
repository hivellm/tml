# Proposal: ELF Linker — Linux Linker in TML

**Task**: phase22b_elf-linker
**Status**: Planned
**Priority**: P0
**Estimated effort**: 4–5 weeks
**Risk**: Medium-High
**LOC estimate**: ~7,200 LOC TML

## Problem

The TML compiler uses LLD to produce ELF executables on Linux. LLD is embedded in `tml.exe`
as 350 MB of compiled C++ code. Eliminating it from the Linux build path requires a TML
implementation of an ELF linker that can produce working executables linking against glibc
and system shared libraries.

This is the Linux-specific phase of ERA 3. Phase 22a (PE/COFF) handles Windows. The ELF
format is more complex than COFF due to GOT/PLT construction, symbol versioning, and the
dynamic linking model, which is why this phase has a higher risk rating.

## Proposed Solution

Implement a `tml-link` subcommand (or standalone binary) that processes ELF64 `.o` files,
resolves symbols, constructs the GOT/PLT machinery for dynamic linking, applies relocations,
and writes a valid ELF64 executable.

**ELF64 object parser** — The System V ABI ELF specification defines ELF objects as a header
plus named sections referenced through a section header table. The parser reads each
`Elf64_Shdr` to discover sections (`.text`, `.data`, `.rodata`, `.bss`, `.symtab`,
`.strtab`, `.rela.*`) and reads their raw bytes into `Buffer` for later processing.

**Symbol resolution** — The ELF symbol binding model distinguishes `STB_GLOBAL` (strong),
`STB_WEAK` (weak), and `STB_LOCAL` (file-scoped). Resolution follows the same strong-beats-
weak logic as COFF but with the additional complication of `SHN_COMMON` (uninitialized
tentative symbols) that must be allocated in `.bss` at their required alignment.

**Symbol versioning** — glibc ships multiple versions of functions like `memcpy` and
`stat` behind ELF symbol versioning (`.gnu.version_r` sections in `.so` files). The linker
must parse these version requirement sections and bind imported symbols to their correct
glibc version to avoid "GLIBC_2.17 not found" errors on older systems.

**GOT/PLT** — The hardest part. Every dynamically-linked function call goes through a
PLT stub that redirects through a GOT entry. At program startup `ld.so` patches GOT
entries with actual function addresses. The `tml-link` implementation must:
1. Allocate a GOT slot for every dynamic symbol that is referenced.
2. Emit a PLT stub for every dynamic function call.
3. Emit the `.dynamic` section listing all required shared libraries and relocation tables.
4. Set `ld.so` up to do the rest at runtime.

**ELF output** — The final binary contains two `PT_LOAD` segments (one RX for code, one
RW for data), a `PT_INTERP` pointing to `/lib64/ld-linux-x86-64.so.2`, and a `PT_DYNAMIC`
pointing to `.dynamic`. Section headers are written at the end (optional for executables,
required for shared objects and debug info).

Target: ~7,200 LOC TML across seven files.

## Key Decisions

- **ELF spec reference: System V ABI, AMD64 supplement** — The two documents are
  `sysv-abi.pdf` and `x86_64-abi.pdf` (also known as the "psABI"). The AMD64 supplement
  defines all x86-64 relocation types and the GOT/PLT calling convention.

- **No position-independent executables (PIE) initially** — Non-PIE loads at a fixed
  address (0x400000), which simplifies relocation. PIE is needed for ASLR and can be
  added later as a flag.

- **Symbol versioning is non-optional** — Without version binding, a binary linking against
  glibc will fail on any system where glibc differs from the build machine. The versioning
  logic must be correct from the start.

- **Lazy binding via PLT** — Eager binding (filling GOT at startup) is simpler to implement
  but incompatible with the default behavior of `ld.so`. Lazy binding is required for
  compatibility with existing shared libraries that expect PLT-based call dispatch.

- **`R_X86_64_GOTPCRELX` relaxation is optional initially** — This optimization converts
  GOT-indirect loads to direct PC-relative calls when the symbol is local. It is a
  correctness-neutral optimization; skipping it produces slightly larger code but still
  correct binaries.

## Files to Create

| File | LOC | Purpose |
|------|-----|---------|
| `compiler-tml/src/link/elf/elf_parser.tml` | ~1,200 | Parse ELF64 object files |
| `compiler-tml/src/link/elf/symbol_table.tml` | ~900 | Symbol resolution + versioning |
| `compiler-tml/src/link/elf/relocations.tml` | ~800 | Apply ELF64 relocations |
| `compiler-tml/src/link/elf/got.tml` | ~500 | GOT slot allocation |
| `compiler-tml/src/link/elf/plt.tml` | ~600 | PLT stub emission |
| `compiler-tml/src/link/elf/elf_writer.tml` | ~1,800 | Emit ELF64 binary |
| `compiler-tml/src/link/elf/mod.tml` | ~400 | Module root, CLI entry point |
| `compiler-tml/src/link/elf/error.tml` | ~300 | Typed linker errors |

## Success Criteria

- `tml-link hello.o libc.so.6 -o hello` produces an ELF64 binary that runs on Linux x86-64.
- `readelf -a hello` shows correct segment layout, `.dynamic` entries, and symbol table.
- `ldd hello` shows the expected shared library dependencies.
- All existing TML tests pass on Linux when `lld_linker.cpp` is switched to invoke
  `tml-link` instead of LLD. LLD is no longer invoked on Linux.

## Dependencies

- **Depends on**: phase18c — ELF `.o` file emission from the TML x86-64 backend.
- **Depends on**: phase22a — linker infrastructure patterns (symbol table types, error
  reporting conventions) established in the PE/COFF implementation.
- **Blocks**: phase22d — incremental linker builds on section layout data from this phase.
- **Reference**: System V ABI ELF spec, AMD64 ABI supplement (`x86_64-abi.pdf`), GNU
  binutils source (ld/emulparams/elf_x86_64.sh), mold linker source (gold standard for
  modern ELF linking speed and correctness).
