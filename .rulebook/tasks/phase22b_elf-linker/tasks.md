# Tasks: ELF Linker — Linux Linker in TML

**Status**: Planned (0/20)
**Depends on**: phase18c (object emission — ELF .o files from TML backend), phase22a (linker infrastructure patterns)
**Blocks**: phase22d (incremental linker)
**Duration**: 4–5 weeks
**Risk**: Medium-High — GOT/PLT construction and symbol versioning for glibc are the hardest parts

---

## Phase 1: ELF64 Object Parser (4 items)

- [ ] 1.1 Create `compiler-tml/src/link/elf/elf_parser.tml` — parse ELF64 header (`e_ident` magic + class + data + version, `e_type`, `e_machine` must be EM_X86_64 = 0x3E, `e_shoff`, `e_shnum`, `e_shstrndx`); return `ElfObject` struct
- [ ] 1.2 Parse section headers: read `Elf64_Shdr` array at `e_shoff`; resolve `sh_name` strings from `.shstrtab`; record `sh_type`, `sh_flags`, `sh_addr`, `sh_offset`, `sh_size`, `sh_link`, `sh_info`, `sh_addralign`; read raw section data into `Buffer`
- [ ] 1.3 Parse symbol table (`SHT_SYMTAB` section): read `Elf64_Sym` entries (24 bytes each); decode `st_name` from `.strtab`, binding/type from `st_info`, `st_shndx` (special values: `SHN_UNDEF`, `SHN_COMMON`, `SHN_ABS`); collect into `List[ElfSymbol]`; also parse `.dynsym` when present
- [ ] 1.4 Parse relocation sections (`SHT_RELA`): read `Elf64_Rela` entries (24 bytes: `r_offset`, `r_info`, `r_addend`); decode symbol index as `r_info >> 32` and relocation type as `r_info & 0xFFFFFFFF`; store per-section as `List[ElfRela]`

## Phase 2: Symbol Resolution (3 items)

- [ ] 2.1 Create `compiler-tml/src/link/elf/symbol_table.tml` — collect symbols from all input `.o` files; resolve `STB_GLOBAL` (strong) over `STB_WEAK` (weak); `STB_LOCAL` symbols are never exported and do not participate in global resolution; two `STB_GLOBAL` definitions for the same name is a linker error; report all undefined symbols together after processing all inputs
- [ ] 2.2 Implement symbol versioning for glibc compatibility: parse `.gnu.version` (SHT_GNU_versym) and `.gnu.version_r` (SHT_GNU_verneed) sections from shared library inputs (`.so` files passed as `-l` arguments); when the same symbol name appears at multiple versions (e.g., `memcpy@GLIBC_2.2.5` and `memcpy@GLIBC_2.14`), prefer the version matching the minimum glibc requirement
- [ ] 2.3 Handle `SHN_COMMON` symbols (tentative definitions): sort by descending alignment, allocate consecutively in output `.bss`; if two objects both define a `COMMON` symbol with the same name, take the larger size — this matches the System V ABI common symbol merging rules

## Phase 3: Relocation Processing (4 items)

- [ ] 3.1 Create `compiler-tml/src/link/elf/relocations.tml` — `apply_relocations(section: mut ref ElfSection, syms: ref SymbolTable, got: ref Got, plt: ref Plt, base: I64)` entry point; compute `S` (symbol address), `A` (addend from `r_addend`), `P` (address of relocation site), `G` (GOT offset of symbol), `GOT` (GOT base address)
- [ ] 3.2 Implement `R_X86_64_64` (type 1): `S + A` — write 64-bit absolute value; used in `.data` for function pointer tables and vtables
- [ ] 3.3 Implement `R_X86_64_PC32` (type 2) and `R_X86_64_PLT32` (type 4): `S + A - P` — 32-bit PC-relative; `PLT32` uses the symbol's PLT stub address instead of `S` for dynamic symbols; validate result fits in I32 signed range, emit relocation-overflow error with symbol name and section if not
- [ ] 3.4 Implement `R_X86_64_GOTPCREL` (type 9) and `R_X86_64_GOTPCRELX` (type 41): `G + GOT + A - P` — PC-relative reference to GOT entry; `GOTPCRELX` supports relaxation to `R_X86_64_PC32` when the target is local (eliminates GOT indirection for local symbols)

