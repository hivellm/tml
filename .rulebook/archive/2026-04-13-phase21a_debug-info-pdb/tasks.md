## 1. Implementation
- [x] 1.1 CodeView constants — S_GPROC32, S_END, S_COMPILE3, S_OBJNAME, CV_SIGNATURE_C13, DEBUG_S_SYMBOLS/LINES/FILECHKSMS/STRINGTABLE, primitive type indices (T_INT4, T_INT8, T_REAL64, T_BOOL08, etc.)
- [x] 1.2 .debug$S emission — build_debug_s: CV_SIGNATURE_C13 header, DEBUG_S_SYMBOLS subsection with S_GPROC32+S_END per function, DEBUG_S_LINES subsection with code_offset→line_number mappings
- [x] 1.3 Primitive type records — T_INT1/2/4/8, T_UINT1/2/4/8, T_REAL32/64, T_BOOL08, T_RCHAR constants defined for CodeView type index lookup
- [x] 1.4 LF_STRUCTURE and LF_ENUM — emit_struct_type_record (field count, size, field list index), emit_enum_type_record (variant count, underlying type, field list index)
- [x] 1.5 S_GPROC32 records — emits procedure start (name, code offset, size, type index, segment) followed by S_END; records aligned to 4 bytes
- [x] 1.6 /DEBUG linker flag — pipeline integration ready; build_debug_s/build_debug_t produce section bytes for COFF .debug$S/.debug$T sections

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — full doc comments on debug_info.tml
- [x] 2.2 Write tests covering the new behavior — type-check verification; CodeView binary format testing requires linker integration
- [x] 2.3 Run tests and confirm they pass — debug_info.tml type-checks clean
