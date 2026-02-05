# Tasks: Language Ergonomics Improvements

**Status**: In Progress (43%)

## Phase 1: Never Type Coercion ✓

- [x] 1.1.1 Add `is_never()` method to Type class
- [x] 1.1.2 Update type unification to handle never type
- [x] 1.1.3 Update when expression type inference for never branches
- [x] 1.1.4 Add unit tests for never type coercion
- [x] 1.1.5 Test cipher.tml when expressions work without workaround

## Phase 2: Chained Reference Field Access ✓

- [x] 2.1.1 Analyze current field access codegen for ref types
- [x] 2.1.2 Fix GEP generation for chained access on refs
- [x] 2.1.3 Add unit tests for `ref_var.field1.field2` patterns
- [x] 2.1.4 Test cipher.tml `tag.data.handle` works without workaround

## Phase 3: Auto-Dereference for Method Calls ✓

- [x] 3.1.1 Implement method resolution with auto-deref candidates
- [x] 3.1.2 Update codegen to insert deref when needed
- [x] 3.1.3 Add unit tests for `ref_var.method()` calls
- [x] 3.1.4 Test `key.len()` works on `ref Buffer` parameters

## Phase 4: String Pattern Matching in When ✓

- [x] 4.1.1 Detect string type in when expression patterns
- [x] 4.1.2 Implement desugaring to if-else chain in HIR
- [x] 4.1.3 Add unit tests for string when patterns
- [x] 4.1.4 Test cipher.tml `from_name` with when syntax

## Phase 5: Try Operator (!) ✓

Note: TML uses `!` instead of `?` for better LLM visibility.

- [x] 5.1.1 Add `!` token to lexer (already existed)
- [x] 5.1.2 Parse `!` as postfix operator in expressions
- [x] 5.1.3 Add TryExpr AST/HIR node
- [x] 5.1.4 Implement type checking for try expressions
- [x] 5.1.5 Implement HIR lowering for try expressions
- [x] 5.1.6 Implement codegen for try operator
- [x] 5.1.7 Add unit tests for `!` on Outcome
- [x] 5.1.8 Add unit tests for `!` on Maybe
- [x] 5.1.9 Add error for `!` in non-fallible function context
- [x] 5.1.10 Tests in try_operator.test.tml validate all features

## Phase 6: If Let / Let Else Patterns ✓

- [x] 6.1.1 Parse `if let Pattern = expr` in parser
- [x] 6.1.2 Parse `let Pattern = expr else { ... }` in parser
- [x] 6.1.3 Add IfLetExpr and LetElseStmt to HIR nodes
- [x] 6.1.4 Implement type checking for pattern bindings
- [x] 6.1.5 Implement codegen for if let expressions
- [x] 6.1.6 Implement codegen for let else statements
- [x] 6.1.7 Add unit tests for if let patterns (if_let_basic.test.tml)
- [x] 6.1.8 Add unit tests for let else early exit (let_else_basic.test.tml)

## Phase 7: Better Type Inference

- [ ] 7.1.1 Analyze current type inference limitations
- [ ] 7.1.2 Implement RHS-to-LHS type propagation for let
- [ ] 7.1.3 Improve generic type inference from arguments
- [ ] 7.1.4 Add unit tests for inferred type declarations
- [ ] 7.1.5 Test cipher.tml compiles with fewer annotations

## Phase 8: Tuple Destructuring

- [ ] 8.1.1 Parse tuple patterns `(a, b)` in let statements
- [ ] 8.1.2 Parse tuple patterns in when arms
- [ ] 8.1.3 Parse tuple patterns in function parameters
- [ ] 8.1.4 Implement type checking for tuple destructuring
- [ ] 8.1.5 Implement codegen using extractvalue
- [ ] 8.1.6 Add unit tests for tuple destructuring

## Phase 9: Default Struct Field Values

- [ ] 9.1.1 Parse default values in struct field definitions
- [ ] 9.1.2 Store defaults in type metadata
- [ ] 9.1.3 Validate default types match field types
- [ ] 9.1.4 Generate missing fields with defaults in struct literals
- [ ] 9.1.5 Add unit tests for default field values

## Phase 10: Late Variable Initialization

- [ ] 10.1.1 Parse `let x: T` without initializer
- [ ] 10.1.2 Implement definite assignment analysis
- [ ] 10.1.3 Track assignment status through control flow
- [ ] 10.1.4 Error on use before assignment
- [ ] 10.1.5 Error if any path doesn't assign
- [ ] 10.1.6 Add unit tests for late initialization

## Phase 11: Implicit Returns

- [ ] 11.1.1 Detect last expression without semicolon in blocks
- [ ] 11.1.2 Convert to implicit return in HIR lowering
- [ ] 11.1.3 Handle if/when expressions as implicit returns
- [ ] 11.1.4 Add unit tests for implicit returns
- [ ] 11.1.5 Update documentation with implicit return rules

## Phase 12: Struct Field Iteration (Compile-Time Reflection)

- [ ] 12.1.1 Design syntax for field iteration (`loop field in T.fields()`)
- [ ] 12.1.2 Add `fields()` compile-time intrinsic for types
- [ ] 12.1.3 Implement FieldInfo type with name, type, offset metadata
- [ ] 12.1.4 Parse field iteration loops in parser
- [ ] 12.1.5 Unroll field loops at compile time in HIR lowering
- [ ] 12.1.6 Support instance field iteration (`obj.fields()` yields name/value pairs)
- [ ] 12.1.7 Implement codegen for unrolled field access
- [ ] 12.1.8 Add unit tests for type field iteration
- [ ] 12.1.9 Add unit tests for instance field iteration
- [ ] 12.1.10 Test struct-to-struct mapping without explicit field names

## Phase 13: Union Types (C++ Style)

- [ ] 13.1.1 Add `union` keyword to lexer
- [ ] 13.1.2 Parse union declarations with field list syntax
- [ ] 13.1.3 Add UnionDecl AST node similar to StructDecl
- [ ] 13.1.4 Implement type checking for union types
- [ ] 13.1.5 Calculate union size as max of all field sizes
- [ ] 13.1.6 Calculate union alignment as max of all field alignments
- [ ] 13.1.7 Generate LLVM IR union type (single allocation, field access via bitcast)
- [ ] 13.1.8 Implement field read codegen (load from union pointer)
- [ ] 13.1.9 Implement field write codegen (store to union pointer)
- [ ] 13.1.10 Mark union field access as `lowlevel` (unsafe - no type checking at runtime)
- [ ] 13.1.11 Add `size_of[UnionType]()` intrinsic support
- [ ] 13.1.12 Add unit tests for basic union declaration and access
- [ ] 13.1.13 Add unit tests for union with different sized fields
- [ ] 13.1.14 Add unit tests for union in struct (nested)
- [ ] 13.1.15 Document union syntax and safety considerations

## Phase 14: Final Validation

- [ ] 14.1.1 Run full test suite
- [ ] 14.1.2 Refactor cipher.tml to fully idiomatic code
- [ ] 14.1.3 Update documentation with all new syntax
- [ ] 14.1.4 Update CLAUDE.md with working patterns
- [ ] 14.1.5 Create migration guide for existing code