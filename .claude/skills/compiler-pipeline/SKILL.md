---
name: compiler-pipeline
description: Deep knowledge of TML compiler internals. Inject before working on compiler C++ code. Maps the full pipeline with key functions, types, and cross-subsystem boundaries.
user-invocable: false
---

# TML Compiler Pipeline — Deep Reference

This skill provides the detailed knowledge needed to work on compiler C++ code without losing cross-subsystem context.

## Pipeline Phases with Key Functions

### Phase 1: Lexer → Tokens
- **Entry**: `Lexer::tokenize()` in `compiler/src/lexer/lexer.cpp`
- **Input**: `Source` (UTF-8 text)
- **Output**: `std::vector<Token>`
- **Error**: `LexerError { message, span, code }`
- **Notable**: Template literals (backtick) produce `InterpStringStart`/`InterpStringEnd` tokens

### Phase 2: Parser → AST
- **Entry**: `Parser::parse_module()` in `compiler/src/parser/parser.cpp`
- **Input**: `std::vector<Token>`
- **Output**: `Module` (AST root node)
- **Split files**: `parser_expr.cpp` (expressions), `parser_decl.cpp` (declarations), `parser_stmt.cpp` (statements)
- **Parser type**: Pratt (operator precedence) for expressions, recursive descent for declarations
- **Key AST nodes**: `FuncDecl`, `StructDecl`, `EnumDecl`, `TraitDecl`, `ImplDecl`, `ClassDecl`

### Phase 3: Type Checker → TypeEnv
- **Entry**: `TypeChecker::check_module()` in `compiler/src/types/checker.cpp`
- **Input**: `Module` (AST)
- **Output**: `TypeEnv` (symbol table + all resolved types)
- **Split files**: `checker_decl.cpp`, `checker_expr.cpp`, `checker_stmt.cpp`, `checker_pattern.cpp`, `expr_call.cpp`, `expr_call_method.cpp`
- **4 sub-phases**:
  1. Declaration registration (collect type/function/behavior definitions)
  2. Use declaration processing (resolve imports)
  3. Impl block registration (register behavior implementations)
  4. Body checking (type check function bodies)
- **Inference**: Hindley-Milner with unification

### Phase 4: Borrow Checker → Validation
- **Entry**: `BorrowChecker::check_module()` in `compiler/src/borrow/checker.cpp`
- **Input**: Type-checked `Module` + `TypeEnv`
- **Output**: Validation result (pass/fail + errors)
- **Model**: NLL (Non-Lexical Lifetimes), place-based tracking
- **Error codes**: B001 (use after move) through B016

### Phase 5: HIR Lowering → HirModule
- **Entry**: `HirBuilder::lower_module()` in `compiler/src/hir/hir_builder.cpp`
- **Input**: Type-checked `Module` + `TypeEnv`
- **Output**: `HirModule` (desugared + monomorphized)
- **Split files**: `hir_builder_expr.cpp` (expression lowering)
- **Key transforms**:
  - All expressions get concrete resolved types from TypeEnv
  - Desugaring: `var x = e` → `let mut x = e`, `for` → iterator protocol, `if let` → `when`
  - **Monomorphization**: Generic types/functions instantiated with concrete types
  - Field/variant index resolution (names → numeric indices)
  - Closure capture analysis
- **Cache**: `MonomorphizationCache` prevents duplicate instantiations
- **Name mangling**: `Vec__I32`, `Map__Str_I32`

### Phase 6: THIR Lowering → ThirModule
- **Entry**: `ThirLower::lower_module()` in `compiler/src/thir/thir_lower.cpp`
- **Input**: `HirModule` + `TypeEnv` + `TraitSolver`
- **Output**: `ThirModule`
- **Key transforms**:
  - Insert explicit `CoercionExpr` nodes for implicit type coercions
  - Method resolution via trait solver
  - Operator desugaring to method calls (`a + b` → `a.add(b)`)
  - Pattern exhaustiveness checking on `when` expressions
  - Associated type normalization

### Phase 7: MIR Building → mir::Module

⚠️ **TWO ACTIVE PATHS** — changes often need to go in BOTH:

