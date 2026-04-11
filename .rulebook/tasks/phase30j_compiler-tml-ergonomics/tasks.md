## 1. For-in loops (54+ instances)
- [ ] 1.1 types/ty.tml — replace 12 manual index loops with for-in
- [ ] 1.2 types/register.tml — replace 12 manual index loops with for-in
- [ ] 1.3 types/module_binary.tml — replace 18 manual index loops with for-in
- [ ] 1.4 types/module_loader.tml — replace 4 manual index loops with for-in
- [ ] 1.5 ast/serial.tml — replace 2 manual index loops with for-in
- [ ] 1.6 ast/ast_writer.tml — replace 2 manual index loops with for-in

## 2. @repr on enums
- [ ] 2.1 types/ty.tml — add @repr(U8) to PrimitiveKind (17 variants)
- [ ] 2.2 types/env.tml — add @repr(U8) to ScopeKind (3 variants)

## 3. Struct update syntax
- [ ] 3.1 types/register.tml — use ..base for StructDef/EnumDef construction where fields overlap

## 4. Pattern guards + let-else
- [ ] 4.1 types/register.tml — flatten nested when for return type resolution
- [ ] 4.2 types/imports.tml — flatten nested when for module lookup chains

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
