# Tasks: Bootstrap IR Generator

## Progress: 0% (0/32 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/ir/` directory structure
- [ ] 1.2 Set up CMake configuration for IR module
- [ ] 1.3 Create base header files with include guards

## 2. IR Type System Phase
- [ ] 2.1 Implement IR primitive types (i32, i64, f32, f64, bool, ptr)
- [ ] 2.2 Implement IR aggregate types (struct, array)
- [ ] 2.3 Implement IR function types
- [ ] 2.4 Implement IR pointer/reference types
- [ ] 2.5 Implement type size/alignment calculation

## 3. IR Value Phase
- [ ] 3.1 Implement IR value representation
- [ ] 3.2 Implement SSA value numbering
- [ ] 3.3 Implement constant values
- [ ] 3.4 Implement global values
- [ ] 3.5 Implement function arguments

## 4. IR Instruction Phase
- [ ] 4.1 Implement arithmetic instructions (add, sub, mul, div)
- [ ] 4.2 Implement comparison instructions (eq, ne, lt, gt, le, ge)
- [ ] 4.3 Implement memory instructions (alloca, load, store)
- [ ] 4.4 Implement control flow instructions (br, condbr, ret)
- [ ] 4.5 Implement call instructions
- [ ] 4.6 Implement cast instructions
- [ ] 4.7 Implement GEP (get element pointer) instruction
- [ ] 4.8 Implement phi instructions for SSA

## 5. IR Structure Phase
- [ ] 5.1 Implement basic block representation
- [ ] 5.2 Implement function IR representation
- [ ] 5.3 Implement module IR representation
- [ ] 5.4 Implement IR builder interface

## 6. Lowering Phase
- [ ] 6.1 Implement expression lowering
- [ ] 6.2 Implement statement lowering
- [ ] 6.3 Implement function declaration lowering
- [ ] 6.4 Implement struct/enum lowering
- [ ] 6.5 Implement pattern matching lowering
- [ ] 6.6 Implement closure lowering
- [ ] 6.7 Implement method call lowering

## 7. Testing Phase
- [ ] 7.1 Write unit tests for IR types
- [ ] 7.2 Write unit tests for IR instructions
- [ ] 7.3 Write unit tests for lowering
- [ ] 7.4 Write integration tests with full programs
- [ ] 7.5 Verify test coverage ≥95%

## 8. Documentation Phase
- [ ] 8.1 Document public API in header files
- [ ] 8.2 Update CHANGELOG.md with IR generator implementation