**Path A — HIR→MIR (legacy, `--legacy` flag):**
- Entry: `HirMirBuilder::build()` in `compiler/src/mir/hir_mir_builder.cpp`
- Expression handling: `compiler/src/mir/builder/hir_expr.cpp`
- Control flow: `compiler/src/mir/builder/hir_expr_control.cpp`
- Type mapping: `compiler/src/mir/builder/builder_types.cpp`

**Path B — THIR→MIR (new, default):**
- Entry: `ThirMirBuilder::build()` in `compiler/src/mir/thir_mir_builder.cpp`
- Expression handling: `compiler/src/mir/thir_mir_builder_expr.cpp`

**Output**: `mir::Module` containing:
- `structs: Vec<Struct>` — struct definitions
- `enums: Vec<Enum>` — enum definitions
- `functions: Vec<Function>` — each with basic blocks, instructions, terminators

**MIR is SSA form** — every value defined once, basic blocks have terminators (br, cond_br, ret, switch)

### Phase 8: MIR Passes → Optimized mir::Module
- **Entry**: `MirPassManager::run()` in `compiler/src/mir/mir_pass.cpp`
- **Input/Output**: `mir::Module` → `mir::Module`
- **Critical pass**: `mem2reg` (promotes allocas to SSA registers)
- **Other key passes**: dead_function_elimination, block_merge, constant_folding
- **Full list**: 30+ passes in `compiler/include/mir/passes/`

### Phase 9: MIR Codegen → LLVM IR Text
- **Entry**: `MirCodegen::generate()` in `compiler/src/codegen/mir_codegen.cpp`
- **Input**: `mir::Module`
- **Output**: `std::string` (LLVM IR text format)
- **Instruction emission**: `compiler/src/codegen/mir/instructions.cpp`
  - `emit_call_inst()` — ALL calls including method calls
  - `emit_store_inst()`, `emit_load_inst()`, `emit_gep_inst()`
  - `emit_cast_inst()`, `emit_binary_inst()`, `emit_unary_inst()`
- **Method calls**: `compiler/src/codegen/mir/instructions_method.cpp`
- **Misc**: `compiler/src/codegen/mir/instructions_misc.cpp` (phi, alloca, etc.)
- **Type mapping**: `compiler/src/codegen/mir/mir_types.cpp` (MIR types → LLVM type strings)

⚠️ **CRITICAL**: Method calls in MIR path produce `CallInst`, NOT `MethodCallInst`. All method dispatch goes through `emit_call_inst`.

### Phase 10: LLVM Backend → Object File
- **Entry**: `LLVMBackend::compile_ir_to_object()` in `compiler/src/backend/llvm_backend.cpp`
- **Input**: LLVM IR text
- **Output**: `.obj` file
- **Options**: O0-O3 optimization, debug info, target triple

### Phase 11: LLD Linker → Executable
- **Entry**: `LldLinker::link()` in `compiler/src/backend/lld_linker.cpp`
- **Input**: `.obj` files + runtime libraries
- **Output**: `.exe`

## Query System

All phases wrapped in queries for caching:
```
QueryContext::force(ReadSourceKey) → ReadSourceResult
QueryContext::force(TokenizeKey) → TokenizeResult
QueryContext::force(ParseModuleKey) → ParseModuleResult
...etc through CodegenUnitKey → CodegenUnitResult
```

File: `compiler/src/query/query_context.cpp`
Cache: In-memory + `.incr-cache/incr.bin` (fingerprint-based)

## Debugging Workflow

1. **Emit MIR**: `mcp__tml__emit-mir` — see the MIR instructions
2. **Emit IR**: `mcp__tml__emit-ir` — see the LLVM IR output
3. **Compare with Rust**: `rustc --emit=llvm-ir` for equivalent code
4. **Trace a value**: Source → TypeEnv (type?) → HIR (desugared?) → MIR (instruction?) → LLVM IR (emitted?)

## Build Notes

- `tml_compiler.dll` contains ALL phases up to and including IR generation
- `tml_codegen_x86.dll` contains ONLY IR→obj + LLD linking
- When fixing codegen bugs, rebuild `tml_compiler_plugin` (NOT `tml_codegen_x86_plugin`)
- Build command: `cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat" 2>&1`
