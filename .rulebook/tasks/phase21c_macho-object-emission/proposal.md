# Proposal: phase21c_macho-object-emission

## Why
macOS uses the Mach-O binary format exclusively; the system linker (`ld64`) and
`dyld` will not accept ELF or COFF objects. Without Mach-O emission the native
backend cannot produce binaries on Apple hardware, cutting off the large macOS
developer population and Apple Silicon machines. Mach-O support completes
cross-platform native object emission alongside COFF (Windows) and ELF (Linux).

## What Changes
- New file `compiler-tml/src/native/macho_emit.tml` implementing a Mach-O 64-bit
  relocatable object writer: mach_header_64 (MH_OBJECT, CPU_TYPE_X86_64,
  CPU_SUBTYPE_ALL), LC_SEGMENT_64 load command with a single __TEXT/__text
  section, nlist_64 symbol table (N_EXT | N_SECT for globals), string table, and
  X86_64_RELOC_BRANCH / X86_64_RELOC_SIGNED relocation entries.
- Platform routing in `pipeline.tml` to select `macho_emit` when the target
  triple contains `darwin` or `macos`.

## Impact
- Affected specs: native-backend/object-emission
- Affected code: compiler-tml/src/native/macho_emit.tml (new), compiler-tml/src/native/pipeline.tml (routing)
- Breaking change: NO
- User benefit: `tml build` produces valid Mach-O `.o` files on macOS linkable with `clang` or `ld64`.
