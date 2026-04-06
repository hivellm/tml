## Status: 0/20 items complete

## Phase 1: COFF Data Structures
- [ ] 1.1 Define `CoffFileHeader` (Machine=0x8664, NumberOfSections, TimeDateStamp, SymbolTablePtr, NumberOfSymbols, OptionalHeaderSize=0, Characteristics)
- [ ] 1.2 Define `CoffSectionHeader` (Name[8], VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData, PointerToRelocations, NumberOfRelocations, Characteristics)
- [ ] 1.3 Define `CoffSymbol` (Name[8] or offset into string table, Value, SectionNumber, Type, StorageClass) — 18 bytes per entry
- [ ] 1.4 Define `CoffRelocation` (VirtualAddress, SymbolTableIndex, Type: IMAGE_REL_AMD64_ADDR64=0x0001, IMAGE_REL_AMD64_REL32=0x0004)

## Phase 2: Section Management
- [ ] 2.1 Implement `.text` section builder (executable, readable — characteristics 0x60500020) accepting raw bytes from phase18b
- [ ] 2.2 Implement `.data` section builder (read/write — 0xC0300040) for mutable globals and string data
- [ ] 2.3 Implement `.rdata` section builder (read-only — 0x40300040) for string literals and jump tables
- [ ] 2.4 Implement `.pdata` section builder (0x40300040) for unwind info (RUNTIME_FUNCTION entries — required for Windows SEH)

## Phase 3: Symbol Table
- [ ] 3.1 Emit function symbols: each TML function → COFF symbol (storage class = IMAGE_SYM_CLASS_EXTERNAL, section = .text)
- [ ] 3.2 Emit global data symbols: each TML global → COFF symbol (section = .data or .rdata)
- [ ] 3.3 Emit external reference symbols for each @extern("c") function called (section = 0, storage class = IMAGE_SYM_CLASS_EXTERNAL)

## Phase 4: Relocation Emission
- [ ] 4.1 Emit REL32 relocations for CALL instructions targeting external or cross-module functions
- [ ] 4.2 Emit ADDR64 relocations for global data references loaded via absolute 64-bit address
- [ ] 4.3 Record relocation entries in the section header's PointerToRelocations field

## Phase 5: Object File Writer
- [ ] 5.1 Implement `write_coff(sections, symbols, relocations) -> Buffer` — assemble all COFF components in the correct byte layout
- [ ] 5.2 Implement string table: symbol names longer than 8 bytes go into a string table appended after the symbol table
- [ ] 5.3 Write Buffer to `.obj` file on disk via existing file I/O API

## Phase 6: Integration Test
- [ ] 6.1 Compile `hello.tml` using `--backend=native`: MIR → MachIR → x86 bytes → COFF .obj → link via LLD → .exe
- [ ] 6.2 Verify the resulting `hello.exe` runs and produces correct output (exit code 0, "Hello, world!" printed)
- [ ] 6.3 Verify the .obj file is accepted by MSVC `link.exe` as well as LLD (cross-linker compatibility)
