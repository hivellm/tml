# Proposal: phase31e_destructuring-struct-update

## Why
70+ instances of sequential `let x = s.field` patterns exist where destructuring `let Struct { x, y } = s` is clearer and more concise. Additionally, ~20 struct constructions copy most fields from a base with 1-2 changes, which `..base` syntax (shipped in phase30c) handles elegantly.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
- Replace `let x = s.field; let y = s.field` with `let Struct { x, y } = s`
- Replace field-by-field struct construction (where most fields copied) with `Struct { changed_field: val, ..base }`
- Pure logic-preserving refactor

## Impact
- Affected specs: none
- Affected code: core/net/ip.tml, core/reflect/mod.tml, std/collections/hashmap.tml, std/bigint.tml, std/json/types.tml, std/db/orm/relation.tml, compiler-tml/ast/ast_writer.tml, compiler-tml/lexer/lexer.tml
- Breaking change: NO
- User benefit: More idiomatic, concise code
