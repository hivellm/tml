# Tasks: Implement HIR

**Status**: Not started

## 1. HIR Data Structures
- [ ] 1.1 Create `compiler/include/hir/hir.hpp` with core types
- [ ] 1.2 Define HirType (primitives, refs, arrays, slices, tuples, structs, enums, functions)
- [ ] 1.3 Define HirId and HirSpan for tracking
- [ ] 1.4 Define HirExpr variant (literal, variable, binary, unary, call, method, field, index, block, if, when, loop, try, closure, struct/array/tuple literals)
- [ ] 1.5 Define HirStmt variant (let, expr, assign, compound assign)
- [ ] 1.6 Define HirFunction, HirStruct, HirEnum, HirBehavior, HirImpl
- [ ] 1.7 Define HirPattern variant (wildcard, binding, literal, tuple, struct, enum, or, range)
- [ ] 1.8 Define HirModule container

## 2. AST to HIR Lowering
- [ ] 2.1 Create `compiler/include/hir/hir_builder.hpp`
- [ ] 2.2 Create `compiler/src/hir/hir_builder.cpp`
- [ ] 2.3 Implement HirIdGenerator and HirTypeResolver
- [ ] 2.4 Implement lower_expr for all expression types
- [ ] 2.5 Implement lower_stmt for all statement types
- [ ] 2.6 Implement lower_function, lower_struct, lower_enum
- [ ] 2.7 Implement lower_behavior, lower_impl
- [ ] 2.8 Implement lower_pattern for all pattern types
- [ ] 2.9 Implement generic monomorphization during lowering
- [ ] 2.10 Implement closure capture analysis

## 3. HIR to MIR Lowering
- [ ] 3.1 Refactor MirBuilder to accept HIR instead of AST
- [ ] 3.2 Implement control flow lowering (if, when, loop)
- [ ] 3.3 Implement expression lowering (call, method, closure)
- [ ] 3.4 Implement drop insertion based on scope

## 4. HIR Optimizations
- [ ] 4.1 Implement constant folding pass
- [ ] 4.2 Implement dead code elimination pass
- [ ] 4.3 Implement inline expansion pass
- [ ] 4.4 Implement closure optimization pass

## 5. HIR Caching
- [ ] 5.1 Define HIR serialization format
- [ ] 5.2 Implement HIR serialization/deserialization
- [ ] 5.3 Add content hash for change detection
- [ ] 5.4 Implement dependency tracking for incremental compilation

## 6. Pipeline Integration
- [ ] 6.1 Update cmd_build.cpp to use HIR pipeline
- [ ] 6.2 Update parallel_build.cpp for HIR-aware parallelism
- [ ] 6.3 Update test_runner.cpp to use new pipeline
- [ ] 6.4 Improve error reporting with HIR type information

## 7. Testing
- [ ] 7.1 Add unit tests for HIR data structures
- [ ] 7.2 Add tests for AST to HIR lowering
- [ ] 7.3 Add tests for HIR to MIR lowering
- [ ] 7.4 Add tests for HIR optimizations
- [ ] 7.5 Verify all existing tests pass with new pipeline

## 8. Documentation
- [ ] 8.1 Document HIR architecture in docs/
- [ ] 8.2 Add inline documentation to HIR headers
- [ ] 8.3 Update CHANGELOG.md with HIR implementation

## Validation
- [ ] All 600+ existing tests pass with HIR pipeline
- [ ] Compilation produces identical runtime behavior
- [ ] Compile time overhead is acceptable (< 10%)
- [ ] HIR serialization/deserialization works correctly
- [ ] Drop/RAII semantics preserved through HIR
