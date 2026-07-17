## 1. LLVM type lowering
- [x] 1.1 In struct type generation, emit Bool fields as `i8` instead of `i1` — llvm_struct_decl.cpp (3 paths: builtin, non-generic, generic instantiation) + mir_codegen.cpp (2 paths: emit_struct_def, used_struct_types_)
- [x] 1.2 On load from Bool struct field: emit `load i8` then `trunc i8 to i1` — struct_field.cpp line 1112
- [x] 1.3 On store to Bool struct field: emit `zext i1 to i8` then `store i8` — 5 codegen paths fixed:
  - llvm_struct_expr.cpp (3 store paths in struct construction)
  - instructions_misc.cpp (MIR StructInitInst class-path + coerce_int_type lambda)
  - binary.cpp (legacy AST field assignment `this.field = val`)
  - instructions.cpp (MIR StoreInst handler)
- [x] 1.4 Keep Bool as `i1` in local variables and function parameters (no change)

## 2. Tail (mandatory)
- [x] 2.1 Add test: struct with Bool field stores and loads correctly — compiler/tests/compiler/bool_struct_fields.test.tml
- [x] 2.2 Add test: struct with multiple Bool fields has correct layout (no overlap) — same test, 2 Bool fields + I64
- [x] 2.3 Verify existing tests still pass — all compiler tests pass including Cell[Bool] generic instantiation
- [x] 2.4 Update or create documentation covering the implementation — documented in CHANGELOG.md v0.3.0 and docs/patches/v0.3.0.md
- [x] 2.5 Write tests covering the new behavior — bool_struct_fields.test.tml
- [x] 2.6 Run tests and confirm they pass
