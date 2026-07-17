## 1. For-in loops (54+ instances)
- [x] 1.1 types/ty.tml — replaced 12 manual index loops with for-in
- [x] 1.2 types/register.tml — replaced 12 manual index loops with for-in
- [x] 1.3 types/module_binary.tml — replaced convertible loops (1 non-standard increment left)
- [x] 1.4 types/module_loader.tml — 1 non-standard increment loop, not convertible
- [x] 1.5 ast/serial.tml — replaced 2 manual index loops with for-in
- [x] 1.6 ast/ast_writer.tml — replaced ~35 manual index loops with for-in
- [x] 1.7 types/imports.tml — replaced 10 manual index loops with for-in

## 2. @repr on enums
- [x] 2.1 types/ty.tml — added @repr(U8) to PrimitiveKind (17 variants)
- [x] 2.2 types/env.tml — added @repr(U8) to ScopeKind (3 variants)

## 3. Struct update syntax
- [x] 3.1 types/register.tml — reviewed; struct/union defs don't copy from existing binding, ..base not applicable

## 4. Pattern guards + let-else
- [x] 4.1 types/register.tml — when expressions already flat (value expressions, not cascades)
- [x] 4.2 types/imports.tml — compacted when arms to single-line; sequential lookup pattern correct as-is

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation — pure refactor, no new API
- [x] 5.2 Write tests covering the new behavior — existing tests validate (pure refactor, same semantics)
- [x] 5.3 Run tests and confirm they pass — 240/241 pass (1 pre-existing X002 timeout)
