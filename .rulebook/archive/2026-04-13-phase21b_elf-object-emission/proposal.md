# Proposal: phase21b_elf-object-emission

## Why
The native backend currently produces object files only in COFF format (Windows PE).
Linux and BSD systems use ELF64 as their object format. Without ELF emission, the
native x86-64 backend is Windows-only and cannot link against system libraries on
any POSIX platform. Adding ELF support unlocks the native backend for the majority
of server and CI environments where TML programs are deployed.

## What Changes
- New file `compiler-tml/src/native/elf_emit.tml` implementing the full ELF64
  object file writer: header (e_ident magic, ET_REL, EM_X86_64), section header
  table, .text, .symtab, .strtab, and .rela.text sections with correct relocation
  entries (R_X86_64_PC32 for data references, R_X86_64_PLT32 for function calls).
- The writer accepts the same `NativeModule` IR already produced by `pipeline.tml`
  and serialises it to a byte buffer that can be written directly to a `.o` file.
- Platform detection in `pipeline.tml` to route Linux/BSD targets to `elf_emit`
  instead of the existing COFF writer.

## Impact
- Affected specs: native-backend/object-emission
- Affected code: compiler-tml/src/native/elf_emit.tml (new), compiler-tml/src/native/pipeline.tml (routing)
- Breaking change: NO
- User benefit: `tml build` produces valid `.o` files on Linux/BSD that link with `ld` or `clang`.
