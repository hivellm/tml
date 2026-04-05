# TML Compiler: Complete Subsystem Inventory

**Date**: 2026-04-05
**Measurement basis**: Source lines counted with `wc -l` on all `.cpp` and `.hpp`/`.h` files.
**LOC includes**: All code, comments, blank lines — reflects actual file size.
**Estimated TML LOC**: 65% of C++ LOC, based on TML's expressiveness relative to C++ (fewer type annotations, template boilerplate eliminated, enum variants more concise, no header/source split).

---

## Table of Contents

1. [Lexer](#1-lexer)
2. [Parser](#2-parser)
3. [Type Checker](#3-type-checker)
4. [Borrow Checker](#4-borrow-checker)
5. [HIR Builder](#5-hir-builder)
6. [THIR Lowerer](#6-thir-lowerer)
7. [MIR Builder + Passes](#7-mir-builder--passes)
8. [MIR Codegen (IR Text Generator)](#8-mir-codegen-ir-text-generator)
9. [Legacy LLVM IR Generator](#9-legacy-llvm-ir-generator)
10. [LLVM Backend](#10-llvm-backend)
11. [LLD Linker](#11-lld-linker)
12. [Query System](#12-query-system)
13. [CLI](#13-cli)
14. [Testing Framework](#14-testing-framework)
15. [Formatter](#15-formatter)
16. [Plugin / Launcher](#16-plugin--launcher)
17. [Summary Tables](#17-summary-tables)

---

## 1. Lexer

### Purpose
Transforms raw source bytes into a flat sequence of `Token` values. Handles UTF-8, string escape sequences, numeric literals (decimal, hex, octal, binary, float), template literals (`` `hello {name}` ``), doc comments (`///`), and all TML operators and keywords.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/lexer/) | 2,830 | 9 |
| Headers (compiler/include/lexer/) | 869 | 3 |
| **Total** | **3,699** | **12** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| lexer_core.cpp | 538 | Main dispatch loop, token classification |
| lexer_string.cpp | 753 | String literal parsing, escape sequences, template literals |
| lexer_number.cpp | 375 | Integer and float literal parsing (all bases) |
| lexer_operator.cpp | 240 | Operator tokenization, multi-char operators (`->`, `..=`, `?.`) |
| lexer_ident.cpp | 176 | Identifier scanning, keyword lookup table |
| token.cpp | 402 | Token display, debug formatting |
| lexer_utils.cpp | 130 | Position tracking, Unicode helpers |
| token.hpp | 869 | Token struct, TokenKind enum (170+ variants), keyword map |

### Key Data Structures
- `struct Token { TokenKind kind; Span span; TokenValue value; }` — 367 lines in token.hpp
- `TokenKind` — enum of 170+ variants covering all keywords, operators, literals
- `TokenValue` — union of string, integer, float, char payload
- `Span { u32 start; u32 end; u32 line; u32 col; }` — source location

### Complexity: Simple
The lexer is a single-pass character scanner with no recursion. The most complex part is string/template literal handling (nested `{}` interpolations, escape sequences). No external dependencies.

### External Dependencies: None

### Self-Hosting Difficulty: Easy
The lexer is the simplest subsystem to port. All required TML stdlib features exist: `Str` for input, iteration over `Char` values, `List[Token]` for output, pattern matching for character dispatch. Template literal scanning (tracking nesting depth) requires a simple counter. The keyword map is a `HashMap[Str, TokenKind]`.

### Estimated TML LOC: ~2,400

### Key Porting Challenges
- The keyword-to-`TokenKind` lookup currently uses a `std::unordered_map<std::string_view, TokenKind>` initialized at startup. In TML, use `HashMap[Str, TokenKind]` — already available.
- Template literal scanning requires tracking `{` nesting depth to find the closing `}` of an interpolation. This is straightforward with a counter variable.
- `lexer_string.cpp` handles all Unicode escape sequences (`\u{...}`). The TML `char` module already provides Unicode validation, so this can reuse existing TML functionality.

---

## 2. Parser

### Purpose
Transforms the flat token stream into a Module AST (Abstract Syntax Tree). Uses Pratt parsing for expressions (handles precedence and associativity without a grammar table), and recursive descent for declarations. Produces a `Module` containing all top-level declarations: functions, structs, enums, behaviors, impls, use declarations.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/parser/) | 6,327 | 10 |
| Headers (compiler/include/parser/) | 3,499 | 9 |
| **Total** | **9,826** | **19** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| parser_decl.cpp | 1,010 | Function, struct, enum, behavior, use declarations |
| parser_expr.cpp | 1,002 | Expression Pratt parser (primary expressions) |
| parser_expr_complex.cpp | 885 | Complex expressions: closures, when, loop, blocks, if-let |
| parser_decl_impl.cpp | 868 | `impl Behavior for Type` block parsing |
| parser_oop.cpp | 729 | OOP class/extends/implements parsing |
| parser_core.cpp | 617 | Core infrastructure: token consume/peek, error recovery |
| parser_type.cpp | 545 | Type expression parsing (`List[T]`, `Maybe[T]`, fn types) |
| parser_pattern.cpp | 359 | Pattern parsing for `when` arms and `let`-destructuring |
| parser_stmt.cpp | 215 | Statement parsing (let, return, while, for) |
| ast.hpp | 3,499 | Full AST node definitions (~150 struct/enum types) |

### Key Data Structures
- `struct Module { Vec<Decl> decls; ModulePath path; }` — top-level AST
- `Decl` — variant type covering FuncDecl, StructDecl, EnumDecl, BehaviorDecl, ImplDecl, UseDecl
- `Expr` — 40+ variants (Literal, Binary, Unary, Call, MethodCall, Block, When, Loop, Closure, etc.)
- `Stmt` — Let, Expr, Return, Continue, Break
- `Pattern` — Wildcard, Binding, Struct, Enum, Tuple, Or, Range
- `TypeExpr` — Named, Generic, Fn, Tuple, Array, Slice, Ref, MutRef

### Complexity: Medium
The parser is well-structured: Pratt expression parsing is a mature technique. The complexity comes from the richness of TML's syntax (when-expressions, let-else, optional chaining `?.`, template literals, OOP class syntax, pattern matching). Error recovery is implemented but not deeply tested.

### External Dependencies: None (depends only on lexer output)

### Self-Hosting Difficulty: Easy
Pratt parsers and recursive descent are well-understood. TML enums with data (e.g., `Expr::Binary { op: BinOp, lhs: Heap[Expr], rhs: Heap[Expr] }`) map directly from C++ `std::variant`. The `ast.hpp` file's 3,499 lines of AST node definitions translate directly to TML enum/struct declarations — likely shrinking to ~1,800 lines in TML due to reduced boilerplate.

### Estimated TML LOC: ~6,400

### Key Porting Challenges
- The C++ AST uses `std::variant` + `std::unique_ptr` (owned recursive nodes). In TML, use `Heap[Expr]` for owned recursive fields.
- `ast.hpp` defines ~150 struct types inline in headers. In TML, these become a mix of structs and enum variants — need to decide the representation upfront.
- The parser has basic error recovery (skip tokens until a known synchronization point). The TML port should preserve this to give useful error messages.
- `parser_oop.cpp` (729 LOC) handles OOP-specific syntax (`class`, `extends`, `implements`, `abstract`). This will need careful attention as it is the most TML-specific syntactic feature.

---

## 3. Type Checker

### Purpose
Resolves all types in the program: infers expression types, resolves method calls, checks behavior implementations, monomorphizes generics, and builds the `TypeEnv` (symbol table). Operates in 4 phases: (1) register top-level declarations, (2) resolve imports, (3) check impl blocks, (4) check function bodies. Also handles binary module serialization for incremental compilation.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/types/) | 21,179 | 39 |
| Headers (compiler/include/types/) | 2,137 | 8 |
| **Total** | **23,316** | **47** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| checker/core.cpp | 1,412 | Main check dispatch, function body checking |
| module_binary_read.cpp | 1,409 | Binary module (.tml.meta) deserialization |
| checker/expr_call_method.cpp | 1,363 | Method call resolution, behavior dispatch |
| env_lookups.cpp | 1,265 | Type environment symbol lookup |
| env_module_load_decls.cpp | 1,253 | Loading external module declarations |
| checker/decl_struct.cpp | 1,207 | Struct and enum declaration checking |
| checker/core_oop.cpp | 1,067 | OOP class type checking (vtable, inheritance) |
| type.cpp | 965 | Type representation operations |
| env_module_loading.cpp | 875 | Module search path resolution |
| checker/expr_call.cpp | 802 | Free function call resolution |
| module_binary.cpp | 799 | Binary module (.tml.meta) serialization |
| module_metadata.cpp | 740 | Module dependency metadata |
| checker/types_checker.cpp | 678 | Type unification, subtype checking |

### Key Data Structures
- `struct Type` (293 lines in types/type.hpp) — tagged union representing all TML types: primitive, named, generic, function, tuple, array, slice, ref, behavior object
- `TypeEnv` — the symbol table: maps names to `Type` + `Span` + visibility info
- `Module` — compiled module representation (AST + resolved types + exports)
- `TypeConstraint` — Hindley-Milner constraint (Equality, BehaviorBound)
- `GenericEnv` — per-instantiation substitution map `{TypeParam -> Type}`

### Complexity: Very Complex
The type checker is the most algorithmically complex subsystem. It implements:
- Hindley-Milner type inference with let-polymorphism
- Behavior (trait) bounds and satisfaction checking
- Generic monomorphization with memoization
- Method resolution with multiple impl blocks
- Cross-module symbol resolution with re-exports
- Binary module format (`.tml.meta`) for incremental compilation
- OOP vtable layout and virtual dispatch type checking

### External Dependencies: None (self-contained, reads binary module files)

### Self-Hosting Difficulty: Hard
This is the highest-complexity subsystem. The `struct Type` C++ union with 15+ variants maps naturally to a TML enum. The `TypeEnv` maps to `HashMap[Str, TypeInfo]`. The constraint solving loop is straightforward. The difficulty is the sheer volume of logic (23,316 LOC across 47 files) and the number of interacting subsystems. Recommendation: port last among the core pipeline, or keep C++ type checker for an extended period during the bootstrap.

### Estimated TML LOC: ~15,100

### Key Porting Challenges
- `module_binary_read.cpp` + `module_binary.cpp` (2,208 LOC total): binary module format needs to be preserved for incremental compilation. TML's `Buffer` + `File` APIs are sufficient for reading/writing binary formats.
- Method resolution (`expr_call_method.cpp`, 1,363 LOC) has complex interactions between behavior implementations, generic substitution, and cross-module dispatch. This logic is fragile and heavily tested — the 1,700+ test suite is essential here.
- `env_lookups.cpp` (1,265 LOC) implements the scope chain lookup. In TML, this maps to nested `HashMap[Str, TypeInfo]` scopes, with parent-pointer chaining using `Shared[ScopeFrame]`.
- The OOP subsystem (`checker/core_oop.cpp`, 1,067 LOC) handles `class`/`extends`/`vtable` layout — this is TML-specific and has no Rust/C++ parallel to reference.
- The `type_implements` function has a known false-positive bug (returns true for types that don't implement `Ord`). This bug must be fixed before or during the port.

---

## 4. Borrow Checker

### Purpose
Verifies memory safety rules: no use after free, no aliasing violations, correct lifetime bounds. Uses Non-Lexical Lifetimes (NLL) — the same model as Rust's NLL borrow checker. Also includes a Polonius-based constraint solver for complex cases. Operates on the AST after type checking.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/borrow/) | 4,971 | 9 |
| Headers (compiler/include/borrow/) | 1,629 | 2 |
| **Total** | **6,600** | **11** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| checker_expr.cpp | 1,032 | Expression borrow checking |
| checker_ops.cpp | 973 | Operator and assignment borrow rules |
| checker_nll.cpp | 958 | Non-Lexical Lifetime computation |
| polonius_facts.cpp | 751 | Polonius fact generation |
| checker_core.cpp | 412 | Top-level dispatch, function checking |
| checker_env.cpp | 332 | Borrow environment: live borrows, moved places |
| polonius_solver.cpp | 190 | Polonius constraint solver |
| checker_stmt.cpp | 174 | Statement borrow checking |
| polonius_checker.cpp | 149 | Polonius result interpretation |

### Key Data Structures
- `Place` — a memory location: base variable + projection path (field access, deref, index)
- `Loan` — an active borrow: the place borrowed, mutability, loan region
- `BorrowEnv` — maps `Place` to live loans and move state
- `Region` — a lifetime region (NLL region variable)
- `PoloniusFacts` — set of Datalog facts for the Polonius solver (cfg_edge, loan_issued_at, etc.)

### Complexity: Complex
NLL is a well-researched algorithm (papers available). The complexity is in the precision required — the checker must track every field-level access, distinguish moved-from vs borrowed places, and handle complex control flow. The Polonius integration adds a Datalog solver.

### External Dependencies: None

### Self-Hosting Difficulty: Medium
The NLL algorithm is well-documented and the implementation is reasonably self-contained. `Place` and `Loan` map directly to TML structs. The Polonius solver is ~350 LOC total and implements a simple fixed-point iteration over Datalog facts — this maps to `List[Fact]` + `HashSet` convergence.

### Estimated TML LOC: ~4,300

### Key Porting Challenges
- Place projections (field access, array index, deref) form a linked list. In TML, use `List[Projection]` as the projection path.
- The NLL region computation requires a control flow graph. The borrow checker currently works directly on the AST — this means the CFG must be reconstructed from AST node sequences. In the TML port, consider operating on MIR (which already has explicit CFG) instead of AST.
- The Polonius solver uses a subset of Datalog semantics. In TML, the fact tables can be `HashSet[T]` and the rules implemented as convergence loops.

---

## 5. HIR Builder

### Purpose
Lowers the typed AST to HIR (High-level Intermediate Representation). Performs: type resolution (all expressions get concrete types), desugaring (`var` → `let mut`, `for` → iterator chain, `if let` → `when`), monomorphization (generic instantiations → concrete HIR functions), field/variant index resolution, and closure capture analysis. Includes binary serialization of the HIR for incremental caching.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/hir/) | 10,555 | 17 |
| Headers (compiler/include/hir/) | 4,652 | 11 |
| **Total** | **15,207** | **28** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| hir_builder.cpp | 1,511 | Main HIR lowering dispatcher |
| hir_builder_expr.cpp | 1,485 | Expression HIR lowering |
| hir_pass_inline.cpp | 1,292 | HIR-level inlining pass |
| serializer/binary_reader.cpp | 1,185 | HIR binary deserialization |
| serializer/binary_writer.cpp | 1,099 | HIR binary serialization |
| hir_pass.cpp | 728 | Pass manager, pass registration |
| hir_printer.cpp | 619 | HIR debug printer |
| serializer/serialize_utils.cpp | 560 | Shared serialization helpers |
| hir_builder_stmt.cpp | 433 | Statement HIR lowering |
| hir_builder_pattern.cpp | 362 | Pattern HIR lowering |
| hir_expr.hpp | 1,208 | HirExpr variant definitions (~50 variants) |
| hir_decl.hpp | 504 | HirFunction, HirStruct, HirEnum definitions |

### Key Data Structures
- `HirExpr` — 50+ variants (HirLit, HirVar, HirBinary, HirCall, HirMethodCall, HirBlock, HirWhen, HirLoop, HirClosure, HirField, HirIndex, etc.) — each variant carries a resolved `Type`
- `HirFunction { name, params, body: HirBlock, return_type: Type, generic_args }` — fully monomorphized
- `HirStruct { name, fields: Vec<HirField>, generic_args }` — concrete instantiation
- `HirModule` — collection of concrete `HirFunction` + `HirStruct` + `HirEnum`
- `HirId` — stable numeric identifier for HIR nodes (used in binary serialization)

### Complexity: Complex
Monomorphization is the hardest part — creating concrete function/struct instances for each unique set of generic type arguments, with memoization to avoid duplicates. The HIR binary serializer (2,284 LOC total) is a second significant complexity: it serializes HIR to a binary format for cross-session incremental caching.

### External Dependencies: None (depends on TypeEnv from type checker)

### Self-Hosting Difficulty: Medium
HIR lowering is a tree transformation: AST → HIR. Each `AstExpr` variant maps to a `HirExpr` variant. In TML, `HirExpr` becomes a TML enum with associated data. The `Heap[HirExpr]` boxing for recursive nodes is the primary structural concern. The binary serializer can be ported using TML's `Buffer` API.

### Estimated TML LOC: ~9,900

### Key Porting Challenges
- Monomorphization uses a `HashMap<(FuncId, Vec<Type>), HirFunction>` memoization table. In TML, the key type needs to implement `Hash` and `Eq` — `List[Type]` already does via the `Hash` behavior.
- Closure capture analysis requires tracking which variables in outer scopes are referenced by inner closures. This is a two-pass: first collect all references, then categorize as captured-by-value or captured-by-reference.
- The HIR inlining pass (`hir_pass_inline.cpp`, 1,292 LOC) substitutes function call sites with function bodies — it needs alpha-renaming (fresh variable names) to avoid capture. In TML, use a counter-based fresh name generator.
- The binary serializer must produce bit-for-bit identical output across TML and C++ compiler versions during the bootstrap transition period.

---

## 6. THIR Lowerer

### Purpose
Lowers HIR to THIR (Typed HIR — a narrower, more explicit form). Performs: implicit coercion insertion (`CoercionExpr` nodes for automatic conversions), method resolution via the trait/behavior solver, operator desugaring to method calls (`a + b` → `a.add(b)`), pattern exhaustiveness checking, and associated type normalization. THIR is then consumed by the THIR→MIR builder.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/thir/) | 1,873 | 3 |
| Headers (compiler/include/thir/) | 1,169 | 6 |
| **Total** | **3,042** | **9** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| thir_lower.cpp | 1,138 | Main HIR → THIR lowering |
| exhaustiveness.cpp | 623 | Pattern exhaustiveness checking |
| thir_module.cpp | 112 | ThirModule container |
| thir_expr.hpp | 572 | ThirExpr variant definitions |
| thir_module.hpp | 180 | ThirModule structure |
| thir_lower.hpp | 121 | ThirLower class interface |

### Key Data Structures
- `ThirExpr` — similar to `HirExpr` but with explicit coercions: `ThirCoerce { from: Type, to: Type, expr: Heap[ThirExpr> }`
- `ThirModule` — same structure as HirModule but with THIR nodes
- Exhaustiveness checking uses a matrix algorithm (rows = patterns, columns = constructors) based on the Maranget usefulness algorithm

### Complexity: Medium
The lowering pass is a straightforward tree transformation. Exhaustiveness checking is the algorithmically interesting part — it requires the pattern matrix algorithm to detect non-exhaustive `when` expressions and useless arms.

### External Dependencies: None

### Self-Hosting Difficulty: Medium
The THIR lowerer is the smallest of the pipeline stages. The coercion insertion logic is mechanical (check if types are compatible, insert coercion node if not). Exhaustiveness checking is well-studied — the Maranget algorithm is described in the literature.

### Estimated TML LOC: ~2,000

### Key Porting Challenges
- Operator desugaring requires a complete table of `BinOp → behavior method name` mappings (e.g., `+` → `Add::add`, `==` → `Eq::eq`). This is a 40-entry `HashMap`.
- Exhaustiveness checking requires computing the "constructor set" for each type — for enums, all variants; for `Bool`, `{true, false}`; for integers, all values (treated as a wildcard). The algorithm requires the constructor set to be finite or the pattern to include a wildcard.
- The coercion rules (what converts to what implicitly) must exactly match the C++ version during the bootstrap period.

---

## 7. MIR Builder + Passes

### Purpose
Builds MIR (Mid-level Intermediate Representation) from THIR (new path) or HIR (legacy path). MIR is an SSA-form (Single Static Assignment) IR with explicit basic blocks, phi nodes, and typed instructions. After building, a pipeline of 52 optimization passes transforms the MIR before codegen.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source — builder (compiler/src/mir/ non-passes) | 12,297 | 26 |
| Source — passes (compiler/src/mir/passes/) | 19,422 | 52 |
| Headers (compiler/include/mir/) | 8,474 | 59 |
| **Total** | **40,193** | **137** |

### Key Builder Files
| File | LOC | Role |
|------|-----|------|
| thir_mir_builder.cpp | 1,081 | THIR → MIR main builder |
| thir_mir_builder_expr.cpp | 1,132 | THIR expression lowering to MIR |
| builder/hir_expr.cpp | 1,227 | HIR expression lowering to MIR (legacy) |
| hir_mir_builder.cpp | 818 | HIR → MIR main builder (legacy) |
| builder/hir_expr_control.cpp | 836 | Control flow (loops, when, break, continue) |
| mir_pass.cpp | 903 | Pass manager, pass scheduling, pass pipeline |
| serializer/binary_reader.cpp | 615 | MIR binary deserialization |
| serializer/text_reader.cpp | 584 | MIR text format deserialization |
| mir.hpp | 1,057 | Core MIR types: instructions, blocks, functions |

### Key Pass Files (selected)
| File | LOC | Pass |
|------|-----|------|
| passes/vectorization.cpp | 1,350 | Auto-vectorization (SIMD) |
| passes/escape_analysis.cpp | 1,314 | Heap escape analysis |
| passes/inlining.cpp | 1,132 | Function inlining |
| passes/devirtualization.cpp | 875 | Virtual call devirtualization |
| passes/simplify_cfg.cpp | 795 | Control flow simplification |
| passes/loop_unroll.cpp | 669 | Loop unrolling |
| passes/pgo.cpp | 564 | Profile-guided optimization |
| passes/bounds_check_elimination.cpp | 536 | Array bounds check removal |
| passes/ipo.cpp | 466 | Inter-procedural optimization |
| + 43 more passes | ~12,240 | (see full listing in compiler/src/mir/passes/) |

### Key Data Structures (from mir.hpp, 1,057 lines)
- `MirType` — variant: `MirPrimitiveType`, `MirPointerType`, `MirArrayType`, `MirSliceType`, `MirTupleType`, `MirStructType`, `MirEnumType`, `MirDynType`, `MirFunctionType`
- `MirValue` — SSA value: `(MirValueId, MirType)` pair
- `MirInst` — an instruction: `Assign`, `Call`, `MethodCall`, `BinOp`, `UnaryOp`, `GEP`, `Load`, `Store`, `Cast`, `Alloca`, `Phi`, etc.
- `MirBasicBlock { id, instructions: Vec<MirInst>, terminator: MirTerminator }`
- `MirFunction { name, params, blocks: Vec<MirBasicBlock>, return_type: MirType }`
- `MirTerminator` — `Return`, `Jump`, `Branch`, `Switch`, `Unreachable`
- `mir::Module` — collection of `MirFunction` + `MirGlobal`

### Complexity: Very Complex
The MIR subsystem is the second-most complex after codegen. Two parallel builders (HIR→MIR legacy + THIR→MIR new) must be maintained. The 52 optimization passes each implement a different algorithm — some are simple (dead code elimination: 236 LOC) and some are very complex (vectorization: 1,350 LOC; escape analysis: 1,314 LOC; inlining: 1,132 LOC).

### External Dependencies: None (self-contained)

### Self-Hosting Difficulty: Hard
The MIR data structures translate cleanly to TML enums and structs. The difficulty is the sheer volume and the optimization passes' algorithmic complexity. Recommended porting strategy: port the MIR data structures and builders first (without passes), verify codegen produces correct output, then port passes one-by-one (starting with simple ones like `dead_code_elimination` and `constant_folding`).

### Estimated TML LOC: ~26,100

### Key Porting Challenges
- The two parallel builder paths (HIR→MIR and THIR→MIR) should be collapsed into one during the port. The THIR→MIR path is the "correct" one; the HIR→MIR path is legacy. The port is an opportunity to consolidate.
- The MIR type system (`MirType` with 8+ variants) uses a `std::variant` in C++. In TML, this is a native enum with associated data — cleaner.
- Vectorization pass (1,350 LOC) uses LLVM vector type intrinsics — this pass's output only makes sense in the context of LLVM codegen. Consider porting it last or skipping it for the initial self-hosted version.
- The pass pipeline scheduler (`mir_pass.cpp`, 903 LOC) runs passes in a specific order with fixpoint iteration for some passes. The scheduler logic must be preserved exactly.

---

## 8. MIR Codegen (IR Text Generator)

### Purpose
Generates LLVM IR text from the MIR representation. This is the "new path" codegen — it takes `mir::Module` and emits a `std::string` of LLVM IR text. Method calls in MIR go through `emit_call_inst` in `instructions_call.cpp`. This is separate from the legacy LLVM IR generator (Section 9).

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/codegen/mir/) | 4,080 | 7 |
| Source (compiler/src/codegen/mir_codegen.cpp) | 1,622 | 1 |
| Headers (compiler/include/codegen/ partial) | ~400 | ~3 |
| **Total** | **~6,100** | **11** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| mir_codegen.cpp | 1,622 | Main entry: `MirCodegen::generate()`, IR module header |
| codegen/mir/instructions_call.cpp | 1,236 | `emit_call_inst` — all function/method calls |
| codegen/mir/instructions_misc.cpp | 850 | Arithmetic, comparisons, memory ops |
| codegen/mir/instructions.cpp | 799 | Core instruction emission (alloca, load, store, GEP) |
| codegen/mir/instructions_method.cpp | 569 | Method-specific call conventions |
| codegen/mir/mir_types.cpp | 267 | MIR type → LLVM IR type string conversion |
| codegen/mir/codegen_helpers.cpp | 227 | Shared helpers (name mangling, register naming) |
| codegen/mir/terminators.cpp | 132 | Basic block terminators (br, ret, switch) |

### Key Data Structures
- `MirCodegen` class — holds symbol table of already-emitted declarations, manages register numbering
- `CodegenCtx` — per-function context: current basic block, register counter, local variable map
- `IrBuilder` — string accumulator for IR text (avoids repeated string concatenation)

### Complexity: Complex
String generation is conceptually simple but requires precise adherence to LLVM IR syntax for every instruction type. The call instruction handling (`instructions_call.cpp`, 1,236 LOC) is the most complex part — it handles sret conventions, by-value vs by-pointer passing, generic dispatch, behavior vtable calls, and async function calls.

### External Dependencies: None (pure string generation — no LLVM C API calls)

### Self-Hosting Difficulty: Medium
This subsystem is the most natural to implement in TML. LLVM IR generation is string formatting — TML's template literals and `Text` type make this clean. Each MIR instruction maps to a `Text` snippet. The `IrBuilder` maps to a `List[Text]` that is joined at the end. The `instructions_call.cpp` complexity comes from ABI rules (sret, calling conventions) — these rules must be preserved exactly.

### Estimated TML LOC: ~4,000

### Key Porting Challenges
- The sret (struct return) convention: functions returning structs larger than 2 registers pass the return value through a hidden first pointer argument. This rule must be applied consistently in both the callee declaration and every call site.
- Name mangling: TML uses a specific mangling scheme for monomorphized function names (e.g., `tml__collections__List__push__I64`). The mangling logic must produce identical output to the C++ version during bootstrap.
- The runtime module declarations (preamble that declares all external C functions used) must be emitted correctly — currently handled in `codegen/llvm/core/runtime_modules.cpp` (1,110 LOC) and `runtime_modules_tml.cpp` (1,057 LOC).

---

## 9. Legacy LLVM IR Generator

### Purpose
The original codegen path: directly lowers typed AST/HIR to LLVM IR text (bypassing MIR). This is the larger and more mature codegen path (used for most production compilation). Contains subdirectories for different expression categories: method dispatch, control flow, declarations, builtins, and struct operations. Will eventually be replaced by the MIR codegen path.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/codegen/llvm/) | 69,354 | 121 |
| Source (compiler/src/codegen/cranelift/) | 177 | 1 |
| Headers (compiler/include/codegen/ partial) | ~2,200 | ~10 |
| **Total** | **~71,730** | **133** |

### Codegen Subdirectory Breakdown
| Subdirectory | Role | Key Files |
|-------------|------|-----------|
| llvm/expr/ | Expression codegen (method dispatch, binary ops, struct ops) | method.cpp (1,597), method_impl.cpp (1,499), method_static_dispatch.cpp (1,496), method_outcome.cpp (1,371), binary.cpp (1,121) |
| llvm/core/ | Core infrastructure (type layout, generics, drop, runtime modules) | generic_instantiate_impl.cpp (1,539), drop.cpp (1,264), llvm_types.cpp (1,207), runtime_modules.cpp (1,110) |
| llvm/control/ | Control flow codegen (when/match, loops, async) | when.cpp (1,360) |
| llvm/decl/ | Declaration codegen (functions, impls, derives) | func.cpp (1,351), impl.cpp (1,336) |
| llvm/builtins/ | SIMD intrinsics, builtin operations | intrinsics.cpp (1,164) |
| llvm/derive/ | `@derive` macro codegen | various |
| cranelift/ | Cranelift backend stub | cranelift_codegen_backend.cpp (177) |

### Complexity: Very Complex
The largest single subsystem in the compiler at 71,730 LOC. The method dispatch alone (method.cpp + method_impl.cpp + method_static_dispatch.cpp = 4,592 LOC) is a complete subsystem. Generic instantiation (`generic_instantiate_impl.cpp`, 1,539 LOC) tracks which generic functions have been emitted and generates their concrete LLVM IR. The drop system (`drop.cpp`, 1,264 LOC) handles destructor insertion.

### External Dependencies: Uses LLVM IR text format (no LLVM C API calls in this subsystem)

### Self-Hosting Difficulty: Very Hard
At 71,730 LOC, this is the single largest porting effort. However, the self-hosting strategy can avoid porting this subsystem entirely: the MIR codegen path (Section 8, ~6,100 LOC) is the intended replacement. The recommended approach for self-hosting is to port the MIR codegen path and make it feature-complete rather than porting the legacy LLVM IR generator. This reduces the codegen porting effort from ~71,730 LOC to ~6,100 LOC — a 12x reduction.

### Estimated TML LOC (if ported directly): ~46,600
### Estimated TML LOC (if replaced by enhanced MIR path): ~5,000 additional

### Key Porting Challenges
- Method dispatch logic is deeply intertwined with the type checker's resolution results. Each method call has a resolved `impl` block pointer, a concrete callee type, and a vtable offset. This resolution context must be preserved in HIR/THIR to make the codegen stateless.
- The drop system inserts destructor calls at the points where values go out of scope. This requires a scope lifetime analysis that tracks which variables are live at each point.
- Generic instantiation memoization: a `HashMap<(FuncId, Vec<TypeArg>), String>` of already-emitted function bodies. In TML, use `HashMap[Text, Bool]` keyed on the mangled name.

---

## 10. LLVM Backend

### Purpose
Accepts LLVM IR text as input and produces native object files (`.obj`/`.o`). Uses the LLVM C API (`LLVMParseIRInContext`, `LLVMTargetMachineEmitToFile`). Supports X86-64 and AArch64 targets with configurable optimization levels (O0–O3). Also includes a JIT engine for runtime execution.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/backend/) | 1,593 | 3 |
| Headers (compiler/include/backend/) | 541 | 4 |
| **Total** | **2,134** | **7** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| llvm_backend.cpp | 978 | `LLVMParseIRInContext` → `.obj` |
| lld_linker.cpp | 504 | LLD in-process linker |
| jit_engine.cpp | 111 | LLVM JIT for `tml run` |

### External Dependencies: LLVM 18 C API, LLD (in-process)

### Self-Hosting Strategy: Keep in C++ Permanently
The LLVM backend must remain in C++. `LLVMParseIRInContext` is a C function, but the surrounding infrastructure (target initialization, pass manager configuration, optimization pipeline) requires careful C++ orchestration. The TML-written compiler will call the same LLVM backend C++ library through the same interface — IR text in, `.obj` file out.

The key architectural insight: because the compiler accepts IR as **text** (not as an LLVM `Module*` object), the TML-written codegen has zero LLVM API dependency. It only produces a `Text` value. The LLVM backend translates that text to machine code.

### Estimated TML LOC: 0 (keep in C++)

---

## 11. LLD Linker

Included in the LLVM backend section above. The LLD linker (`lld_linker.cpp`, 504 LOC) wraps LLD's in-process API. It receives a list of `.obj` files and produces an executable. The TML-written compiler will invoke this through the same C++ interface.

### Self-Hosting Strategy: Keep in C++ Permanently
LLD's C++ API (`lld::coff::link`, `lld::elf::link`) cannot be reasonably wrapped in TML. Keep as C++ wrapper called via the plugin ABI.

---

## 12. Query System

### Purpose
Provides demand-driven, memoized, incremental compilation. Each compilation step is a "query" that can be cached across sessions using fingerprints. `QueryContext::force(QueryKey)` returns a cached result or runs the computation. Fingerprinting detects when inputs change and marks downstream queries stale (RED/YELLOW/GREEN coloring).

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/query/) | 2,126 | 7 |
| Headers (compiler/include/query/) | 969 | 8 |
| **Total** | **3,095** | **15** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| query_core.cpp | 809 | Query execution, cycle detection, memoization |
| query_incr.cpp | 613 | Incremental cache (.incr-cache/incr.bin) persistence |
| query_context.cpp | 448 | QueryContext: the central registry of queries + results |
| query_key.hpp | 239 | QueryKey enum: ReadSource, Tokenize, Parse, Typecheck, etc. |
| query_context.hpp | 284 | QueryContext class definition |
| query_incr.hpp | 166 | Incremental cache interface |

### Key Data Structures
- `QueryKey` — enum identifying which query to compute (239-line definition with ~20 variants)
- `QueryContext` — holds the memoized result table: `HashMap<QueryKey, QueryResult>`
- `QueryResult` — `{value: Any, fingerprint: u64, dependencies: Vec<QueryKey>}`
- Incremental cache — binary file at `.incr-cache/incr.bin` mapping `QueryKey → (fingerprint, serialized_result)`

### Complexity: Medium
The query system is an elegant but complex infrastructure layer. Cycle detection (queries calling themselves) requires a stack of in-progress queries. Incremental invalidation requires propagating RED status through the dependency graph.

### External Dependencies: None (binary file I/O only)

### Self-Hosting Difficulty: Medium
The query system architecture maps cleanly to TML. `QueryContext` becomes a struct holding `HashMap[QueryKey, QueryResult]`. The `QueryKey` enum is a direct TML enum. The binary cache format uses `Buffer` + `File` I/O. The main challenge is the `Any` type for query results — in TML, this requires either a tagged union of all possible result types or a type-erased pointer.

### Estimated TML LOC: ~2,000

### Key Porting Challenges
- Query results hold heterogeneous types (token streams, AST modules, HIR modules, etc.). In TML, define a `QueryResult` enum with variants for each result type.
- The incremental cache binary format must be preserved for cross-session caching to work. A format version number in the file header allows safe format evolution.
- Cycle detection uses a `HashSet<QueryKey>` of in-progress queries. If a query is requested while it is already being computed, that indicates a cycle — report an error.

---

## 13. CLI

### Purpose
Implements all user-facing commands: `tml build`, `tml run`, `tml check`, `tml test`, `tml doc`, `tml format`, `tml lint`, `tml explain`, `tml profile`, and `tml inspect`. Includes the build system (dependency resolution, parallel compilation, build cache, object file management) and diagnostic renderer.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/cli/) | 26,791 | 55 |
| Headers (compiler/include/cli/) | 0 | 0 |
| **Total** | **26,791** | **55** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| builder/build.cpp | 1,538 | Main build pipeline orchestration |
| commands/cmd_doc.cpp | 1,123 | `tml doc` — documentation generator |
| builder/object_compiler.cpp | 1,077 | Per-file compilation to .obj |
| diagnostic.cpp | 1,040 | Error diagnostic rendering (colors, spans, caret) |
| builder/builder_helpers_runtime.cpp | 971 | Runtime file selection, link flags |
| builder/builder_run.cpp | 968 | `tml run` — build + execute |
| builder/build_cache.cpp | 939 | Build artifact caching (.run-cache/) |
| builder/build_config.cpp | 870 | Build configuration (targets, optimization levels) |
| builder/parallel_build.cpp | 858 | Parallel compilation scheduling |
| explain/codegen_errors.cpp | 812 | Error code explanations (codegen errors) |
| dispatcher.cpp | 810 | Command dispatch: parse args, route to command |
| commands/cmd_profile.cpp | 810 | `tml profile` — Tracy profiler integration |
| explain/type_errors.cpp | 790 | Error code explanations (type errors) |
| builder/dependency_resolver.cpp | 779 | Module dependency graph resolution |

### Complexity: Complex
The CLI is large but each component is relatively self-contained. The build system (dependency resolution, parallel compilation, caching) is the most complex part. The diagnostic renderer implements rich error formatting (source spans, colored output, secondary labels) comparable to rustc's diagnostics.

### External Dependencies: File system, process spawning, terminal colors

### Self-Hosting Difficulty: Medium
The CLI maps well to TML. The build system uses `HashMap` for the dependency graph, `List` for compilation queues, `File` for cache I/O, and `Process::spawn` for subprocess invocation. The diagnostic renderer uses `Text` building with ANSI color codes. TML already has all required APIs.

### Estimated TML LOC: ~17,400

### Key Porting Challenges
- The parallel compilation scheduler (`parallel_build.cpp`, 858 LOC) uses `std::thread` + `std::mutex` + a work queue. In TML, use `Thread::spawn` + `Mutex[WorkQueue]` + MPSC channels.
- The dependency resolver must read `.tml.meta` files (the binary module metadata format) to discover transitive dependencies. This requires the module metadata format to be readable without running the full type checker.
- `cmd_doc.cpp` (1,123 LOC) generates HTML documentation from doc comments. This is purely text processing — straightforward in TML.

---

## 14. Testing Framework

### Purpose
Implements the `tml test` command and the entire testing infrastructure. Uses a subprocess-based architecture (Go model): each test suite compiles to a `.exe`, runs as a subprocess, and streams results via NDJSON protocol. Includes coverage tracking (function-level, line-level), HTML coverage reports, test caching (skip unchanged tests), and parallel test execution.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/testing/) | 9,968 | 12 |
| Headers (compiler/include/testing/) | 1,013 | 10 |
| **Total** | **10,981** | **22** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| testing_coordinator.cpp | 1,447 | Main orchestrator: discover → compile → run → report |
| testing_coverage_html.cpp | 1,397 | HTML coverage report generation |
| testing_compile.cpp | 1,353 | Per-suite test compilation |
| testing_coverage.cpp | 1,153 | Coverage data collection and aggregation |
| testing_process.cpp | 849 | Subprocess launch and NDJSON result reading |
| testing_dispatcher_gen.cpp | 682 | Code generation for test dispatcher main() |
| testing_protocol.cpp | 672 | NDJSON event parsing |
| testing_test_cache.cpp | 650 | Test result caching (.new-test-cache.json) |
| testing_compile_parallel.cpp | 648 | Parallel test compilation |
| testing_reporter.cpp | 452 | Terminal output formatting |
| testing_discovery.cpp | 250 | Test file discovery (*.test.tml) |

### Complexity: Complex
The testing system is an independent build+run pipeline within the compiler. The NDJSON streaming protocol, parallel subprocess management, and coverage instrumentation (injecting coverage probes at compile time) each add significant complexity.

### External Dependencies: File system, process spawning, JSON parsing

### Self-Hosting Difficulty: Hard
The testing framework requires the ability to compile TML files (using the TML-written compiler) and launch subprocesses. This creates a bootstrapping dependency: the testing framework can only be ported after the core pipeline is working. The coverage instrumentation requires modifying the codegen to inject probe calls — this change must be coordinated with the codegen port.

### Estimated TML LOC: ~7,100

### Key Porting Challenges
- The NDJSON protocol parser is straightforward JSON line parsing. TML's `std::json` module handles this.
- The test cache (`.new-test-cache.json`) uses JSON serialization with file fingerprints. Use `std::json` for this too.
- Coverage probe injection (`testing_coverage.cpp`, 1,153 LOC) requires the codegen to emit calls to a coverage runtime at each instrumented point. This means the coverage feature has a hard dependency on codegen being complete before it can be ported.
- The HTML report generator (`testing_coverage_html.cpp`, 1,397 LOC) is pure string generation — very natural in TML.

---

## 15. Formatter

### Purpose
Implements `tml format` — auto-formats TML source code to the canonical style. Parses source to AST, then pretty-prints it with correct indentation, spacing, and line breaks. Also handles `tml format --check` (reports if file would change without writing).

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/format/) | 1,181 | 6 |
| Headers (compiler/include/format/) | 146 | 1 |
| **Total** | **1,327** | **7** |

### Key Files
| File | LOC | Role |
|------|-----|------|
| format_expr.cpp | 475 | Expression pretty-printing |
| format_decl.cpp | 363 | Declaration pretty-printing |
| format_pattern.cpp | 102 | Pattern pretty-printing |
| format_type.cpp | 102 | Type expression pretty-printing |
| format_core.cpp | 71 | Indentation management, output buffer |
| format_stmt.cpp | 68 | Statement pretty-printing |

### Complexity: Simple
The formatter is a tree visitor that pretty-prints each AST node with rules for indentation and line width. The most complex part is handling trailing commas, multi-line vs inline formatting decisions, and comment preservation.

### External Dependencies: None (depends on parser output)

### Self-Hosting Difficulty: Easy
The formatter is the simplest Layer 2 subsystem. It is a pure function from `Module → Text`. In TML, each `format_*` function returns a `Text` value built with template literals and string concatenation.

### Estimated TML LOC: ~860

### Key Porting Challenges
- Comment preservation: comments are stripped by the lexer but must be re-inserted at the correct positions during formatting. This requires storing comment positions as `Span` values in the token stream.
- Line-length-aware formatting: for expressions that exceed the maximum line width, the formatter switches to multi-line mode. This requires two-pass formatting: try single-line, measure width, fall back to multi-line.

---

## 16. Plugin / Launcher

### Purpose
Implements the modular build architecture (optional). The launcher (`src/launcher/main_launcher.cpp`, 121 LOC) is a thin executable that loads `tml_compiler.dll` via the plugin ABI. The plugin system (`src/plugin/`, 795 LOC) defines the ABI contracts for plugins and the DLL loader infrastructure.

### Size
| Component | LOC | Files |
|-----------|-----|-------|
| Source (compiler/src/plugin/) | 795 | 6 |
| Source (compiler/src/launcher/) | 121 | 1 |
| Headers (compiler/include/plugin/) | 282 | 4 |
| **Total** | **1,198** | **11** |

### Self-Hosting Difficulty: Easy
The plugin ABI (`plugin/abi.h`) is a pure C interface. The TML-written compiler can expose the same C interface using `@extern("c")` exported functions. The launcher is 121 LOC and trivially ported.

### Estimated TML LOC: ~780

---

## 17. Summary Tables

### Subsystem Summary

| Subsystem | Src LOC | Hdr LOC | Total LOC | Files | Complexity | SH Difficulty | Est. TML LOC |
|-----------|---------|---------|-----------|-------|-----------|--------------|-------------|
| Lexer | 2,830 | 869 | 3,699 | 12 | Simple | Easy | 2,400 |
| Parser | 6,327 | 3,499 | 9,826 | 19 | Medium | Easy | 6,400 |
| Type Checker | 21,179 | 2,137 | 23,316 | 47 | Very Complex | Hard | 15,100 |
| Borrow Checker | 4,971 | 1,629 | 6,600 | 11 | Complex | Medium | 4,300 |
| HIR Builder | 10,555 | 4,652 | 15,207 | 28 | Complex | Medium | 9,900 |
| THIR Lowerer | 1,873 | 1,169 | 3,042 | 9 | Medium | Medium | 2,000 |
| MIR Builder | 12,297 | — | — | 26 | Very Complex | Hard | — |
| MIR Passes | 19,422 | 8,474 | 40,193 | 137 | Very Complex | Hard | 26,100 |
| MIR Codegen | 5,702 | — | ~6,100 | 11 | Complex | Medium | 4,000 |
| Legacy LLVM IR Gen | 69,354 | 2,634 | ~71,730 | 133 | Very Complex | Very Hard | (skip — use MIR path) |
| LLVM Backend | 1,593 | 541 | 2,134 | 7 | Medium | Keep C++ | 0 |
| Query System | 2,126 | 969 | 3,095 | 15 | Medium | Medium | 2,000 |
| CLI | 26,791 | 0 | 26,791 | 55 | Complex | Medium | 17,400 |
| Testing Framework | 9,968 | 1,013 | 10,981 | 22 | Complex | Hard | 7,100 |
| Formatter | 1,181 | 146 | 1,327 | 7 | Simple | Easy | 860 |
| Plugin / Launcher | 916 | 282 | 1,198 | 11 | Simple | Easy | 780 |
| C Runtime | 18,650 | — | 18,650 | ~30 | — | Keep C | 0 |
| **TOTAL** | **215,735** | **28,014** | **245,029** | **589** | — | — | **~98,340** |

Note: "MIR Builder" and "MIR Passes" share the MIR header LOC (8,474). Legacy LLVM IR Gen is excluded from the recommended TML LOC estimate — it will be superseded by the MIR codegen path.

### Top 20 Largest C++ Files

| Rank | File | LOC | Subsystem |
|------|------|-----|-----------|
| 1 | codegen/llvm/expr/method.cpp | 1,597 | Legacy LLVM Codegen |
| 2 | codegen/llvm/core/generic_instantiate_impl.cpp | 1,539 | Legacy LLVM Codegen |
| 3 | cli/builder/build.cpp | 1,538 | CLI |
| 4 | hir/hir_builder.cpp | 1,511 | HIR Builder |
| 5 | codegen/llvm/expr/method_impl.cpp | 1,499 | Legacy LLVM Codegen |
| 6 | codegen/llvm/expr/method_static_dispatch.cpp | 1,496 | Legacy LLVM Codegen |
| 7 | hir/hir_builder_expr.cpp | 1,485 | HIR Builder |
| 8 | testing/testing_coordinator.cpp | 1,447 | Testing |
| 9 | types/checker/core.cpp | 1,412 | Type Checker |
| 10 | types/module_binary_read.cpp | 1,409 | Type Checker |
| 11 | testing/testing_coverage_html.cpp | 1,397 | Testing |
| 12 | codegen/llvm/expr/method_outcome.cpp | 1,371 | Legacy LLVM Codegen |
| 13 | types/checker/expr_call_method.cpp | 1,363 | Type Checker |
| 14 | codegen/llvm/control/when.cpp | 1,360 | Legacy LLVM Codegen |
| 15 | testing/testing_compile.cpp | 1,353 | Testing |
| 16 | codegen/llvm/decl/func.cpp | 1,351 | Legacy LLVM Codegen |
| 17 | mir/passes/vectorization.cpp | 1,350 | MIR Passes |
| 18 | codegen/llvm/decl/impl.cpp | 1,336 | Legacy LLVM Codegen |
| 19 | mir/passes/escape_analysis.cpp | 1,314 | MIR Passes |
| 20 | hir/hir_pass_inline.cpp | 1,292 | HIR Builder |

### External Dependency Inventory

| Dependency | Used By | Type | Self-Hosting Impact |
|-----------|---------|------|-------------------|
| LLVM 18 (C API) | llvm_backend.cpp, jit_engine.cpp | C API FFI | Keep in C++ permanently — not rewritable |
| LLD (C++ API) | lld_linker.cpp | C++ library | Keep in C++ permanently — not rewritable |
| OpenSSL / BCrypt | compiler/runtime/crypto/ | C library | Keep in C runtime — `@extern("c")` in TML |
| Winsock2 / POSIX sockets | compiler/runtime/net/ | OS API | Keep in C runtime — `@extern("c")` in TML |
| Windows IOCP | compiler/runtime/net/iocp.c | OS API | Keep in C runtime — `@extern("c")` in TML |
| pthreads / Windows Events | compiler/runtime/concurrency/ | OS API | Keep in C runtime — `@extern("c")` in TML |
| malloc / free | compiler/runtime/memory/mem.c | C stdlib | Keep in C runtime — `@extern("c")` in TML |
| std::thread, std::mutex | compiler/src/cli/ (parallel build) | C++ stdlib | Replace with TML `Thread::spawn` + `Mutex[T]` |
| std::variant | Throughout compiler | C++ stdlib | Replace with TML enums |
| std::unique_ptr / shared_ptr | Throughout compiler | C++ stdlib | Replace with `Heap[T]` / `Shared[T]` |
| std::unordered_map | Throughout compiler | C++ stdlib | Replace with `HashMap[K,V]` |
| std::vector | Throughout compiler | C++ stdlib | Replace with `List[T]` |
| std::string | Throughout compiler | C++ stdlib | Replace with `Text` / `Str` |
| dbghelp.dll | diagnostics/backtrace.c | Windows API | Keep in C runtime |
| SQLite | lib/std/src/sqlite/ | C library | Already `@extern("c")` — no change |

### Subsystem Dependency Graph

The following shows data flow dependencies. An arrow `A → B` means B consumes output from A.

```
Source Files (*.tml)
  │
  ▼
Lexer ──────────────────────────────── produces: List[Token]
  │
  ▼
Parser ─────────────────────────────── produces: Module (AST)
  │
  ├──► Type Checker ─────────────────── produces: TypeEnv + resolved Module
  │         │
  │         ├──► Borrow Checker ─────── consumes: Module + TypeEnv
  │         │
  │         └──► HIR Builder ──────────── produces: HirModule
  │                   │
  │                   └──► THIR Lowerer ─ produces: ThirModule
  │                             │
  │                             └──► MIR Builder ── produces: mir::Module
  │                                       │
  │                                       ▼
  │                                  MIR Passes (52) ── produces: mir::Module (optimized)
  │                                       │
  │                                       ├──► MIR Codegen ──► LLVM IR text
  │                                       │                         │
  │                                       └──► Legacy LLVM IR Gen ──┤ (parallel path)
  │                                                                  │
  │                                                                  ▼
  │                                                         LLVM Backend ──► .obj file
  │                                                                  │
  │                                                                  ▼
  │                                                         LLD Linker ──► executable
  │
  └──► Query System ─────────── wraps all stages with memoization + incremental cache
       │
  CLI ─┴────────────── drives the full pipeline from user commands
       │
  Testing ────────────── compiles + runs *.test.tml files as subprocesses
       │
  Formatter ──────────── Parser → pretty-print → reformatted source
```

**Legend**: All subsystems left of "LLVM Backend" are candidates for rewriting in TML. The LLVM Backend and LLD Linker remain in C++ permanently.

---

*See also: [00-executive-summary.md](00-executive-summary.md) | [02-stdlib-readiness.md](02-stdlib-readiness.md) | [03-language-gaps.md](03-language-gaps.md)*
