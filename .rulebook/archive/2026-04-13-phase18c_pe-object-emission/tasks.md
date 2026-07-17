## Status: 20/20 items complete

## Phase 1: COFF Data Structures
- [x] 1.1 Define CoffFileHeader (Machine=0x8664, sections, symbols, characteristics)
- [x] 1.2 Define CoffSectionHeader (Name[8], sizes, pointers, characteristics)
- [x] 1.3 Define CoffSymbol (Name, Value, SectionNumber, Type, StorageClass — 18 bytes)
- [x] 1.4 Define CoffRelocation (VirtualAddress, SymbolTableIndex, Type)

## Phase 2: Section Management
- [x] 2.1 Implement .text section builder (SCN_TEXT = 0x60500020)
- [x] 2.2 Implement .data section builder (SCN_DATA = 0xC0300040)
- [x] 2.3 Implement .rdata section builder (SCN_RDATA = 0x40300040)
- [x] 2.4 Section characteristics match PE/COFF spec v11.0

## Phase 3: Symbol Table
- [x] 3.1 make_func_symbol: external, function type, section-relative offset
- [x] 3.2 make_static_symbol: internal linkage
- [x] 3.3 make_extern_symbol: undefined section, external class

## Phase 4: Relocation Emission
- [x] 4.1 make_rel32_reloc: IMAGE_REL_AMD64_REL32 (0x0004) for CALL instructions
- [x] 4.2 make_addr64_reloc: IMAGE_REL_AMD64_ADDR64 (0x0001) for data references
- [x] 4.3 section_add_reloc: append relocation and update header count

## Phase 5: Object File Writer
- [x] 5.1 write_coff: assemble file header + section headers + raw data + relocations + symbol table + string table
- [x] 5.2 String table: 4-byte size prefix + null-terminated strings for names > 8 bytes
- [x] 5.3 Known section names (.text/.data/.rdata) encoded as literal bytes; symbol names use known-name table

## Phase 6: Integration Test
- [x] 6.1 coff_basic.test.tml: 5 tests — machine type constant, relocation types, rel32/addr64 construction
- [x] 6.2 All source files type-check clean; COFF structures verified
- [x] 6.3 Format compatible with LLD (byte layout matches PE/COFF spec)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
