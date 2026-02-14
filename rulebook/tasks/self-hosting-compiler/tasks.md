# Tasks: Self-Hosting TML Compiler

**Status**: Proposed (0%)

**Scope**: Rewrite ~195K lines of C++ compiler in TML, achieve bootstrap self-compilation

**Prerequisite**: `migrate-runtime-to-tml` must be completed first

---

## Stage 0: Language Readiness — Prerequisites

> Before writing any compiler code in TML, the language must support
> everything a compiler needs: file I/O, hash maps with string keys,
> recursive data structures, LLVM FFI, and robust string processing.

### Phase 0.1: Verify Core Language Features

- [ ] 0.1.1 Test: recursive enum (AST-like tree with enum variants containing the same enum type)
- [ ] 0.1.2 Test: `HashMap[Str, T]` with 10K+ entries — insert, get, iterate, remove
- [ ] 0.1.3 Test: `List[T]` with polymorphic element types (List of enums)
- [ ] 0.1.4 Test: nested generics — `HashMap[Str, List[AstNode]]`
- [ ] 0.1.5 Test: pattern matching (`when`) on recursive enum with 20+ variants
- [ ] 0.1.6 Test: `Outcome[T, E]` error propagation through 5+ function call chain
- [ ] 0.1.7 Test: file read → process → file write round-trip (read .tml source, write .ll output)
- [ ] 0.1.8 Test: string concatenation performance — build 100K+ char string incrementally
- [ ] 0.1.9 Test: closures with variable capture (if implemented; if not, document workaround)
- [ ] 0.1.10 Fix any compiler bugs found in 0.1.1–0.1.9 before proceeding

### Phase 0.2: LLVM C API Bindings

