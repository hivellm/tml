## 1. Implementation
- [ ] 1.1 Define ELF64 header constants and structs (e_ident 16-byte magic, ET_REL=1, EM_X86_64=62, EV_CURRENT=1, Elf64_Ehdr layout)
- [ ] 1.2 Implement section header table (Elf64_Shdr, sh_type SHT_NULL/PROGBITS/SYMTAB/STRTAB/RELA, sh_flags SHF_ALLOC/EXECINSTR)
- [ ] 1.3 Emit .text section from NativeModule instruction bytes with correct sh_offset and sh_size
- [ ] 1.4 Emit .symtab section: STB_LOCAL for file/section symbols, STB_GLOBAL STT_FUNC for exported functions (Elf64_Sym layout)
- [ ] 1.5 Emit .strtab section: null-terminated symbol name strings, index tracking per symbol
- [ ] 1.6 Emit .rela.text section: R_X86_64_PC32 for data refs, R_X86_64_PLT32 for function calls (Elf64_Rela with r_offset/r_info/r_addend)
- [ ] 1.7 Wire ElfEmitter.emit() to produce complete .o byte buffer and add platform routing in pipeline.tml (Linux/BSD → elf_emit)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
