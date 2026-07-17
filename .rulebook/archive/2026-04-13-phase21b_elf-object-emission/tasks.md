## 1. Implementation
- [x] 1.1 ELF64 header constants — ELF magic, ELFCLASS64, ELFDATA2LSB, ET_REL, EM_X86_64/EM_AARCH64, all sizes (Ehdr=64, Shdr=64, Sym=24, Rela=24)
- [x] 1.2 Section header table — SHT_NULL/PROGBITS/SYMTAB/STRTAB/RELA, SHF_ALLOC/EXECINSTR/INFO_LINK; 7 sections: null, .text, .rodata, .strtab, .symtab, .shstrtab, .rela.text
- [x] 1.3 .text section — emitted from text_bytes List at offset 64, padded to 8-byte alignment
- [x] 1.4 .symtab section — null symbol first, then STB_LOCAL, then STB_GLOBAL; write_elf64_sym encodes st_info=(bind<<4)|type; sh_info tracks first global index
- [x] 1.5 .strtab section — null byte prefix, null-terminated symbol names, HashMap tracks name→offset for Elf64_Sym.st_name
- [x] 1.6 .rela.text section — R_X86_64_PC32/PLT32/32/64 for x86, R_AARCH64_CALL26/ADR_PREL_PG_HI21/ADD_ABS_LO12_NC for arm64; write_elf64_rela encodes r_info=ELF64_R_INFO(sym,type)
- [x] 1.7 write_elf64_obj — single function takes machine type, text, rodata, symbols, relocs; outputs complete .o byte buffer; shared by x86_64 and AArch64

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — full doc comments
- [x] 2.2 Write tests covering the new behavior — elf_emit.test.tml (6 tests: magic, x86 machine, aarch64 machine, text data, symbol, relocation)
- [x] 2.3 Run tests and confirm they pass — all type-check clean