> The LLVM C API (llvm-c/*.h) provides ~500 functions for building IR.
> TML must declare these as `@extern("c")` to call them.

- [ ] 0.2.1 Create `lib/std/src/llvm/mod.tml` — LLVM FFI module
- [ ] 0.2.2 Declare core types: `LLVMModuleRef`, `LLVMBuilderRef`, `LLVMValueRef`, `LLVMTypeRef`, `LLVMContextRef`, `LLVMBasicBlockRef`
- [ ] 0.2.3 Declare context functions: `LLVMContextCreate`, `LLVMContextDispose`
- [ ] 0.2.4 Declare module functions: `LLVMModuleCreateWithNameInContext`, `LLVMDisposeModule`, `LLVMPrintModuleToString`, `LLVMWriteBitcodeToFile`
- [ ] 0.2.5 Declare type functions: `LLVMInt1Type`, `LLVMInt8Type`, `LLVMInt32Type`, `LLVMInt64Type`, `LLVMFloatType`, `LLVMDoubleType`, `LLVMPointerType`, `LLVMVoidType`, `LLVMFunctionType`, `LLVMStructType`, `LLVMArrayType`
- [ ] 0.2.6 Declare function functions: `LLVMAddFunction`, `LLVMGetParam`, `LLVMSetLinkage`, `LLVMSetFunctionCallConv`
- [ ] 0.2.7 Declare builder functions: `LLVMCreateBuilderInContext`, `LLVMPositionBuilderAtEnd`, `LLVMDisposeBuilder`
- [ ] 0.2.8 Declare instruction builders: `LLVMBuildAdd`, `LLVMBuildSub`, `LLVMBuildMul`, `LLVMBuildICmp`, `LLVMBuildFCmp`, `LLVMBuildAlloca`, `LLVMBuildLoad2`, `LLVMBuildStore`, `LLVMBuildGEP2`, `LLVMBuildCall2`, `LLVMBuildRet`, `LLVMBuildRetVoid`, `LLVMBuildBr`, `LLVMBuildCondBr`, `LLVMBuildPhi`, `LLVMBuildSwitch`, `LLVMBuildBitCast`, `LLVMBuildTrunc`, `LLVMBuildZExt`, `LLVMBuildSExt`, `LLVMBuildFPToSI`, `LLVMBuildSIToFP`
- [ ] 0.2.9 Declare basic block functions: `LLVMAppendBasicBlockInContext`, `LLVMGetInsertBlock`
- [ ] 0.2.10 Declare constant functions: `LLVMConstInt`, `LLVMConstReal`, `LLVMConstStringInContext`, `LLVMConstNull`, `LLVMConstPointerNull`, `LLVMConstArray`, `LLVMConstStruct`
- [ ] 0.2.11 Declare global variable functions: `LLVMAddGlobal`, `LLVMSetInitializer`, `LLVMSetGlobalConstant`
- [ ] 0.2.12 Declare target machine functions: `LLVMGetDefaultTargetTriple`, `LLVMGetTargetFromTriple`, `LLVMCreateTargetMachine`, `LLVMTargetMachineEmitToFile`
- [ ] 0.2.13 Declare pass builder functions: `LLVMCreatePassBuilderOptions`, `LLVMRunPasses`
- [ ] 0.2.14 Declare analysis functions: `LLVMVerifyModule`, `LLVMVerifyFunction`
- [ ] 0.2.15 Write test: create LLVM module, add function, emit `ret i32 42`, verify, print IR
- [ ] 0.2.16 Write test: compile IR to object file via target machine
- [ ] 0.2.17 Write safe TML wrapper types: `Module`, `Builder`, `Value`, `Type_`, `Context` with drop semantics

### Phase 0.3: LLD Linker Bindings

- [ ] 0.3.1 Declare LLD entry points: `lld::coff::link`, `lld::elf::link`, `lld::macho::link` (or the C wrapper equivalents)
- [ ] 0.3.2 Write test: link a .obj file produced by Phase 0.2 into an executable
- [ ] 0.3.3 Write test: run the linked executable via `std::process`, verify output

### Phase 0.4: Utility Libraries for Compiler

- [ ] 0.4.1 Implement `StringInterner` — deduplicate strings, return integer IDs (symbol table foundation)
- [ ] 0.4.2 Implement `Arena[T]` allocator — bulk allocation for AST/IR nodes, free all at once
- [ ] 0.4.3 Implement `BitSet` — compact set of integers for data-flow analysis
- [ ] 0.4.4 Implement `IndexVec[I, T]` — type-safe indexed vector (like rustc's `IndexVec`)
- [ ] 0.4.5 Implement `Span` struct — source location tracking (file_id, start_offset, end_offset)
- [ ] 0.4.6 Implement `DiagnosticEmitter` — structured error/warning output with source snippets
- [ ] 0.4.7 Test all utility libraries

---

## Stage 1: Frontend — Lexer + Parser in TML

> Target: ~12,500 C++ lines → TML
> The lexer and parser are self-contained, well-defined, and testable in isolation.

### Phase 1.1: Lexer (compiler/src/lexer/ → compiler-tml/lexer/)

> C++ source: lexer.cpp (1,293), token.cpp (363), keywords.cpp (295), source_map.cpp (365)
> C++ headers: lexer.hpp (373), token.hpp (364), keywords.hpp (130)
> Total: ~3,183 lines

- [ ] 1.1.1 Create `compiler-tml/` directory structure
- [ ] 1.1.2 Define `Token` enum with all 80+ token kinds (keywords, operators, literals, punctuation)
- [ ] 1.1.3 Define `TokenKind` classification (keyword, operator, literal, ident, whitespace, comment)
- [ ] 1.1.4 Define `Span` integration for source location on each token
- [ ] 1.1.5 Implement keyword recognition — lookup table of 32 TML keywords
- [ ] 1.1.6 Implement single-character tokenization: `(`, `)`, `[`, `]`, `{`, `}`, `,`, `;`, `:`, `.`, `@`
- [ ] 1.1.7 Implement multi-character operators: `==`, `!=`, `<=`, `>=`, `->`, `=>`, `::`, `..`, `+=`, `-=`, `*=`, `/=`, `%=`
- [ ] 1.1.8 Implement number literal lexing: integers (decimal, hex `0x`, octal `0o`, binary `0b`), floats, underscores as separators
- [ ] 1.1.9 Implement string literal lexing: `"..."` with escape sequences (`\n`, `\t`, `\\`, `\"`, `\0`, `\x##`, `\u{####}`)
- [ ] 1.1.10 Implement string interpolation: `"hello {expr}"` — emit StringStart, Expr tokens, StringEnd
- [ ] 1.1.11 Implement character literal lexing: `'a'`, `'\n'`, `'\u{2764}'`
- [ ] 1.1.12 Implement comment skipping: `//` line comments, `/* */` block comments (nested)
- [ ] 1.1.13 Implement identifier lexing with UTF-8 support
- [ ] 1.1.14 Implement source map: track line/column for each token
- [ ] 1.1.15 Write test: lex every keyword (32 tokens)
- [ ] 1.1.16 Write test: lex every operator type
- [ ] 1.1.17 Write test: lex number edge cases (0xFF, 1_000_000, 3.14e-10, 0b1010)
- [ ] 1.1.18 Write test: lex string edge cases (escapes, interpolation, multiline)
- [ ] 1.1.19 Write test: lex real .tml source file, compare token stream with C++ lexer output
- [ ] 1.1.20 Write test: lex ALL files in lib/core/src/ — must produce identical token counts

### Phase 1.2: Preprocessor (compiler/src/preprocessor/ → compiler-tml/preprocessor/)

> C++ source: preprocessor.cpp (819), preprocessor.hpp (349)
> Total: ~1,168 lines

- [ ] 1.2.1 Implement `#if`, `#elif`, `#else`, `#endif` directive parsing
- [ ] 1.2.2 Implement `#ifdef`, `#ifndef` shorthand
- [ ] 1.2.3 Implement `defined()` operator
- [ ] 1.2.4 Implement boolean expressions: `&&`, `||`, `!`, parentheses
- [ ] 1.2.5 Implement predefined symbols: `WINDOWS`, `LINUX`, `MACOS`, `X86_64`, `ARM64`, `DEBUG`, `RELEASE`, `TEST`
- [ ] 1.2.6 Implement `-D` flag support (user-defined symbols)
- [ ] 1.2.7 Write test: conditional compilation with platform symbols
- [ ] 1.2.8 Write test: nested `#if`/`#elif`/`#else` chains

### Phase 1.3: Parser (compiler/src/parser/ → compiler-tml/parser/)

> C++ source: parser.cpp (2,148), expr_parser.cpp (1,243), decl_parser.cpp (897), stmt_parser.cpp (634), type_parser.cpp (478), pattern_parser.cpp (389), error_recovery.cpp (141)
> C++ headers: parser.hpp (892), ast.hpp (1,478), ast_visitor.hpp (184)
> Total: ~9,384 lines

- [ ] 1.3.1 Define AST node types as TML enums:
  - Declarations: `FuncDecl`, `StructDecl`, `EnumDecl`, `BehaviorDecl`, `ImplDecl`, `TypeAlias`, `ConstDecl`, `UseDecl`, `ModDecl`
  - Statements: `LetStmt`, `ReturnStmt`, `ExprStmt`, `AssignStmt`, `WhileStmt`, `LoopStmt`, `BreakStmt`, `ContinueStmt`, `BlockStmt`
  - Expressions: `IntLiteral`, `FloatLiteral`, `StringLiteral`, `BoolLiteral`, `Identifier`, `BinaryExpr`, `UnaryExpr`, `CallExpr`, `MethodCallExpr`, `FieldAccessExpr`, `IndexExpr`, `IfExpr`, `WhenExpr`, `BlockExpr`, `LambdaExpr`, `RangeExpr`, `CastExpr`, `TupleExpr`, `ArrayExpr`, `StructExpr`
  - Patterns: `LiteralPattern`, `IdentPattern`, `WildcardPattern`, `TuplePattern`, `StructPattern`, `EnumPattern`, `RangePattern`, `OrPattern`
  - Types: `NamedType`, `GenericType`, `RefType`, `MutRefType`, `PtrType`, `ArrayType`, `TupleType`, `FuncType`, `NeverType`
- [ ] 1.3.2 Implement Pratt parser for expressions:
  - Binding powers for: `or` (1), `and` (2), `==`/`!=` (3), `<`/`>`/`<=`/`>=` (4), `+`/`-` (5), `*`/`/`/`%` (6), unary `-`/`not`/`ref`/`mut ref` (7), `.`/`::`/`()` /`[]` (8)
  - Prefix parsing: literals, identifiers, unary ops, grouping `()`, `if`, `when`, `do`, `[`, block `{`
  - Infix parsing: binary ops, method calls `.name()`, field access `.field`, indexing `[idx]`, function calls `(args)`, as-cast, range `to`/`through`
- [ ] 1.3.3 Implement declaration parsing:
  - `func name[T](params) -> ReturnType { body }`
  - `struct Name[T] { fields }`
  - `enum Name[T] { Variant1(T), Variant2 { field: T } }`
  - `behavior Name[T] { func methods... }`
  - `impl[T] Name[T] { func methods... }`
  - `extend Type with Behavior { ... }`
  - `type Alias = Existing[T]`
  - `const NAME: Type = expr`
  - `use module::path`
  - `mod name`
- [ ] 1.3.4 Implement statement parsing:
  - `let name: Type = expr`
  - `let mut name = expr`
  - `return expr`
  - `loop { ... }`, `loop name in iterable { ... }`
  - `break`, `continue`
  - Assignment: `name = expr`, `name += expr`
- [ ] 1.3.5 Implement type parsing:
  - Named: `I32`, `Str`, `MyStruct`
  - Generic: `List[T]`, `HashMap[K, V]`, `Outcome[T, E]`
  - Reference: `ref T`, `mut ref T`
  - Pointer: `*T`, `*mut T`
  - Array: `[T; N]`
  - Tuple: `(T1, T2, T3)`
  - Function: `func(T1, T2) -> R`
  - Never: `Never`
- [ ] 1.3.6 Implement pattern parsing:
  - Literals, identifiers, wildcards `_`
  - Tuples `(a, b)`, structs `Point { x, y }`
  - Enums `Just(x)`, `Err(e)`, `Nothing`
  - Ranges `0 to 10`, `'a' through 'z'`
  - Or-patterns `A | B | C`
- [ ] 1.3.7 Implement error recovery: sync on `;`, `}`, `func`, `struct`, `enum`
- [ ] 1.3.8 Implement directive parsing: `@test`, `@bench`, `@inline`, `@extern`, `@intrinsic`, `@derive`, `@deprecated`, `@cfg`
- [ ] 1.3.9 Write test: parse every declaration type
- [ ] 1.3.10 Write test: parse operator precedence edge cases
- [ ] 1.3.11 Write test: parse real .tml files, compare AST structure with C++ parser
- [ ] 1.3.12 Write test: parse ALL of lib/core/src/ — zero parse errors

### Phase 1.4: AST Printer & Validation

- [ ] 1.4.1 Implement AST pretty-printer (TML source → parse → print → must be valid TML)
- [ ] 1.4.2 Implement AST visitor pattern for tree traversal
- [ ] 1.4.3 Write round-trip test: parse → print → re-parse → compare ASTs
- [ ] 1.4.4 Cross-validate: C++ parser and TML parser produce equivalent ASTs on full test suite

---

## Stage 2: Middle-End — Type Checker + HIR + Codegen

> Target: ~87,400 C++ lines → TML
> This is the core of the compiler. Requires robust HashMap, recursive data structures, LLVM FFI.

### Phase 2.1: Type System Data Structures

> C++ source: types/ directory (~20,205 lines)

- [ ] 2.1.1 Define `Type` enum:
  - Primitives: `I8`, `I16`, `I32`, `I64`, `I128`, `U8`, `U16`, `U32`, `U64`, `U128`, `F32`, `F64`, `Bool`, `Char`, `Str`, `Unit`, `Never`
  - Compound: `Struct(StructId)`, `Enum(EnumId)`, `Tuple(List[Type])`, `Array(Type, I64)`, `Slice(Type)`
  - Reference: `Ref(Type)`, `MutRef(Type)`, `Ptr(Type)`
  - Function: `Func(List[Type], Type)`
  - Generic: `TypeVar(TypeVarId)`, `GenericInst(TypeId, List[Type])`
  - Special: `Inferred`, `Error`
- [ ] 2.1.2 Define `TypeContext` — owns all types, provides interning (deduplication)
- [ ] 2.1.3 Define `SymbolTable` — nested scopes, name→TypeId mapping using `HashMap[Str, TypeId]`
- [ ] 2.1.4 Define `TraitRegistry` — behavior→methods mapping, impl resolution

### Phase 2.2: Type Checker

- [ ] 2.2.1 Implement type inference for `let` bindings (Hindley-Milner)
- [ ] 2.2.2 Implement unification algorithm (Type ↔ Type matching with type variables)
- [ ] 2.2.3 Implement function call type checking (argument types, return type, generic instantiation)
- [ ] 2.2.4 Implement method resolution (find impl block for type + method name)
- [ ] 2.2.5 Implement behavior (trait) bound checking — `where T: Hash + Eq`
- [ ] 2.2.6 Implement numeric literal inference (`42` → I32 default, I64 if needed)
- [ ] 2.2.7 Implement auto-deref and coercion rules
- [ ] 2.2.8 Implement generic instantiation / monomorphization decisions
- [ ] 2.2.9 Implement exhaustiveness checking for `when` expressions (Maranget algorithm)
- [ ] 2.2.10 Implement const evaluation (compile-time constant folding)
- [ ] 2.2.11 Write test: infer types for all numeric types
- [ ] 2.2.12 Write test: generic function instantiation with 3+ type parameters
- [ ] 2.2.13 Write test: behavior bounds resolution with multiple constraints
- [ ] 2.2.14 Write test: type check ALL of lib/core/src/ — zero type errors

### Phase 2.3: HIR Lowering (AST → HIR)

> C++ source: hir/ directory (~14,295 lines)

- [ ] 2.3.1 Define HIR node types (type-annotated, desugared AST)
- [ ] 2.3.2 Implement AST→HIR lowering: desugar `for`→`loop`, `if let`→`when`, `+=`→`assign(add)`
- [ ] 2.3.3 Implement monomorphization: generic functions → concrete instantiations
- [ ] 2.3.4 Implement coercion materialization: insert explicit casts where implicit coercions occur
- [ ] 2.3.5 Implement method resolution: `obj.method()` → `Type::method(obj)`
- [ ] 2.3.6 Implement derive expansion: `@derive(Debug, Duplicate)` → generated impl blocks
- [ ] 2.3.7 Write test: round-trip HIR → verify all types are resolved

### Phase 2.4: LLVM Code Generation (HIR → LLVM IR)

> C++ source: codegen/ directory (~52,907 lines) — LARGEST component
> This wraps the LLVM C API bindings from Phase 0.2

- [ ] 2.4.1 Implement module-level codegen: create LLVM module, declare runtime functions
- [ ] 2.4.2 Implement function codegen: prologue, parameter setup, body, epilogue, return
- [ ] 2.4.3 Implement type mapping: TML types → LLVM types (`I32`→`i32`, `Str`→`ptr`, `struct`→`%struct.Name`)
- [ ] 2.4.4 Implement literal codegen: integers, floats, booleans, strings (global constants), null
- [ ] 2.4.5 Implement variable codegen: alloca, load, store for local variables
- [ ] 2.4.6 Implement arithmetic codegen: `add`, `sub`, `mul`, `sdiv`, `srem`, `fadd`, `fsub`, `fmul`, `fdiv`
- [ ] 2.4.7 Implement comparison codegen: `icmp eq/ne/slt/sgt/sle/sge`, `fcmp oeq/one/olt/ogt`
- [ ] 2.4.8 Implement logical codegen: `and`→short-circuit branch, `or`→short-circuit branch, `not`→`xor true`
- [ ] 2.4.9 Implement control flow: `if`→`br`, `loop`→back-edge `br`, `break`→`br` to after-loop, `return`→`ret`
- [ ] 2.4.10 Implement function call codegen: argument preparation, `call`, result handling
- [ ] 2.4.11 Implement method call codegen: resolve impl, call with self parameter
- [ ] 2.4.12 Implement struct codegen: `%struct.Name` type, `getelementptr` for field access, constructor
- [ ] 2.4.13 Implement enum codegen: tagged union (discriminant + payload), variant construction, pattern match dispatch
- [ ] 2.4.14 Implement `when` codegen: decision tree → series of branches and comparisons
- [ ] 2.4.15 Implement closure codegen: environment struct capture, function pointer + env pointer pair
- [ ] 2.4.16 Implement string interpolation codegen: concatenate parts via `str_concat`
- [ ] 2.4.17 Implement array/slice codegen: stack allocation, bounds checking, GEP access
- [ ] 2.4.18 Implement memory operations: `mem_alloc`/`mem_free` calls, `ptr_read`/`ptr_write` via load/store
- [ ] 2.4.19 Implement builtin function codegen: `print`, `println`, `panic`, `assert`
- [ ] 2.4.20 Implement runtime function declarations: declare all C runtime functions used
- [ ] 2.4.21 Write test: compile and run "Hello World" program end-to-end
- [ ] 2.4.22 Write test: compile and run fibonacci(30) — verify correctness
- [ ] 2.4.23 Write test: compile and run program using List[I32].push/get
- [ ] 2.4.24 Write test: compile and run program using HashMap[Str, I64]
- [ ] 2.4.25 Write test: compile and run program with pattern matching on enum

### Phase 2.5: Backend Integration

- [ ] 2.5.1 Implement LLVM module verification (call `LLVMVerifyModule`)
- [ ] 2.5.2 Implement optimization pipeline (call `LLVMRunPasses` with "default<O2>")
- [ ] 2.5.3 Implement object file emission (call `LLVMTargetMachineEmitToFile`)
- [ ] 2.5.4 Implement linking via LLD FFI (pass object files → produce executable)
- [ ] 2.5.5 Write test: full pipeline — .tml source → tokens → AST → HIR → LLVM IR → .obj → .exe → run → verify output

### Phase 2.6: CLI (Minimal)

- [ ] 2.6.1 Implement argument parsing: `tml-self build file.tml [-o output]`
- [ ] 2.6.2 Implement file reading: read source file into string
- [ ] 2.6.3 Implement error display: show file:line:column with source snippet
- [ ] 2.6.4 Implement pipeline orchestration: lex → parse → check → lower → codegen → link
- [ ] 2.6.5 Write test: `tml-self build hello.tml` produces working executable

---

## Stage 3: Advanced — MIR Optimizations + Borrow Checker

> Target: ~43,500 C++ lines → TML
> These are complex but can be added incrementally.
> The compiler works without them (just produces less optimized / less safe code).

### Phase 3.1: MIR Construction

> C++ source: mir/ directory (~36,951 lines, of which ~18K is optimization passes)

- [ ] 3.1.1 Define MIR data structures: `BasicBlock`, `Statement`, `Terminator`, `Place`, `Operand`, `Rvalue`
- [ ] 3.1.2 Implement HIR → MIR lowering (convert tree to SSA-like CFG)
- [ ] 3.1.3 Implement SSA construction (mem2reg: allocas → phi nodes)
- [ ] 3.1.4 Implement MIR → LLVM IR codegen (replaces direct HIR→LLVM from Stage 2)
- [ ] 3.1.5 Write test: MIR round-trip — same output as direct codegen

### Phase 3.2: MIR Optimization Passes (port from C++ one by one)

> 49 passes in C++. Priority order (most impactful first):

**Tier A — High impact, implement first:**
- [ ] 3.2.1 Dead code elimination
- [ ] 3.2.2 Constant folding and propagation
- [ ] 3.2.3 Copy propagation
- [ ] 3.2.4 Function inlining (small functions)
- [ ] 3.2.5 Control flow simplification (unreachable blocks, empty blocks)
- [ ] 3.2.6 Mem2reg (promote memory to registers)

**Tier B — Medium impact:**
- [ ] 3.2.7 Common subexpression elimination
- [ ] 3.2.8 Loop invariant code motion
- [ ] 3.2.9 Jump threading
- [ ] 3.2.10 Tail call optimization
- [ ] 3.2.11 Bounds check elimination
- [ ] 3.2.12 Devirtualization (static dispatch for known types)

**Tier C — Low priority, add later:**
- [ ] 3.2.13 Loop unrolling
- [ ] 3.2.14 Loop rotation
- [ ] 3.2.15 Vectorization hints
- [ ] 3.2.16 Escape analysis (stack promotion)
- [ ] 3.2.17 Alias analysis
- [ ] 3.2.18 Global value numbering
- [ ] 3.2.19 Strength reduction
- [ ] 3.2.20 Remaining 30 passes from C++ (port as needed)

### Phase 3.3: Borrow Checker

> C++ source: borrow/ directory (~6,579 lines)
> Can start with simplified version, add Polonius later.

- [ ] 3.3.1 Implement lifetime tracking: assign regions to borrows
- [ ] 3.3.2 Implement borrow rules: at most one `mut ref` OR any number of `ref`, never both
- [ ] 3.3.3 Implement move semantics: detect use-after-move
- [ ] 3.3.4 Implement NLL (Non-Lexical Lifetimes): borrows end at last use, not end of scope
- [ ] 3.3.5 Implement error messages: "cannot borrow X as mutable because it is also borrowed as immutable"
- [ ] 3.3.6 Write test: borrow checker accepts valid code, rejects invalid code
- [ ] 3.3.7 Write test: borrow check ALL of lib/core/src/ — zero false positives
- [ ] 3.3.8 (Optional) Implement Polonius-style analysis for more permissive checking

### Phase 3.4: Query System (Incremental Compilation)

> C++ source: query/ directory (~2,736 lines)
> Optimization — not required for correctness.

- [ ] 3.4.1 Implement query memoization: cache results of each compilation stage
- [ ] 3.4.2 Implement fingerprinting: hash-based change detection for source files
- [ ] 3.4.3 Implement dependency tracking: which queries depend on which inputs
- [ ] 3.4.4 Implement incremental recompilation: only re-run queries whose inputs changed
- [ ] 3.4.5 Implement cache persistence: save/load fingerprints and results to disk
- [ ] 3.4.6 Write test: modify one file, verify only affected modules recompile

---

## Stage 4: Tooling — CLI, Test Runner, Formatter, Linter

> Target: ~45,700 C++ lines → TML
> Quality-of-life tools needed for a complete compiler distribution.

### Phase 4.1: Full CLI

> C++ source: cli/commands/ (~3,752 lines), cli/builder/ (~8,977 lines)

- [ ] 4.1.1 Implement `tml build file.tml` — full compilation pipeline
- [ ] 4.1.2 Implement `tml build --release` — with optimization passes
- [ ] 4.1.3 Implement `tml build --emit-ir` — dump LLVM IR
- [ ] 4.1.4 Implement `tml build --emit-mir` — dump MIR
- [ ] 4.1.5 Implement `tml run file.tml` — build + execute
- [ ] 4.1.6 Implement `tml check file.tml` — type check only, no codegen
- [ ] 4.1.7 Implement `tml build --crate-type=lib/dylib/rlib` — library builds
- [ ] 4.1.8 Implement parallel compilation: multiple codegen units
- [ ] 4.1.9 Implement build caching: skip unchanged files

### Phase 4.2: Test Runner

> C++ source: cli/tester/ (~12,899 lines)

- [ ] 4.2.1 Implement test discovery: find `@test` functions in .tml files
- [ ] 4.2.2 Implement test compilation: compile tests as DLLs
- [ ] 4.2.3 Implement test execution: load DLL, call test functions, catch panics
- [ ] 4.2.4 Implement test output: pass/fail counts, failure messages, timing
- [ ] 4.2.5 Implement `@should_panic` tests
- [ ] 4.2.6 Implement `@bench` benchmarks
- [ ] 4.2.7 Implement `--coverage` mode
- [ ] 4.2.8 Implement `--filter` for running specific tests
- [ ] 4.2.9 Write test: self-hosted test runner produces same results as C++ test runner on full suite

### Phase 4.3: Code Formatter

> C++ source: format/ (~1,169 lines)

- [ ] 4.3.1 Implement `tml format file.tml` — format source code
- [ ] 4.3.2 Implement `tml format --check` — verify formatting without modifying
- [ ] 4.3.3 Write test: format all lib/ files — output matches C++ formatter

### Phase 4.4: Linter

> C++ source: cli/linter/ (~1,347 lines)

- [ ] 4.4.1 Implement `tml lint file.tml` — check for style issues
- [ ] 4.4.2 Implement `tml lint --fix` — auto-fix where possible
- [ ] 4.4.3 Port lint rules from C++ implementation

---

## Stage 5: Bootstrap — Self-Compilation

> The moment the TML compiler can compile itself.

### Phase 5.1: Self-Compilation Attempt

- [ ] 5.1.1 Attempt: use C++ compiler to compile TML compiler → `tml_stage1.exe`
- [ ] 5.1.2 Attempt: use `tml_stage1.exe` to compile TML compiler → `tml_stage2.exe`
- [ ] 5.1.3 Fix: any compilation errors in Stage 1→2 (these reveal compiler bugs or missing features)
- [ ] 5.1.4 Iterate until `tml_stage2.exe` is produced successfully

### Phase 5.2: Bootstrap Validation

- [ ] 5.2.1 Use `tml_stage2.exe` to compile TML compiler → `tml_stage3.exe`
- [ ] 5.2.2 Verify: `tml_stage2.exe` and `tml_stage3.exe` produce identical output (byte compare of binaries OR compare test suite output)
- [ ] 5.2.3 Run full test suite (6,400+ tests) with `tml_stage2.exe` — all must pass
- [ ] 5.2.4 Run full test suite with `tml_stage3.exe` — all must pass
- [ ] 5.2.5 Verify: both stage executables produce identical test results

### Phase 5.3: Performance Validation

- [ ] 5.3.1 Benchmark: C++ compiler vs TML compiler — compile time for full test suite
- [ ] 5.3.2 Benchmark: C++ compiler vs TML compiler — compile time for large project
- [ ] 5.3.3 Target: TML compiler within 3x of C++ compiler speed
- [ ] 5.3.4 Profile: identify hotspots in TML compiler if >3x slower
- [ ] 5.3.5 Optimize: apply TML-specific optimizations to hot paths

### Phase 5.4: Transition

- [ ] 5.4.1 Set up CI: bootstrap chain runs automatically (C++→Stage1→Stage2→verify)
- [ ] 5.4.2 Document bootstrap procedure in README
- [ ] 5.4.3 Ship pre-built `tml.exe` binaries for each platform
- [ ] 5.4.4 Mark C++ compiler as "bootstrap only" — no new features added
- [ ] 5.4.5 All future compiler development happens in TML
