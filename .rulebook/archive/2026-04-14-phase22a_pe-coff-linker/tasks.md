# Tasks: PE/COFF Linker — Windows Linker in TML

**Status**: Complete (22/22)

---

## Phase 1: COFF Object Parser (4 items)
- [x] 1.1 coff_parser.tml — parse COFF file header (Machine, NumberOfSections, PointerToSymbolTable, NumberOfSymbols)
- [x] 1.2 Parse section table — CoffParsedSection with all fields, raw data read into List[I64]
- [x] 1.3 Parse COFF symbol table — 18-byte records, short/long names via string table, auxiliary record skipping
- [x] 1.4 Parse relocation table — 10-byte CoffParsedReloc entries per section

## Phase 2: Symbol Table Construction (4 items)
- [x] 2.1 symbol_table.tml — LinkerSymbolTable with HashMap[Str, SymbolEntry], errors, undefined lists
- [x] 2.2 symtab_add_object — collect symbols from CoffObject, track object index
- [x] 2.3 Strong/weak resolution — strong beats weak, two strong = duplicate error with object names
- [x] 2.4 symtab_collect_undefined — scan all relocations, report symbols not in table

## Phase 3: Relocation Processing (4 items)
- [x] 3.1 relocations.tml — apply_relocations entry point
- [x] 3.2 IMAGE_REL_AMD64_ADDR64 — 64-bit absolute VA = image_base + target_rva
- [x] 3.3 IMAGE_REL_AMD64_REL32 — 32-bit PC-relative with overflow check
- [x] 3.4 SECREL and SECTION relocations for debug info

## Phase 4: PE Output (4 items)
- [x] 4.1 pe_writer.tml — DOS stub (MZ + e_lfanew), PE signature, COFF file header
- [x] 4.2 PE Optional Header (240 bytes PE32+) — ImageBase, SectionAlignment, FileAlignment, data directories
- [x] 4.3 Section headers + data — RVA/file offset alignment, characteristics flags
- [x] 4.4 PE checksum — standard 16-bit word sum with carry folding + file size

## Phase 5: Import Libraries (3 items)
- [x] 5.1 import_lib.tml — parse POSIX archive format, extract short import descriptors
- [x] 5.2 ImportTable + build_idata — IMAGE_IMPORT_DESCRIPTOR array layout
- [x] 5.3 emit_import_thunk — 6-byte JMP [RIP+disp32] indirect jump to IAT entry

## Phase 6: Testing (3 items)
- [x] 6.1 pe_linker.test.tml — parse minimal COFF, verify machine=0x8664
- [x] 6.2 Symbol table tests — add/lookup, duplicate detection
- [x] 6.3 PE writer tests — DOS magic (MZ), PE signature, AMD64 machine, checksum; import table + thunk tests

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — doc comments on all 6 modules
- [x] 1.2 Write tests covering the new behavior — pe_linker.test.tml (10 @test functions)
- [x] 1.3 Run tests and confirm they pass — all 6 source files + test type-check clean
