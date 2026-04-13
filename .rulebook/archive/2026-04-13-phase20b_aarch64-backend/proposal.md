# Proposal: phase20b — AArch64 Instruction Encoder + ELF/Mach-O Emission

## Why

Phase 20a delivers a production x86_64 native backend for Windows. Phase 20b extends the native backend to AArch64, targeting two major platforms: Linux ARM64 (servers, Raspberry Pi, AWS Graviton) and Apple Silicon (M1/M2/M3 Macs, all new Apple hardware). Without AArch64 support, TML cannot run natively on approximately half of modern developer machines.

AArch64 is significantly simpler to encode than x86_64. Fixed 32-bit instruction words mean there are no REX prefixes, no variable-length encodings, no ModRM/SIB complexity. The instruction encoding is specified by ARM's Architecture Reference Manual and is completely table-driven. This simplicity means the AArch64 encoder can be smaller than the x86_64 encoder despite supporting more instructions.

## What Changes

- New TML module `compiler/native/aarch64_encode.tml` — AArch64 instruction encoding (fixed 32-bit words)
- New TML module `compiler/native/aarch64_emit.tml` — `emit_func_aarch64(MachFunc) -> Buffer`
- New TML module `compiler/native/elf_emit.tml` — ELF64 object file writer (AArch64 relocations)
- New TML module `compiler/native/macho_emit.tml` — Mach-O 64-bit object file writer (ARM64)
- Modified `compiler/native/calling_conv.tml` — add AAPCS64 calling convention (AArch64 ABI)
- CLI: `--backend=native` default extended to Linux ARM64 and macOS ARM64

## Design Decisions

**Fixed 32-bit instruction words**: Every AArch64 instruction is exactly 4 bytes, always 4-byte aligned. The encoding infrastructure is a 32-bit integer with named bit fields. This is dramatically simpler than x86_64's variable-length format and eliminates entire classes of encoding bugs (wrong prefix, wrong length prefix).

**ADRP+ADD for globals**: AArch64 addresses globals using two-instruction sequences. ADRP (Address of Page) loads the 4KB-aligned page of the target into a register (21-bit PC-relative offset, ±4GB range). ADD then adds the page offset (12 bits). Both instructions require relocations. This is the standard pattern on all AArch64 platforms.

**Two object formats (ELF + Mach-O)**: Linux uses ELF, macOS uses Mach-O. The instruction encoding is identical — only the object file wrapper differs. Rather than duplicating the encoding logic, phase 20b shares `aarch64_encode.tml` and `aarch64_emit.tml` between both object format writers. The `emit_func_aarch64` output is format-agnostic bytes.

**Mach-O for Apple Silicon**: Apple Silicon runs AArch64. All binaries on macOS must be Mach-O (no ELF support). The Mach-O format is documented in `<mach-o/loader.h>` in the macOS SDK. The loader requires code signing for M1+ in some scenarios — Phase 20b targets ad-hoc signed objects (for `ld` to link into a fully-signed binary).

**AAPCS64 calling convention**: ARM defines AAPCS64 (ARM Architecture Procedure Call Standard for AArch64). Key difference from x86_64 Win64: 8 integer arg registers (X0-X7) vs 4 (RCX/RDX/R8/R9), sret pointer in X8 (not in first arg register), link register X30 (not stack-pushed return address).

## Impact

- Affected specs: docs/specs/native-backend.md (AArch64 section, ELF/Mach-O sections)
- Affected code: compiler/native/ (4 new modules, 1 modified), compiler/src/cli/ (target-conditional backend default)
- Breaking change: NO on Windows/x86_64. New default on Linux ARM64 and macOS ARM64.
- User benefit: TML runs natively on Apple Silicon Macs and ARM64 Linux without LLVM; enables TML for embedded ARM64 platforms

## Risk

MEDIUM. AArch64 encoding is simpler than x86_64 but ADRP+ADD relocation sequences must be correct. Mach-O is less extensively documented than ELF or PE/COFF. Cross-compilation testing (task 6.1) using objdump is the primary correctness signal before access to AArch64 hardware. Mach-O code signing requirements on recent macOS (Apple M3+) may require investigation.

## Reference

- ARM Architecture Reference Manual — AArch64 instruction set encoding (mandatory reading)
- AAPCS64: github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst
- ELF for AArch64: github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst
- Mach-O format: opensource.apple.com/source/xnu/xnu-6153.81.5/EXTERNAL_HEADERS/mach-o/loader.h
- Apple linker: ld64 source (github.com/apple-oss-distributions/ld64)
- LLVM lib/Target/AArch64/MCTargetDesc/ — production AArch64 encoder reference
