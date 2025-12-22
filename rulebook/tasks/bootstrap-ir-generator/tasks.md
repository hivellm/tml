# Tasks: Bootstrap IR Generator

## Progress: 91% (29/32 tasks complete)

## 1. Setup Phase
- [x] 1.1 Create `src/ir/` directory structure
- [x] 1.2 Set up CMake configuration for IR module
- [x] 1.3 Create base header files with include guards

## 2. IR Type System Phase
- [x] 2.1 Implement IR primitive types (i32, i64, f32, f64, bool, ptr)
- [x] 2.2 Implement IR aggregate types (struct, array)
- [x] 2.3 Implement IR function types
- [x] 2.4 Implement IR pointer/reference types
- [x] 2.5 Implement type size/alignment calculation

## 3. IR Value Phase
- [x] 3.1 Implement IR value representation
- [x] 3.2 Implement SSA value numbering
- [x] 3.3 Implement constant values
- [x] 3.4 Implement global values
- [x] 3.5 Implement function arguments

## 4. IR Instruction Phase
- [x] 4.1 Implement arithmetic instructions (add, sub, mul, div)
- [x] 4.2 Implement comparison instructions (eq, ne, lt, gt, le, ge)
- [x] 4.3 Implement memory instructions (alloca, load, store)
- [x] 4.4 Implement control flow instructions (br, condbr, ret)
- [x] 4.5 Implement call instructions
- [x] 4.6 Implement cast instructions
- [x] 4.7 Implement GEP (get element pointer) instruction
- [x] 4.8 Implement phi instructions for SSA

## 5. IR Structure Phase
- [x] 5.1 Implement basic block representation
- [x] 5.2 Implement function IR representation
- [x] 5.3 Implement module IR representation
- [x] 5.4 Implement IR builder interface

## 6. Lowering Phase
- [x] 6.1 Implement expression lowering
- [x] 6.2 Implement statement lowering
- [x] 6.3 Implement function declaration lowering
- [x] 6.4 Implement struct/enum lowering
- [x] 6.5 Implement pattern matching lowering
- [ ] 6.6 Implement closure lowering
- [x] 6.7 Implement method call lowering

## 7. Testing Phase
- [x] 7.1 Write unit tests for IR types
- [x] 7.2 Write unit tests for IR instructions
- [x] 7.3 Write unit tests for lowering
- [x] 7.4 Write integration tests with full programs
- [ ] 7.5 Verify test coverage ≥95%

## 8. Documentation Phase
- [x] 8.1 Document public API in header files
- [x] 8.2 Update CHANGELOG.md with IR generator implementation

## Implementation Notes

**Completed**: IR generator fully modularized into 10 modules:

**Builder modules** (6 files):
- `builder_module.cpp` - Module-level IR building
- `builder_decls.cpp` - Declaration lowering
- `builder_expr.cpp` - Expression lowering
- `builder_stmt.cpp` - Statement lowering
- `builder_type.cpp` - Type lowering
- `builder_utils.cpp` - Builder utilities

**Emitter modules** (4 files):
- `emitter_core.cpp` - Core IR emission
- `emitter_decls.cpp` - Declaration emission
- `emitter_expr.cpp` - Expression emission
- `emitter_stmt.cpp` - Statement emission

**Features**:
- ✅ Full SSA-based IR representation
- ✅ All arithmetic and comparison instructions
- ✅ Memory management (alloca, load, store)
- ✅ Control flow (branches, returns, phi nodes)
- ✅ Function calls and method dispatch
- ✅ Struct and enum lowering
- ✅ Pattern matching lowering
- ✅ Type size/alignment calculation

**Known Issues**:
- ⚠️ Missing closure lowering (6.6)
- ⚠️ Test coverage not verified

**Status**: Fully functional IR generation with SSA form, ready for code generation.

**Next Steps**:
- [ ] Implement closure lowering
- [ ] Verify test coverage ≥95%
- [ ] Add optimization passes (optional, future work)
