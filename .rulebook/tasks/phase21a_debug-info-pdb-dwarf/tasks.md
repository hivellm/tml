## Status: 0/24 items complete

## Phase 1: Source Location Tracking
- [ ] 1.1 Thread `SourceSpan` (file path, line, column) through MIR instructions — attach to each MirInst as optional metadata
- [ ] 1.2 Propagate SourceSpan from MIR through MachIR lowering — each MachInst carries the span of the MIR instruction that produced it
- [ ] 1.3 Record PC → SourceSpan mapping table during x86/AArch64 emission (byte offset of each instruction → span)

## Phase 2: DWARF Emission
- [ ] 2.1 Emit `.debug_abbrev` section: abbreviation table defining DIE attribute layouts used in `.debug_info`
- [ ] 2.2 Emit `.debug_info` section: DW_TAG_compile_unit DIE containing DW_TAG_subprogram DIEs for each function
- [ ] 2.3 Emit `DW_AT_name`, `DW_AT_low_pc`, `DW_AT_high_pc`, `DW_AT_language` (DW_LANG_C99 as placeholder) on DW_TAG_subprogram
- [ ] 2.4 Emit `.debug_line` section: line number program (standard opcode sequence mapping PC ranges to source lines)
- [ ] 2.5 Emit `.debug_str` section: deduplicated string table referenced by `DW_AT_name` entries via `DW_FORM_strp`
- [ ] 2.6 Emit `.debug_ranges` section for functions with non-contiguous code (inlined functions, loop transformations)

## Phase 3: PDB Emission
- [ ] 3.1 Emit PDB MSF (Multi-Stream Format) container: superblock, free page map, stream directory
- [ ] 3.2 Emit PDB Info Stream (stream index 1): version, GUID, age, named stream map
- [ ] 3.3 Emit DBI Stream (stream index 3): DbiStreamHeader, ModInfo records (one per .obj), SectionContributions, SectionMap
- [ ] 3.4 Emit TPI Stream (stream index 2): type records for primitive types (T_INT8, T_INT32, T_INT64, T_REAL64, T_VOID)
- [ ] 3.5 Emit public symbols stream: each TML function → PublicSym32 record (name, offset, section)
- [ ] 3.6 Emit Global Symbol stream: GDATA32/GPROC32 records for global variables and procedures with source file references

## Phase 4: Variable Scope Tracking
- [ ] 4.1 Extend MIR with variable liveness scopes: record which MIR Variables are live at each MirInst
- [ ] 4.2 Emit `DW_TAG_variable` DIEs under each `DW_TAG_subprogram`: `DW_AT_name`, `DW_AT_type`, `DW_AT_location` (DW_OP_fbreg for stack vars)
- [ ] 4.3 Emit `DW_AT_start_scope` on variables that are not live for the entire function (scoped `let` bindings)

## Phase 5: Type Info Emission
- [ ] 5.1 Emit DWARF type DIEs: DW_TAG_base_type (I32/I64/F64/Bool), DW_TAG_structure_type (TML structs), DW_TAG_enumeration_type (TML enums)
- [ ] 5.2 Emit CodeView type records in PDB TPI stream: LF_STRUCTURE for TML structs, LF_ENUM for TML enums, LF_POINTER for references
- [ ] 5.3 Emit DW_TAG_member for each struct field with DW_AT_data_member_location (byte offset from struct base)
- [ ] 5.4 Emit LF_FIELDLIST records in PDB for struct fields, with LF_MEMBER for each field offset

## Phase 6: Debugger Verification
- [ ] 6.1 Set a breakpoint on a TML source line using VS Code + Microsoft C++ extension (PDB) and lldb (DWARF) — verify breakpoint hits
- [ ] 6.2 Step through a TML function line by line, inspect local variable values — verify values match TML source semantics
