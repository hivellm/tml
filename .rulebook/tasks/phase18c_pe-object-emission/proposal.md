# Proposal: phase18c — PE/COFF Object File Emission

## Why

Phases 18a and 18b produce x86_64 machine code bytes in memory. Phase 18c wraps those bytes in a PE/COFF object file so the existing LLD linker (already embedded in TML) can link them into an executable. This completes the Phase 18 MVP: a working end-to-end native backend path for Windows.

PE/COFF is well-documented (Microsoft PE/COFF specification, version 11.0) and the format is relatively simple for object files (as opposed to executable images). Object files do not need an Optional Header, a PE signature, or an import directory. They need only: a COFF file header, section headers, raw section data (.text, .data, .rdata), a symbol table, relocations, and a string table.

## What Changes

- New TML module `compiler/native/coff_emit.tml` — COFF file header, section header, symbol, relocation structures and writer
- New TML module `compiler/native/obj_writer.tml` — top-level `write_obj(MachModule) -> Buffer` that orchestrates all COFF components
- CLI: `--backend=native` flag (stubbed in 18a) now drives the full pipeline on Windows
- No changes to the LLVM backend

## Design Decisions

**Use LLD for linking (Phase 18)**: The existing LLD linker in `compiler/src/backend/lld_linker.cpp` accepts standard COFF .obj files. Phase 18c targets LLD compatibility. A custom linker (ERA 3) replaces LLD later. This means phase 18c gets a working end-to-end system immediately, without waiting for linker work.

**.pdata section (Windows SEH)**: Windows requires `.pdata` with RUNTIME_FUNCTION entries for any function that modifies RSP (i.e., every non-leaf function). Without .pdata, stack unwinding fails and C++ exceptions / debuggers cannot walk the stack. Phase 18c emits minimal RUNTIME_FUNCTION entries pointing to a trivial unwind code.

**Relocation types**: Two types cover Phase 18's needs. IMAGE_REL_AMD64_REL32 (0x0004) patches CALL rel32 instructions that reference external symbols. IMAGE_REL_AMD64_ADDR64 (0x0001) patches 64-bit absolute addresses for global data. Both are standard and supported by LLD and MSVC link.exe.

**String table for long names**: COFF symbol names are 8 bytes. Names longer than 8 bytes use the string table format: the Name field contains 0x00000000 followed by a 4-byte offset into the string table that follows the symbol table. All TML function names (which include module paths) will likely exceed 8 bytes.

## Impact

- Affected specs: docs/specs/native-backend.md (object file layout section)
- Affected code: compiler/native/coff_emit.tml (new), compiler/native/obj_writer.tml (new), compiler/src/cli/commands/build.cpp (--backend=native routing)
- Breaking change: NO — native backend is opt-in, LLVM path unchanged
- User benefit: `tml build --backend=native` produces working executables on Windows without LLVM

## Risk

MEDIUM. The COFF format has strict byte-level layout requirements. Off-by-one errors in section offsets or symbol table pointers cause LLD to reject the object with cryptic errors. The integration test (task 6.1) is the primary correctness signal. Testing against both LLD and MSVC link.exe (task 6.3) provides confidence in format correctness.

## Reference

- Microsoft PE/COFF Specification v11.0 — authoritative byte-level format definition
- LLVM lib/MC/WinCOFFObjectWriter.cpp — reference implementation
- chibicc codegen.c, pe_object.c — minimal COFF writer in ~400 LOC
- Windows SDK winnt.h — IMAGE_SECTION_HEADER, IMAGE_SYMBOL, IMAGE_RELOCATION definitions
