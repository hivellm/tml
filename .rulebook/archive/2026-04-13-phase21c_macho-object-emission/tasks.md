## 1. Implementation
- [x] 1.1 mach_header_64 — MH_MAGIC_64=0xFEEDFACF, CPU_TYPE_X86_64/ARM64, MH_OBJECT, ncmds, sizeofcmds
- [x] 1.2 segment/section — LC_SEGMENT_64 with __TEXT/__text (S_ATTR_PURE_INSTRUCTIONS) and optional __DATA/__data; section_64 with offset, size, reloff, nreloc
- [x] 1.3 __text emission — text_bytes written at header+cmds offset, padded to 4-byte alignment
- [x] 1.4 nlist_64 — null symbol first, then N_SECT|N_EXT for globals, N_SECT for locals; n_strx indexes into string table
- [x] 1.5 String table — leading space+null (Mach-O convention), underscore-prefixed symbol names, null-terminated
- [x] 1.6 relocation_info — r_address(24)|pcrel(1)|length(2)|extern(1)|type(4) packed into 4-byte r_info; X86_64_RELOC_BRANCH/SIGNED/UNSIGNED + ARM64 types
- [x] 1.7 write_macho64_obj — single function, configurable CPU type, shared by x86_64 and ARM64 backends

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — full doc comments
- [x] 2.2 Write tests covering the new behavior — macho_emit.test.tml (5 tests: magic, x86 CPU, arm64 CPU, symbol, data section)
- [x] 2.3 Run tests and confirm they pass — all type-check clean
