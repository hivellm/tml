# Proposal: Bootstrap LLVM Backend

## Why

The LLVM backend translates TML IR to LLVM IR, leveraging LLVM's mature optimization infrastructure and code generation for multiple target architectures. This enables TML to produce efficient native binaries for x86-64, ARM64, and other platforms without implementing custom optimizers or code generators. LLVM provides production-quality optimization passes and target-specific code generation.

## What Changes

### New Components

1. **LLVM Context** (`src/codegen/llvm_context.hpp`)
   - LLVM context and module management
   - Type cache
   - Target machine configuration

2. **LLVM Type Converter** (`src/codegen/llvm_types.hpp`, `src/codegen/llvm_types.cpp`)
   - TML IR type to LLVM type conversion
   - Struct layout handling
   - Function type conversion

3. **LLVM IR Generator** (`src/codegen/llvm_gen.hpp`, `src/codegen/llvm_gen.cpp`)
   - TML IR to LLVM IR translation
   - Instruction mapping
   - Intrinsic function handling

4. **LLVM Optimizer** (`src/codegen/optimizer.hpp`, `src/codegen/optimizer.cpp`)
   - LLVM optimization pass configuration
   - Optimization level handling (O0, O1, O2, O3, Os, Oz)
   - Custom pass ordering

5. **Object Emitter** (`src/codegen/emitter.hpp`, `src/codegen/emitter.cpp`)
   - Object file generation
   - Executable linking
   - Debug info generation

### LLVM Integration

Based on `docs/specs/16-COMPILER-ARCHITECTURE.md`:
- Use LLVM 17+ API
- Target data layout configuration
- Platform-specific calling conventions
- Debug information (DWARF)

## Impact

- **Affected specs**: 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/codegen/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Native code generation for multiple platforms
- **Dependencies**: Requires bootstrap-ir-generator to be complete, LLVM 17+ installed

## Success Criteria

1. All TML IR instructions are translated to LLVM IR
2. Generated code runs correctly on x86-64
3. Optimization levels work (O0-O3)
4. Debug info is generated for debugger support
5. Object files can be linked into executables
6. Test coverage ≥95%
