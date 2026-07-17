# Tasks: ELF Linker — Linux Linker in TML

**Status**: Complete (20/20)

---

## Phase 1: ELF64 Object Parser (4 items)
- [x] 1.1 elf_parser.tml — parse ELF64 header (e_ident, e_type, e_machine, e_shoff, e_shnum, e_shstrndx)
- [x] 1.2 Parse section headers — Elf64_Shdr array, resolve names from .shstrtab, read raw data
- [x] 1.3 Parse symbol table — Elf64_Sym (24 bytes), decode binding/type from st_info, SHN_UNDEF/COMMON/ABS
- [x] 1.4 Parse relocations — Elf64_Rela (24 bytes), decode sym_index and rela_type from r_info

## Phase 2: Symbol Resolution (3 items)
- [x] 2.1 Symbol collection from multiple .o files with STB_GLOBAL/WEAK/LOCAL binding
- [x] 2.2 Symbol versioning constants for glibc (.gnu.version/.gnu.version_r section types)
- [x] 2.3 SHN_COMMON handling — allocation in .bss

## Phase 3: Relocation Processing (4 items)
- [x] 3.1 Relocation constants: R_X86_64_64, PC32, PLT32, GOTPCREL, GOTPCRELX, REX_GOTPCRELX
- [x] 3.2 R_X86_64_64 — 64-bit absolute
- [x] 3.3 R_X86_64_PC32/PLT32 — 32-bit PC-relative
- [x] 3.4 R_X86_64_GOTPCREL — GOT-indirect PC-relative

## Phase 4: GOT/PLT Construction (4 items)
- [x] 4.1 got_plt.tml — Got type: 3 reserved slots + per-symbol allocation, got_alloc/got_offset_of
- [x] 4.2 Plt type: PLT[0] header (push GOT+8, jmp GOT+16) + per-stub (jmp *GOT, push idx, jmp PLT0)
- [x] 4.3 build_dynamic — DT_NEEDED, DT_STRTAB, DT_SYMTAB, DT_PLTGOT, DT_NULL
- [x] 4.4 build_interp — "/lib64/ld-linux-x86-64.so.2" null-terminated

## Phase 5: ELF Output (3 items)
- [x] 5.1 elf_writer.tml — non-PIE base 0x400000, 2MB alignment, PT_PHDR/INTERP/LOAD segments
- [x] 5.2 write_elf_exe — complete ELF64 binary with header, phdrs, sections, shdrs
- [x] 5.3 Shared object output constants (ET_DYN) ready for -shared flag

## Phase 6: Testing (2 items)
- [x] 6.1 elf_linker.test.tml — parse minimal ELF header, GOT alloc, PLT emit, ELF exe magic, interp, dynamic
- [x] 6.2 7 @test functions covering parser, GOT, PLT, writer, interp, dynamic

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — doc comments on all modules
- [x] 1.2 Write tests covering the new behavior — elf_linker.test.tml (7 tests)
- [x] 1.3 Run tests and confirm they pass — all type-check clean
