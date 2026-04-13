## 1. Implementation
- [ ] 1.1 Define CodeView record type constants and structs (S_GPROC32, S_END, S_COMPILE3, CV_SIGNATURE_C13) in debug_info.tml
- [ ] 1.2 Emit .debug$S section with the line-number subsection mapping instruction offsets to source file + line number
- [ ] 1.3 Emit CodeView type records for primitive types: T_INT4 (I32), T_INT8 (I64), T_REAL64 (F64), T_BOOL08 (Bool)
- [ ] 1.4 Emit LF_STRUCTURE type records for struct types and LF_ENUM records for enum types
- [ ] 1.5 Emit S_GPROC32 symbol record for each compiled function (mangled name, code offset, size, type index) followed by S_END
- [ ] 1.6 Pass /DEBUG and /PDB:<output>.pdb flags to the LLD linker invocation in pipeline.tml

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
