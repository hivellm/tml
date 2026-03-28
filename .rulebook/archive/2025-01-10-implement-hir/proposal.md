# Proposal: Implement HIR (High-level Intermediate Representation)

## Why

The current TML compiler architecture goes directly from the type-checked AST to LLVM IR generation, with an underused MIR (Mid-level IR) layer that was designed but never fully integrated into the main compilation path. This creates several problems:

1. **Limited optimization opportunities**: Without a proper HIR, type-aware optimizations like constant folding, dead code elimination, and inlining must be done either at the AST level (too early, types not fully resolved) or at LLVM level (too late, high-level information lost).

2. **Poor incremental compilation**: The current pipeline requires full recompilation on any change because there's no stable intermediate representation that can be cached and reused.

3. **Weak IDE support**: The LSP integration lacks rich type information because the AST doesn't preserve all resolved type information in a query-friendly format.

4. **Complex codegen**: The LLVM IR generator (llvm_ir_gen*.cpp) is doing too much work - type resolution, lowering, and IR generation all at once. This makes the code harder to maintain and extend.

5. **Error recovery limitations**: Without a clean separation between type checking and lowering, error recovery and partial compilation are difficult to implement.

Adding a proper HIR layer between the type checker and MIR will address all these issues by providing a stable, typed, query-friendly intermediate representation.

## What Changes

### Architecture Change

Current pipeline:
```
Source → Lexer → Parser → AST → TypeChecker → AST (typed) → LLVMIRGen → LLVM IR
```

New pipeline:
```
Source → Lexer → Parser → AST → TypeChecker → HIR → MIR → LLVM IR
```

### New Components

1. **HIR Data Structures** (`compiler/include/hir/`, `compiler/src/hir/`)
   - HirType - simplified type representation with resolved generics
   - HirExpr - typed expression nodes with HirId tracking
   - HirStmt - statement nodes
   - HirDecl - function, struct, enum, behavior, impl declarations
   - HirPattern - pattern matching nodes
   - HirModule - top-level module container

2. **AST to HIR Lowering** (`compiler/src/hir/hir_builder.cpp`)
   - Converts type-checked AST to HIR
   - Performs generic monomorphization
   - Resolves all method calls and field accesses
   - Performs closure capture analysis

3. **HIR to MIR Lowering** (`compiler/src/hir/lower_to_mir.cpp`)
   - Refactors existing MIR builder to accept HIR
   - Simplifies control flow lowering
   - Handles drop insertion

4. **HIR Optimizations** (`compiler/src/hir/hir_opt/`)
   - Constant folding
   - Dead code elimination
   - Inline expansion
   - Closure optimization

5. **HIR Caching** (`compiler/src/hir/hir_cache.cpp`)
   - Serialization/deserialization
   - Content hashing for change detection
   - Dependency tracking

### Modified Components

- `cmd_build.cpp` - Use new pipeline
- `parallel_build.cpp` - HIR-aware parallelism
- `test_runner.cpp` - Use new pipeline
- MIR builder - Accept HIR instead of AST

## Impact

- **Affected specs**: None (internal compiler change)
- **Affected code**:
  - New: `compiler/include/hir/`, `compiler/src/hir/`
  - Modified: `compiler/src/cli/`, `compiler/src/mir/`
- **Breaking change**: NO (internal change only)
- **User benefit**:
  - Faster incremental compilation
  - Better IDE support (hover, completion, go-to-definition)
  - More optimized output code
  - Better error messages with rich type context
