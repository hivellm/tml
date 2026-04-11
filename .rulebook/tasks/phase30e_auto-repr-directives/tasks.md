## 1. @repr directive
- [ ] 1.1 Register `@repr` as a known built-in directive in the compiler
- [ ] 1.2 Parse `@repr(u8)`, `@repr(u16)`, `@repr(i32)` on enum/type declarations
- [ ] 1.3 Store repr layout info on the TypeDecl AST node
- [ ] 1.4 In codegen: assign sequential discriminants (0, 1, 2...) matching repr type
- [ ] 1.5 Allow `enum_value as U8` cast when @repr(u8) is present

## 2. @auto directive
- [ ] 2.1 Register `@auto` as a known built-in directive
- [ ] 2.2 Parse `@auto(debug, duplicate, equal)` — list of behavior names
- [ ] 2.3 Implement auto-generation for `Duplicate` (field-by-field clone)
- [ ] 2.4 Implement auto-generation for `PartialEq` / `equal` (field-by-field comparison)
- [ ] 2.5 Implement auto-generation for `Debug` / `display` (field-by-field formatting)

## 3. @packed directive
- [ ] 3.1 Register `@packed` as a known built-in directive
- [ ] 3.2 In codegen: emit packed struct layout in LLVM IR (no padding)

## 4. Tail (mandatory)
- [ ] 4.1 Add tests: @repr enum with `as U8` cast
- [ ] 4.2 Add tests: @auto(duplicate) generates working Duplicate impl
- [ ] 4.3 Add tests: @packed struct has expected size
- [ ] 4.4 Update CHANGELOG.md
- [ ] 4.5 Run tests and confirm they pass
