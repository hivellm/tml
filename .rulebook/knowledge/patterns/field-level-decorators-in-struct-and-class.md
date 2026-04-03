# Field-Level Decorators in StructField and ClassField

## Context

Added field-level decorator support (`@column`, `@primary_column`, `@nullable`, etc.) to
both `StructField` (parser/ast_decls.hpp) and `ClassField` (parser/ast_oop.hpp).

## Pattern

### AST side (ast_decls.hpp / ast_oop.hpp)
Add `std::vector<Decorator> decorators;` field to the struct. Because `std::vector` is
default-constructible to empty, all existing designated initializer sites that omit
`.decorators` continue to compile without changes — C++20 designated initializers
value-initialize undesignated members.

### Parser side — struct fields (parser_decl.cpp)
In the field-parsing loop inside `parse_struct_decl`, call `parse_decorators()` AFTER
`collect_doc_comment()` but BEFORE `parse_visibility()`. Store the result and pass it
into the `StructField{...}` push_back with `.decorators = std::move(field_decorators)`.

### Parser side — class fields (parser_oop.cpp)
`parse_class_member` already calls `parse_decorators()` at the top of the function for
the whole member (field or method). The `decorators` variable was being forwarded to
`ClassMethod` but NOT to `ClassField`. Fix: add `.decorators = std::move(decorators)`
to the `ClassField{...}` return at the end of the field path.

## Non-impacted sites

- Union field parser (parser_decl.cpp ~line 665): unions don't support decorators; omit
- Enum struct-variant field parsers (parser_decl.cpp ~line 800, parser_decl_impl.cpp
  ~line 375): enum variant fields don't support decorators; omit is safe

## Key fact

Decorators are metadata-only. The type checker, HIR builder, MIR builder, and codegen
all ignore `StructField::decorators` and `ClassField::decorators` — no downstream
changes are needed.
