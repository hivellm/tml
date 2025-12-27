# Proposal: Bootstrap IR Generator

## Why

The IR (Intermediate Representation) Generator transforms the Typed AST into TML IR, a lower-level representation suitable for optimization and code generation. TML IR is designed to be simple enough for LLVM-based backends while preserving all semantic information needed for memory safety. This stage bridges the gap between the high-level language semantics and machine code generation.

## What Changes

### New Components

1. **TML IR Types** (`src/ir/types.hpp`)
   - Basic blocks and instructions
   - IR value representation
   - IR type system (matches but simplifies TML types)

2. **IR Builder** (`src/ir/builder.hpp`, `src/ir/builder.cpp`)
   - Instruction construction
   - Basic block management
   - Value numbering

3. **IR Generator** (`src/ir/generator.hpp`, `src/ir/generator.cpp`)
   - TAST to IR translation
   - Expression lowering
   - Statement lowering
   - Pattern matching lowering

4. **IR Function** (`src/ir/function.hpp`)
   - Function IR representation
   - Parameter handling
   - Local variable allocation

5. **IR Module** (`src/ir/module.hpp`)
   - Module-level IR representation
   - Global definitions
   - Type definitions

### IR Design Principles

Based on `docs/specs/16-COMPILER-ARCHITECTURE.md`:
- SSA (Static Single Assignment) form
- Explicit memory operations (alloca, load, store)
- Explicit reference/pointer operations
- Control flow via basic blocks and terminators
- No implicit copies or moves (all explicit)

## Impact

- **Affected specs**: 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/ir/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Foundation for optimization and codegen
- **Dependencies**: Requires bootstrap-borrow-checker to be complete

## Success Criteria

1. All TAST node types are translated to IR
2. SSA form is correctly maintained
3. Memory operations are explicit
4. Control flow is correctly lowered
5. Pattern matching is correctly lowered
6. IR can be serialized for debugging
7. Test coverage ≥95%
