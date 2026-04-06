# Proposal: Mach-O Linker — macOS Linker in TML

**Task**: phase22c_macho-linker
**Status**: Planned
**Priority**: P0
**Estimated effort**: 3–4 weeks
**Risk**: Medium
**LOC estimate**: ~4,800 LOC TML

## Problem

The TML compiler uses LLD to produce Mach-O executables on macOS. Mach-O is the third and
final object format used by TML's three-platform support. Eliminating LLD from the macOS
build requires implementing a Mach-O linker in TML that handles macOS-specific requirements:
two-level namespaces, dyld chained fixups (macOS 12+), and mandatory ad-hoc code signing
(required on Apple Silicon and on Intel since macOS Monterey 12.3 for certain operations).

## Proposed Solution

Implement Mach-O linking support in `tml-link`, adding macOS as a third target alongside
the existing PE/COFF (phase 22a) and ELF (phase 22b) implementations.

**Mach-O 64-bit parser** — Mach-O files begin with a 32-byte header followed by load
commands. The parser iterates load commands to find `LC_SEGMENT_64` (containing sections),
`LC_SYMTAB` (symbol/string table locations), and relocation data embedded in the segment.
Unlike ELF, there is no separate section header table — all metadata is in load commands.

**Two-level namespace** — macOS Mach-O uses a two-level symbol namespace: each undefined
symbol records which dylib it comes from (as a library ordinal). This is more efficient
than ELF's flat namespace at load time but requires the linker to track which dylib exports
which symbol and assign ordinals consistently.

**dyld chained fixups (macOS 12+)** — Modern macOS uses `LC_DYLD_CHAINED_FIXUPS` instead
of the older `LC_DYLD_INFO_ONLY` bind/lazy bind opcode streams. Chained fixups embed
rebase and bind information directly in the data pages as a linked list of 64-bit slots,
which `dyld` processes at load time. This is the required format for Apple Silicon
binaries targeting macOS 12+.

**Code signing** — Since macOS Catalina (10.15), binaries must be code-signed to run.
For development and CI purposes, ad-hoc signing (signing with your own key without an
Apple Developer account) is sufficient. Ad-hoc signing requires computing a `CodeDirectory`
blob containing SHA-256 hashes of every 4KB page of the `__TEXT` segment and writing an
`LC_CODE_SIGNATURE` load command pointing to it. Getting the `CodeDirectory` fields wrong
(wrong version, wrong `pageSize`, wrong hash count) causes the kernel to kill the process
with `SIGKILL` immediately on launch.

**Universal binaries (fat Mach-O)** — The fat binary format wraps multiple Mach-O
binaries (one per architecture) with a common header. This is required to produce a
single `.tml` binary that runs natively on both Intel Macs and Apple Silicon.

Target: ~4,800 LOC TML across seven files.

## Key Decisions

- **Target macOS 12+ (Monterey) as minimum** — This justifies implementing chained fixups
  rather than the older `LC_DYLD_INFO_ONLY` opcode format. Chained fixups are simpler to
  generate (just fill in the chain pointer fields in the data section) and are the
  documented-forward path.

- **Ad-hoc code signing, not notarization** — Notarization requires Apple Developer
  credentials and internet access. Ad-hoc signing is sufficient for running locally and
  in CI. Distribution-quality signing with a real key can be added later.

- **SHA-256 hashes use `std::crypto::sha256`** — The SHA-256 implementation from
  `lib/std/src/crypto/sha256.tml` is used for page hashing. No C crypto library is needed.

- **`LC_CODE_SIGNATURE` must be the last load command** — Apple's tools enforce this.
  The signature blob's size must be estimated before writing the binary because the
  `__TEXT` segment size (and thus page count) determines the number of hash slots.

- **Reference: cctools-port and ld64** — Apple's open-source `ld64` linker (available via
  the `cctools-port` project on GitHub) is the authoritative reference for Mach-O output.
  The `zld` linker (fast drop-in replacement for ld64) and `mold` are also useful references.

## Files to Create

| File | LOC | Purpose |
|------|-----|---------|
| `compiler-tml/src/link/macho/macho_parser.tml` | ~900 | Parse Mach-O 64-bit object files |
| `compiler-tml/src/link/macho/symbol_table.tml` | ~600 | Two-level namespace symbol resolution |
| `compiler-tml/src/link/macho/macho_writer.tml` | ~1,400 | Emit Mach-O 64-bit binary |
| `compiler-tml/src/link/macho/codesign.tml` | ~500 | Ad-hoc LC_CODE_SIGNATURE |
| `compiler-tml/src/link/macho/fat.tml` | ~300 | Universal fat Mach-O writer |
| `compiler-tml/src/link/macho/mod.tml` | ~400 | Module root, CLI entry point |
| `compiler-tml/src/link/macho/error.tml` | ~300 | Typed linker errors |

## Success Criteria

- `tml-link hello.o -o hello -lSystem` produces a valid Mach-O 64-bit executable on macOS.
- `otool -l hello` shows correct `LC_SEGMENT_64`, `LC_MAIN`, `LC_CODE_SIGNATURE` commands.
- `codesign -v hello` reports "valid on disk" and "satisfies its Designated Requirement".
- Binary runs correctly on both x86-64 and Apple Silicon (ARM64).
- `tml-link --arch x86_64 --arch arm64 -o tml.out` produces a fat binary verified by
  `lipo -info tml.out`.
- All existing TML tests pass on macOS when `lld_linker.cpp` is switched to `tml-link`.
  LLD is no longer invoked on macOS.

## Dependencies

- **Depends on**: phase18c — object file emission from the TML x86-64 backend.
- **Depends on**: phase22a, phase22b — shared linker infrastructure (symbol table types,
  error types, `Buffer`-based binary writing patterns).
- **Blocks**: phase22d — incremental linker needs a working base linker for all platforms.
- **Reference**: Apple `ld64` source (cctools-port), Mach-O Reference Manual, Apple Silicon
  `dyld` source, `zld` linker, `codesign` man page and SecCodeRef internals.
