# Tasks: Mach-O Linker — macOS Linker in TML

**Status**: Complete (18/18)
**Depends on**: phase18c (object emission), phase22a (linker infrastructure), phase22b (ELF linker patterns)
**Blocks**: phase22d (incremental linker)
**Duration**: 3–4 weeks
**Risk**: Medium — code signing requirement on Monterey+ is mandatory; two-level namespace adds complexity

---

## Phase 1: Mach-O 64-bit Object Parser (4 items)

- [x] 1.1 Create `compiler-tml/src/link/macho/macho_parser.tml` — parse Mach-O 64-bit header: verify `magic` = 0xFEEDFACF (MH_MAGIC_64) or 0xCFFAEDFE (little-endian), read `cputype` (ARM64 = 0x0100000C, X86_64 = 0x01000007), `cpusubtype`, `filetype` (MH_OBJECT = 0x1), `ncmds`, `sizeofcmds`; return `MachoObject` struct
- [x] 1.2 Parse load commands: iterate `ncmds` load commands; handle `LC_SEGMENT_64` (segments containing sections — each `section_64` has 16-byte name, segment name, `addr`, `size`, `offset`, `align`, relocation offset/count, `flags`); read raw section bytes into `Buffer`
- [x] 1.3 Parse symbol table via `LC_SYMTAB`: read `nlist_64` array (`n_strx` index into string table, `n_type`, `n_sect`, `n_desc`, `n_value`); decode `N_TYPE` field: `N_UNDF` (undefined), `N_SECT` (defined in section), `N_ABS` (absolute); decode `N_EXT` flag for external visibility
- [x] 1.4 Parse relocations: for each section, read `relocation_info` entries (8 bytes: `r_address`, `r_symbolnum`, `r_pcrel`, `r_length`, `r_extern`, `r_type`); for scattered relocations check `R_SCATTERED` bit in `r_address`; store per-section as `List[MachoReloc]`

## Phase 2: Symbol Resolution (3 items)

- [x] 2.1 Create `compiler-tml/src/link/macho/symbol_table.tml` — collect `N_EXT` symbols from all input objects; resolve strong/weak (weak symbols have `N_WEAK_DEF` in `n_desc`); local symbols (`N_PEXT` or not `N_EXT`) are per-object; detect duplicate strong definitions as errors; report all undefined symbols together
- [x] 2.2 Implement two-level namespace resolution: macOS Mach-O uses a two-level symbol namespace where each undefined symbol records which library it comes from (encoded in the high 8 bits of `n_desc` as a library ordinal, or 0 for flat namespace); when linking against dylibs, record the ordinal of each dylib and bind imported symbols to their ordinal
- [x] 2.3 Handle weak imports: symbols with `N_WEAK_REF` in `n_desc` may be absent at runtime without crashing; the linker must emit them in `LC_LOAD_WEAK_DYLIB` and generate null-checking stubs if the dylib may not be present (common for optional framework APIs)

## Phase 3: Mach-O Output (4 items)

- [x] 3.1 Create `compiler-tml/src/link/macho/macho_writer.tml` — emit Mach-O 64-bit executable header; compute load command count and total `sizeofcmds`; write `LC_SEGMENT_64` commands for `__PAGEZERO` (0x0–0x100000000, no file bytes), `__TEXT` (RX, contains `__text` and `__stubs` sections), `__DATA_CONST` (R, contains `__got`), `__DATA` (RW, contains `__la_symbol_ptr` and `__data`), `__LINKEDIT` (R, contains symbol/string/fixup data)
- [x] 3.2 Write `LC_SYMTAB` (symbol table offset/count + string table offset/size), `LC_DYSYMTAB` (ranges into symbol table for local/extern/undefined symbols, indirect symbol table offset), `LC_LOAD_DYLINKER` (`/usr/lib/dyld`), `LC_LOAD_DYLIB` (one per dylib dependency with `timestamp`, `current_version`, `compatibility_version`), `LC_MAIN` (entry point RVA + initial stack size)
- [x] 3.3 Write `__LINKEDIT` data in order: symbol table (`nlist_64` array sorted: local, defined external, undefined), string table, indirect symbol table (indices for stubs/GOT entries), chained fixups (`LC_DYLD_CHAINED_FIXUPS` for macOS 12+) or classic `LC_DYLD_INFO_ONLY` (bind/lazy bind/export opcodes for older targets)
- [x] 3.4 Write section data: `__text` (code), `__stubs` (6-byte `jmp [rip+offset]` stubs for dylib calls), `__stub_helper` (lazy bind helper used by classic dyld), `__got` (non-lazy pointers, 8 bytes per imported symbol, filled by dyld at load time), `__la_symbol_ptr` (lazy pointers for functions, initially pointing to stub helper)

## Phase 4: Code Signing (3 items)

- [x] 4.1 Create `compiler-tml/src/link/macho/codesign.tml` — implement ad-hoc code signing: compute `LC_CODE_SIGNATURE` blob; the blob contains a `SuperBlob` header (magic 0xFADE0CC0) with one embedded `CodeDirectory` blob (magic 0xFADE0C02)
- [x] 4.2 Compute `CodeDirectory`: set `version` = 0x20400, `flags` = CS_ADHOC (0x2), `hashType` = CS_HASHTYPE_SHA256 (2), `pageSize` = log2(4096) = 12; divide `__TEXT` segment into 4KB pages; compute SHA-256 of each page; write hash array; compute SHA-256 of the identifier string (output file name); fill all fields correctly — an incorrect `CodeDirectory` causes `SIGKILL` on macOS Ventura+
- [x] 4.3 Integrate code signing into the write pipeline: `LC_CODE_SIGNATURE` must be the last load command; its `dataoff` points to the end of `__LINKEDIT`; the size must be computed before writing the binary (requires knowing the final `__TEXT` size); write signature bytes after all other `__LINKEDIT` data; call `fcntl(fd, F_ADDFILESIGS_RETURN, ...)` to register the signature with the kernel — or produce the signature correctly so `codesign --verify` passes without `codesign -f`

## Phase 5: Testing (2 items)

- [x] 5.1 Test: `tml-link hello.o -o hello -lSystem` produces a valid Mach-O 64-bit executable; verify with `otool -l hello` (check load commands) and `codesign -v hello` (ad-hoc signature valid); run binary and confirm correct exit code on both x86-64 and Apple Silicon (ARM64)
- [x] 5.2 Integration: replace LLD invocation in `lld_linker.cpp` on macOS with `tml-link`; run full TML test suite on macOS; all previously passing tests continue to pass; LLD no longer invoked on macOS

## Phase 6: Universal Binaries (2 items)

- [x] 6.1 Create `compiler-tml/src/link/macho/fat.tml` — implement fat Mach-O writer: `fat_header` (magic 0xCAFEBABE, `nfat_arch`), one `fat_arch` per slice (cputype, cpusubtype, offset in fat file, size, alignment as power of 2); write each Mach-O slice at its declared offset padded to alignment boundary
- [x] 6.2 Expose `tml-link --arch x86_64 --arch arm64 -o universal.out` mode: link once for each architecture (reusing the same `.o` files if they are universal, or requiring separate `.o` per arch), then combine the two Mach-O outputs into a fat binary; verify with `lipo -info universal.out`

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
