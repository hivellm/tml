## 1. Implementation
- [ ] 1.1 Define Mach-O 64-bit header constants and mach_header_64 struct (magic MH_MAGIC_64=0xFEEDFACF, cputype CPU_TYPE_X86_64=0x01000007, filetype MH_OBJECT=1)
- [ ] 1.2 Define segment/section structs: segment_command_64, section_64 with __TEXT/__text names, flags S_REGULAR|S_ATTR_PURE_INSTRUCTIONS
- [ ] 1.3 Emit __text section bytes from NativeModule with correct offset accounting for header + load commands
- [ ] 1.4 Emit nlist_64 symbol table: n_type N_EXT|N_SECT for exported symbols, N_SECT for locals, n_strx into string table
- [ ] 1.5 Emit null-terminated string table with leading null byte (index 0 = empty name convention)
- [ ] 1.6 Emit relocation entries (relocation_info): r_address, r_symbolnum, r_pcrel, r_length=2, r_extern, r_type X86_64_RELOC_BRANCH for calls
- [ ] 1.7 Wire MachoEmitter.emit() to produce complete .o byte buffer and add darwin/macos routing in pipeline.tml

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
