# Tasks: PE/COFF Linker — Windows Linker in TML

**Status**: Planned (0/22)
**Depends on**: phase18c (PE object emission — .obj files produced by TML backend)
**Blocks**: phase22d (incremental linker needs a working PE linker)
**Duration**: 5–6 weeks
**Risk**: Medium — PE/COFF spec is well-documented; import library handling is the hardest part

---

## Phase 1: COFF Object Parser (4 items)

- [ ] 1.1 Create `compiler-tml/src/link/pe/coff_parser.tml` — parse COFF file header (`Machine`, `NumberOfSections`, `PointerToSymbolTable`, `NumberOfSymbols`); return `CoffObject` struct
- [ ] 1.2 Parse section table: for each section header emit `CoffSection` with `Name`, `VirtualSize`, `VirtualAddress`, `SizeOfRawData`, `PointerToRawData`, `Characteristics`; read raw section data into `Buffer`
- [ ] 1.3 Parse COFF symbol table: iterate all symbol records (18 bytes each), handle auxiliary records (section definition, function definition, weak externals), collect into `List[CoffSymbol]`
- [ ] 1.4 Parse relocation table per section: read `CoffRelocation` entries (`VirtualAddress`, `SymbolTableIndex`, `Type`); store per-section as `List[CoffRelocation]`

## Phase 2: Symbol Table Construction (4 items)

- [ ] 2.1 Create `compiler-tml/src/link/pe/symbol_table.tml` — `SymbolTable` type holding `HashMap[Str, SymbolEntry]`; `SymbolEntry` records name, section, offset, size, storage class, weak/strong
- [ ] 2.2 Collect symbols from all input `.obj` files: iterate `CoffSymbol` list, insert into `SymbolTable`; track which object file defined each symbol
- [ ] 2.3 Resolve strong vs weak symbols: if a strong definition exists, weak definitions are discarded; if only weak definitions exist, use the first one; if two strong definitions exist, emit duplicate-symbol error with both object file names
- [ ] 2.4 Detect and report undefined symbols: after all objects processed, any symbol referenced in relocations but absent from `SymbolTable` is an error; collect all missing names before reporting

## Phase 3: Relocation Processing (4 items)

- [ ] 3.1 Create `compiler-tml/src/link/pe/relocations.tml` — `apply_relocations(section: mut ref CoffSection, syms: ref SymbolTable, base_rva: I64)` entry point
- [ ] 3.2 Implement `IMAGE_REL_AMD64_ADDR64` (type 0x0001): write 64-bit absolute virtual address of target symbol into section data at relocation offset
- [ ] 3.3 Implement `IMAGE_REL_AMD64_REL32` (type 0x0004): write 32-bit signed PC-relative offset; value = target_rva - (reloc_rva + 4); verify offset fits in I32 else emit relocation-overflow error
- [ ] 3.4 Implement section-relative relocations `IMAGE_REL_AMD64_SECREL` (type 0x000B) and `IMAGE_REL_AMD64_SECTION` (type 0x000A) required for debug info sections

## Phase 4: PE Output (4 items)

- [ ] 4.1 Create `compiler-tml/src/link/pe/pe_writer.tml` — write DOS stub (64 bytes, `MZ` signature, `e_lfanew` = 0x40), PE signature (`PE\0\0`), COFF file header (`Machine` = 0x8664 for AMD64, section count, timestamp, symbol table pointer = 0)
- [ ] 4.2 Write PE Optional Header (240 bytes for PE32+): `Magic` = 0x020B, linker version, `SizeOfCode`, `SizeOfInitializedData`, `AddressOfEntryPoint`, `ImageBase` = 0x140000000, `SectionAlignment` = 0x1000, `FileAlignment` = 0x200, subsystem, DLL characteristics, stack/heap sizes, data directory array (16 entries)
- [ ] 4.3 Write section headers and section data: assign RVAs (4KB aligned), file offsets (512-byte aligned), set `Characteristics` flags (`IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ` for `.text`); write raw data padded to `FileAlignment`
- [ ] 4.4 Compute and write PE checksum in optional header (`CheckSum` field): use standard PE checksum algorithm (sum all 16-bit words, add file size, handle carries); write final binary to output path

## Phase 5: Import Libraries (3 items)

- [ ] 5.1 Create `compiler-tml/src/link/pe/import_lib.tml` — parse `.lib` archive format (Archive signature `!<arch>\n`, member headers); extract import descriptors containing DLL name, symbol name, hint
- [ ] 5.2 Construct IAT (Import Address Table) and ILT (Import Lookup Table) for each imported DLL: build `IMAGE_IMPORT_DESCRIPTOR` array in `.idata` section; write thunks to IAT entries; null-terminate both ILT and IAT arrays
- [ ] 5.3 Generate import stub functions in `.text` for each imported symbol: `jmp [IAT_entry]` indirect jump; expose the symbol name so relocations to it resolve to the stub

## Phase 6: Testing (3 items)

- [ ] 6.1 Test: `tml-link hello.obj -o hello.exe` (single object, no imports) produces a valid PE32+ executable; verify with `dumpbin /headers` and by running the binary
- [ ] 6.2 Test: `tml-link foo.obj bar.obj kernel32.lib -o foo.exe` (multiple objects, Windows API imports) links successfully; binary runs and calls `ExitProcess` correctly
- [ ] 6.3 Integration: replace `LLDLinker::link_windows()` call in `compiler/src/backend/lld_linker.cpp` with a call to the TML `tml-link` binary; all existing Windows test binaries still pass