## Phase 4: GOT/PLT Construction (4 items)

- [ ] 4.1 Create `compiler-tml/src/link/elf/got.tml` — `Got` type: reserve GOT[0] for `.dynamic` address, GOT[1] for link map (filled by `ld.so`), GOT[2] for `_dl_runtime_resolve` (filled by `ld.so`); allocate one 8-byte slot per dynamic symbol starting at index 3; provide `got_offset_of(sym: Str) -> I64`
- [ ] 4.2 Create `compiler-tml/src/link/elf/plt.tml` — `Plt` type: emit PLT[0] header (16 bytes: `push qword [GOT+8]` / `jmp [GOT+16]` / `nop nop nop nop`); for each dynamic symbol emit 16-byte stub: `jmp [rip + GOT_entry - next_instr]` / `push <index>` / `jmp PLT[0]`; this implements lazy binding via `_dl_runtime_resolve`
- [ ] 4.3 Build `.dynamic` section: write `Elf64_Dyn` array with `DT_NEEDED` (one per shared library name), `DT_SYMTAB`, `DT_STRTAB`, `DT_STRSZ`, `DT_RELA`, `DT_RELASZ`, `DT_RELAENT` (= 24), `DT_PLTGOT`, `DT_JMPREL`, `DT_PLTRELSZ`, `DT_PLTREL` (= DT_RELA), terminated by `DT_NULL`
- [ ] 4.4 Build `.interp` section: write ELF interpreter path as null-terminated string; on Linux x86-64 the path is `/lib64/ld-linux-x86-64.so.2`; this is the path the kernel passes to `execve` as the dynamic linker; emit as `PT_INTERP` program header pointing at this section

## Phase 5: ELF Output (3 items)

- [ ] 5.1 Create `compiler-tml/src/link/elf/elf_writer.tml` — assign final virtual addresses: non-PIE executable base at 0x400000; layout order: `.text` (RX), `.rodata` (R), `.data` (RW), `.bss` (RW, no file bytes); align each segment start to 0x200000 (2 MB, matches kernel huge-page alignment); compute `e_phnum` and `e_shnum`
- [ ] 5.2 Write complete ELF64 binary: header, program headers (`PT_PHDR`, `PT_INTERP`, two `PT_LOAD` for RX and RW, `PT_DYNAMIC`, `PT_GNU_STACK` with `PF_R|PF_W` — no execute bit), section data in file-offset order, section headers at end; write `.symtab`, `.strtab`, `.shstrtab`, `.dynstr`, `.dynsym` with correct `sh_link`/`sh_info` cross-references
- [ ] 5.3 Shared object output (`-shared` flag): set `e_type = ET_DYN`, load base = 0 (position-independent), emit `DT_SONAME` in `.dynamic` from `-soname` argument; export all `STB_GLOBAL` non-hidden symbols in `.dynsym`; the resulting `.so` must be loadable by `dlopen` and usable as a `-l` argument to subsequent link invocations

## Phase 6: Testing (2 items)

- [ ] 6.1 Test: `tml-link hello.o /usr/lib/x86_64-linux-gnu/libc.so.6 -o hello` produces a valid ELF64 executable on Linux x86-64; verify structure with `readelf -a hello`; run binary and confirm correct exit code; check that `ldd hello` shows the correct shared library dependencies
- [ ] 6.2 Integration: replace LLD invocation in `lld_linker.cpp` on Linux with `tml-link`; run the full TML test suite on Linux; all tests that passed before continue to pass; LLD is no longer invoked on Linux

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
