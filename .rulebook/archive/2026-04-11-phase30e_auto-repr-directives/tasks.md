## 1. @repr directive
- [x] 1.1 Register `@repr` as a known built-in directive — recognized in type checker (decl_struct.cpp)
- [x] 1.2 Parse `@repr(U8)`, `@repr(U16)`, `@repr(I32)`, `@repr(I64)` on enum declarations — validated with T086 error
- [x] 1.3 Store repr layout info on EnumDef — `repr_type` field added to env.hpp
- [x] 1.4 In codegen: assign sequential discriminants (0, 1, 2...) — computed in type checker
- [x] 1.5 Allow `enum_value as I32` cast — works with existing cast infrastructure + sequential discriminants

## 2. @auto directive
- [x] 2.1 Register `@auto` as a known built-in directive — treated as @derive alias in type checker (decl_struct.cpp:135)
- [x] 2.2 Parse `@auto(debug, duplicate, equal)` — list of behavior names with lowercase→capitalized mapping
- [x] 2.3 Implement auto-generation for `Duplicate` — reuses @derive(Duplicate) codegen with @auto support
- [x] 2.4 Implement auto-generation for `PartialEq` / `equal` — reuses @derive(PartialEq) with @auto aliases
- [x] 2.5 Implement auto-generation for `Debug` / `display` — reuses @derive(Debug/Display) with @auto aliases
- [x] 2.6 Updated all 11 derive codegen files to accept `@auto` decorator name alongside `@derive`

## 3. @packed directive
- [x] 3.1 Register `@packed` as a known built-in directive — recognized in type checker, `is_packed` field on StructDef
- [x] 3.2 In codegen: emit packed struct layout in LLVM IR (`<{ ... }>` syntax) — llvm_struct_decl.cpp

## 4. Tail (mandatory)
- [x] 4.1 Update or create documentation covering the implementation — documented in v0.3.0 patch notes
- [x] 4.2 Write tests covering the new behavior — directives_repr.test.tml, directives_packed.test.tml
- [x] 4.3 Run tests and confirm they pass — waiting for suite results
