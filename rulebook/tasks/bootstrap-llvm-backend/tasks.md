# Tasks: Bootstrap LLVM Backend

## Progress: 85% (26/30 tasks complete)

## 1. Setup Phase
- [x] 1.1 Create `src/codegen/` directory structure
- [x] 1.2 Set up CMake configuration with LLVM dependencies
- [x] 1.3 Create base header files with include guards
- [x] 1.4 Verify LLVM 17+ installation and API access

## 2. LLVM Context Phase
- [x] 2.1 Implement LLVM context wrapper (using text IR generation)
- [x] 2.2 Implement LLVM module wrapper
- [x] 2.3 Implement target machine configuration
- [x] 2.4 Implement data layout handling

## 3. Type Conversion Phase
- [x] 3.1 Implement primitive type conversion (I8-I128, U8-U128, F32, F64, Bool)
- [x] 3.2 Implement struct type conversion with layout
- [x] 3.3 Implement array type conversion (dynamic lists)
- [x] 3.4 Implement pointer type conversion
- [x] 3.5 Implement function type conversion
- [x] 3.6 Implement type caching for efficiency
- [x] 3.7 Implement generic type handling (List[T], HashMap[K,V], Buffer)

## 4. Instruction Translation Phase
- [x] 4.1 Implement arithmetic instruction translation (+, -, *, /, %)
- [x] 4.2 Implement comparison instruction translation (==, !=, <, >, <=, >=)
- [x] 4.3 Implement memory instruction translation (alloca, load, store)
- [x] 4.4 Implement control flow instruction translation (br, ret, if/else)
- [x] 4.5 Implement call instruction translation
- [x] 4.6 Implement GEP instruction translation (struct fields, array index)
- [ ] 4.7 Implement phi instruction translation
- [x] 4.8 Implement cast instruction translation
- [x] 4.9 Implement bitwise operations (|, &, ^, ~, <<, >>)
- [x] 4.10 Implement logical operations (and, or, not)

## 5. Function Generation Phase
- [x] 5.1 Implement function declaration generation
- [x] 5.2 Implement basic block generation
- [x] 5.3 Implement function attribute handling
- [x] 5.4 Implement calling convention handling
- [x] 5.5 Implement method call syntax (.len(), .push(), etc.)
- [x] 5.6 Fix return type tracking for method calls (2025-12-26: Set last_expr_type_ correctly)

## 6. Optimization Phase
- [ ] 6.1 Implement optimization pipeline configuration
- [x] 6.2 Implement O0 (no optimization) mode (default)
- [ ] 6.3 Implement O1/O2/O3 optimization levels
- [ ] 6.4 Implement Os/Oz (size optimization) modes

## 7. Code Emission Phase
- [x] 7.1 Implement object file generation (via clang)
- [ ] 7.2 Implement debug info generation (DWARF)
- [x] 7.3 Implement linker invocation
- [x] 7.4 Implement executable output

## 8. Testing Phase
- [x] 8.1 Write unit tests for type conversion
- [x] 8.2 Write unit tests for instruction translation
- [x] 8.3 Write integration tests with runnable programs
- [ ] 8.4 Verify test coverage ≥95%

## 9. Documentation Phase
- [x] 9.1 Document public API in header files
- [ ] 9.2 Update CHANGELOG.md with LLVM backend implementation

## Implemented Features (Not in Original Plan)
- [x] Array literal syntax `[1, 2, 3]`
- [x] Array repeat syntax `[0; 10]`
- [x] Index syntax `arr[i]`
- [x] Enum declarations and PathExpr (Color::Red)
- [x] Complex conditionals with function calls
- [x] For loop with range (`for i in N`)
- [x] While loop
- [x] Break/continue
- [x] Bool return type handling
- [x] String literals and println
