# Proposal: Debug Info — PDB (Windows) and DWARF (Linux/macOS)

## Why

Without debug information, compiled TML programs are black boxes to debuggers. Developers cannot set
breakpoints on TML source lines, cannot step through code, and cannot inspect local variables. This
makes debugging TML programs as difficult as debugging stripped binaries. Every production language
emits debug info; TML must do the same to be usable for real software development.

The TML production backend (phase20a/20b) emits machine code directly without linking through LLVM
IR. This means debug info must be generated natively by TML's own backend, not inherited from LLVM.
Phase 21a delivers that native debug info emission as the final piece of ERA 2.

## What Changes

### Source Location Tracking (~1,000 LOC)

Every MIR instruction already carries a `SourceSpan` in the parser — but that span is not threaded
through MachIR lowering or instruction emission. Phase 21a threads spans from MIR through MachIR to
the final PC-offset table so that each emitted byte can be mapped to a source location.

- `MirInst` gains optional `span: Maybe[SourceSpan]` metadata
- `MachInst` carries forwarded span through lowering
- During x86/AArch64 emission, each instruction's starting offset is recorded with its span

### DWARF Emission (~3,000 LOC) — `compiler-tml/src/debug/dwarf.tml`

DWARF 5 is the standard debug format on Linux and macOS, fully documented at dwarfstd.org. TML
emits the following DWARF sections into the ELF object file:

- `.debug_abbrev` — abbreviation table defining DIE attribute layouts
- `.debug_info` — tree of DIEs: DW_TAG_compile_unit containing DW_TAG_subprogram per function,
  DW_TAG_variable per local variable with DW_AT_location using DW_OP_fbreg for stack slots
- `.debug_line` — line number program mapping PC ranges to source file + line + column
- `.debug_str` — deduplicated string pool for DW_AT_name, DW_AT_comp_dir
- `.debug_ranges` — non-contiguous PC ranges for inlined or transformed code
- Type DIEs: DW_TAG_base_type (I32/I64/F64/Bool), DW_TAG_structure_type (structs),
  DW_TAG_enumeration_type (enums), DW_TAG_member for struct fields with byte offsets

### PDB Emission (~4,000 LOC) — `compiler-tml/src/debug/pdb.tml`

PDB is the debug format used by Visual Studio and VS Code's C++ extension on Windows. The format is
partially documented (llvm-pdbutil, microsoft/microsoft-pdb), but sufficient for function-level and
variable-level debugging. TML emits the MSF (Multi-Stream Format) container with:

- PDB Info Stream (stream 1): version, GUID, age, named stream map
- TPI Stream (stream 2): CodeView type records — LF_STRUCTURE for TML structs, LF_ENUM for enums,
  LF_POINTER for references, LF_FIELDLIST for struct field lists
- DBI Stream (stream 3): DbiStreamHeader, ModInfo per object, SectionContributions, SectionMap
- Public Symbols stream: PublicSym32 records for each TML function (name, offset, section)
- Global Symbol stream: GDATA32/GPROC32 records with source file references

### Variable Scope Tracking (~500 LOC)

MIR is extended to record variable liveness scopes: which MIR Variables are live at each
instruction. This drives DW_TAG_variable DIEs with DW_AT_start_scope for scoped `let` bindings,
so debuggers show variables only when they are in scope.

### Type Info Emission (~1,500 LOC)

Both DWARF and PDB require type information to display struct fields, enum variants, and primitive
types correctly. A shared type-info emitter walks the TML type system and produces the appropriate
records for each format.

## Key Decisions

**DWARF 5 only** — DWARF 4 is more widely supported but DWARF 5 is simpler (section offsets are
implicit, string deduplication is built in). Since TML controls both the emitter and the minimum
debugger version (lldb 11+, gdb 10+), DWARF 5 is the right choice.

**PDB via MSF construction** — rather than using LLVM's PDB library (which would reintroduce an
LLVM dependency), TML constructs the MSF binary format directly. The format is well-understood from
microsoft-pdb and llvm-pdbutil source code.

**DW_OP_fbreg for all locals** — using the frame base register offset is the simplest and most
reliable location expression. Register-allocated variables that were spilled are handled the same
way. This avoids the complexity of DW_OP_reg expressions and DWARF call frame information.

## Risk

**CRITICAL — PDB**: PDB is a partially reverse-engineered format. Some fields in the DBI and symbol
streams have undocumented semantics. The implementation will be validated by feeding the PDB to
`llvm-pdbutil dump` and to Visual Studio; any rejected or misread PDB is a blocker.

**HIGH — DWARF line programs**: The DWARF line number program encoding is compact but error-prone.
An off-by-one in the PC delta or line delta produces a debugger that breaks on the wrong line.
Validation uses `llvm-dwarfdump --debug-line` on every test binary.

## Success Criteria

1. `tml build hello.tml` produces a binary where VS Code + C++ extension can set a breakpoint on
   a TML source line and the breakpoint hits correctly (PDB, Windows)
2. `lldb ./hello` can set a breakpoint by source line and step through the function line by line
   (DWARF, Linux/macOS)
3. Inspecting a local variable in the debugger shows the correct value matching the TML source
4. `llvm-dwarfdump --verify` reports no errors on a TML-compiled binary
5. `llvm-pdbutil dump --all` can parse and display TML's PDB without errors

## Deliverable

Phase 21a is the final task in ERA 2 (production backend). Upon completion, LLVM is fully eliminated
from the default TML build path. TML has its own x86-64/AArch64 backend, its own linker integration,
and its own debug info emission — a complete native toolchain.

## Dependencies

- **Requires**: phase20a (x86-64 MachIR backend) + phase20b (AArch64 backend) — both must be
  complete so MachIR exists as a target for source location attachment
- **Blocks**: nothing — ERA 2 ends here; ERA 3 (self-hosting) begins with phase22a

## Estimated Size

~10,000 LOC TML across:
- `compiler-tml/src/debug/dwarf.tml` (~3,000 LOC)
- `compiler-tml/src/debug/pdb.tml` (~4,000 LOC)
- `compiler-tml/src/debug/source_map.tml` (~1,000 LOC — span threading + PC table)
- `compiler-tml/src/debug/scope_tracker.tml` (~500 LOC)
- `compiler-tml/src/debug/type_info.tml` (~1,500 LOC)
