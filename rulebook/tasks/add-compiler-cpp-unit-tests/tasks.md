# Tasks: Add C++ Unit Tests for Untested Compiler Components

**Status**: Proposed (0%)

## Phase 1: MIR Optimization Pass Tests (HIGHEST priority)

### 1.1 Core Optimization Passes

- [ ] 1.1.1 Add tests for `constant_folding.cpp` — verify constant expressions are simplified
- [ ] 1.1.2 Add tests for `constant_propagation.cpp` — verify constants propagate through uses
- [ ] 1.1.3 Add tests for `dead_code_elimination.cpp` — verify unreachable code removal
- [ ] 1.1.4 Add tests for `adce.cpp` — verify aggressive dead code elimination
- [ ] 1.1.5 Add tests for `mem2reg.cpp` — verify alloca-to-SSA promotion
- [ ] 1.1.6 Add tests for `copy_propagation.cpp` — verify redundant copy elimination
- [ ] 1.1.7 Add tests for `sroa.cpp` — verify scalar replacement of aggregates
- [ ] 1.1.8 Add tests for `inlining.cpp` — verify function inlining decisions and correctness
- [ ] 1.1.9 Add tests for `gvn.cpp` — verify global value numbering
- [ ] 1.1.10 Add tests for `early_cse.cpp` — verify early common subexpression elimination
- [ ] 1.1.11 Add tests for `common_subexpression_elimination.cpp` — verify full CSE

### 1.2 Control Flow Passes

- [ ] 1.2.1 Add tests for `simplify_cfg.cpp` — verify CFG simplification
- [ ] 1.2.2 Add tests for `block_merge.cpp` — verify basic block merging
- [ ] 1.2.3 Add tests for `merge_returns.cpp` — verify return merging
- [ ] 1.2.4 Add tests for `jump_threading.cpp` — verify conditional jump threading
- [ ] 1.2.5 Add tests for `unreachable_code_elimination.cpp` — verify unreachable block removal

### 1.3 Loop Passes

- [ ] 1.3.1 Add tests for `licm.cpp` — verify loop-invariant code motion
- [ ] 1.3.2 Add tests for `loop_opts.cpp` — verify general loop optimizations
- [ ] 1.3.3 Add tests for `loop_rotate.cpp` — verify loop rotation
- [ ] 1.3.4 Add tests for `loop_unroll.cpp` — verify loop unrolling
- [ ] 1.3.5 Add tests for `infinite_loop_check.cpp` — verify infinite loop detection

### 1.4 Dead Elimination Passes

- [ ] 1.4.1 Add tests for `dead_arg_elim.cpp` — verify unused argument elimination
- [ ] 1.4.2 Add tests for `dead_function_elimination.cpp` — verify unused function removal
- [ ] 1.4.3 Add tests for `dead_method_elimination.cpp` — verify unused method removal

### 1.5 Strength Reduction & Peephole

- [ ] 1.5.1 Add tests for `strength_reduction.cpp` — verify strength reduction transforms
- [ ] 1.5.2 Add tests for `reassociate.cpp` — verify expression reassociation
- [ ] 1.5.3 Add tests for `peephole.cpp` — verify peephole optimizations
- [ ] 1.5.4 Add tests for `inst_simplify.cpp` — verify instruction simplification
- [ ] 1.5.5 Add tests for `narrowing.cpp` — verify type narrowing
- [ ] 1.5.6 Add tests for `const_hoist.cpp` — verify constant hoisting

### 1.6 Analysis-Based Passes

- [ ] 1.6.1 Add tests for `alias_analysis.cpp` — verify alias query correctness
- [ ] 1.6.2 Add tests for `load_store_opt.cpp` — verify redundant load/store elimination
- [ ] 1.6.3 Add tests for `bounds_check_elimination.cpp` — verify safe bounds check removal
- [ ] 1.6.4 Add tests for `memory_leak_check.cpp` — verify leak detection

### 1.7 Specialized Passes

- [ ] 1.7.1 Add tests for `tail_call.cpp` — verify tail call optimization
- [ ] 1.7.2 Add tests for `rvo.cpp` — verify return value optimization
- [ ] 1.7.3 Add tests for `constructor_fusion.cpp` — verify constructor fusion
- [ ] 1.7.4 Add tests for `destructor_hoist.cpp` — verify destructor hoisting
- [ ] 1.7.5 Add tests for `batch_destruction.cpp` — verify batch destruction
- [ ] 1.7.6 Add tests for `match_simplify.cpp` — verify match simplification
- [ ] 1.7.7 Add tests for `async_lowering.cpp` — verify async state machine lowering
- [ ] 1.7.8 Add tests for `sinking.cpp` — verify code sinking
- [ ] 1.7.9 Add tests for `simplify_select.cpp` — verify select simplification
- [ ] 1.7.10 Add tests for `ipo.cpp` — verify interprocedural optimizations
- [ ] 1.7.11 Add tests for `builder_opt.cpp` — verify MIR builder optimizations
- [ ] 1.7.12 Add tests for `pgo.cpp` — verify profile-guided optimization
- [ ] 1.7.13 Add tests for `vectorization.cpp` — verify auto-vectorization

## Phase 2: LLVM Expression Codegen Tests (HIGHEST priority)

### 2.1 Core Expression Types

- [ ] 2.1.1 Add tests for `binary.cpp` / `binary_ops.cpp` — arithmetic, comparison, logical ops
- [ ] 2.1.2 Add tests for `unary.cpp` — negation, bitwise not, logical not
- [ ] 2.1.3 Add tests for `cast.cpp` — integer/float widening, narrowing, sign extension
- [ ] 2.1.4 Add tests for `core.cpp` — base expression codegen infrastructure
- [ ] 2.1.5 Add tests for `call.cpp` / `call_user.cpp` — function call codegen
- [ ] 2.1.6 Add tests for `call_generic_struct.cpp` — generic struct method calls
- [ ] 2.1.7 Add tests for `print.cpp` — print expression codegen

### 2.2 Struct/Tuple/Enum Expressions

- [ ] 2.2.1 Add tests for `struct.cpp` — struct construction codegen
- [ ] 2.2.2 Add tests for `struct_field.cpp` — field access codegen
- [ ] 2.2.3 Add tests for `tuple.cpp` — tuple construction and access
- [ ] 2.2.4 Add tests for `collections.cpp` — collection literal codegen

### 2.3 Method Dispatch

- [ ] 2.3.1 Add tests for `method.cpp` — general method call dispatch
- [ ] 2.3.2 Add tests for `method_static.cpp` / `method_static_dispatch.cpp` — static dispatch
- [ ] 2.3.3 Add tests for `method_generic.cpp` — generic method instantiation
- [ ] 2.3.4 Add tests for `method_dyn.cpp` — dynamic dispatch via vtable
- [ ] 2.3.5 Add tests for `method_impl.cpp` — impl block method calls
- [ ] 2.3.6 Add tests for `method_class.cpp` — class method calls
- [ ] 2.3.7 Add tests for `method_primitive.cpp` / `method_primitive_ext.cpp` — primitive type methods
- [ ] 2.3.8 Add tests for `method_prim_behavior.cpp` — primitive behavior methods
- [ ] 2.3.9 Add tests for `method_array.cpp` — array method codegen
- [ ] 2.3.10 Add tests for `method_slice.cpp` — slice method codegen
- [ ] 2.3.11 Add tests for `method_collection.cpp` — collection method codegen
- [ ] 2.3.12 Add tests for `method_maybe.cpp` — Maybe[T] method codegen
- [ ] 2.3.13 Add tests for `method_outcome.cpp` — Outcome[T,E] method codegen

### 2.4 Advanced Expressions

- [ ] 2.4.1 Add tests for `closure.cpp` — closure capture, environment layout
- [ ] 2.4.2 Add tests for `try.cpp` — try/error propagation codegen
- [ ] 2.4.3 Add tests for `await.cpp` — async await codegen
- [ ] 2.4.4 Add tests for `infer.cpp` / `infer_methods.cpp` — type inference in codegen

## Phase 3: Codegen Core & Infrastructure (HIGH priority)

### 3.1 Type System

- [ ] 3.1.1 Add tests for `types.cpp` — LLVM type mapping
- [ ] 3.1.2 Add tests for `types_resolve.cpp` — type resolution in codegen context
- [ ] 3.1.3 Add tests for `target.cpp` — target triple configuration

### 3.2 Core Codegen

- [ ] 3.2.1 Add tests for `generate.cpp` — top-level IR generation
- [ ] 3.2.2 Add tests for `generate_support.cpp` — codegen support utilities
- [ ] 3.2.3 Add tests for `generate_cache.cpp` — IR caching
- [ ] 3.2.4 Add tests for `generic.cpp` — generic instantiation codegen
- [ ] 3.2.5 Add tests for `utils.cpp` — codegen utility functions

### 3.3 OOP & Dynamic Dispatch

- [ ] 3.3.1 Add tests for `class_codegen.cpp` — class IR generation
- [ ] 3.3.2 Add tests for `class_codegen_generic.cpp` — generic class codegen
- [ ] 3.3.3 Add tests for `class_codegen_virtual.cpp` — virtual method codegen
- [ ] 3.3.4 Add tests for `dyn.cpp` — trait object / dynamic dispatch codegen
- [ ] 3.3.5 Add tests for `drop.cpp` — drop glue generation

### 3.4 Runtime & Debug

- [ ] 3.4.1 Add tests for `runtime.cpp` — runtime function declarations
- [ ] 3.4.2 Add tests for `runtime_modules.cpp` — runtime module loading
- [ ] 3.4.3 Add tests for `debug_info.cpp` — DWARF debug info generation
- [ ] 3.4.4 Add tests for `optimization_passes.cpp` — LLVM pass pipeline setup

## Phase 4: Derive & Declaration Codegen (MEDIUM priority)

### 4.1 Derive Implementations

- [ ] 4.1.1 Add tests for `partial_eq.cpp` — PartialEq derive codegen
- [ ] 4.1.2 Add tests for `partial_ord.cpp` — PartialOrd derive codegen
- [ ] 4.1.3 Add tests for `debug.cpp` — Debug derive codegen
- [ ] 4.1.4 Add tests for `display.cpp` — Display derive codegen
- [ ] 4.1.5 Add tests for `duplicate.cpp` — Duplicate (Clone) derive codegen
- [ ] 4.1.6 Add tests for `hash.cpp` — Hash derive codegen
- [ ] 4.1.7 Add tests for `default.cpp` — Default derive codegen
- [ ] 4.1.8 Add tests for `serialize.cpp` — Serialize derive codegen
- [ ] 4.1.9 Add tests for `deserialize.cpp` — Deserialize derive codegen
- [ ] 4.1.10 Add tests for `fromstr.cpp` — FromStr derive codegen
- [ ] 4.1.11 Add tests for `reflect.cpp` — Reflect derive codegen

### 4.2 Declaration Codegen

- [ ] 4.2.1 Add tests for `struct.cpp` (decl) — struct type declaration IR
- [ ] 4.2.2 Add tests for `enum.cpp` (decl) — enum type declaration IR
- [ ] 4.2.3 Add tests for `func.cpp` (decl) — function declaration IR
- [ ] 4.2.4 Add tests for `impl.cpp` (decl) — impl block codegen

### 4.3 Control Flow Codegen

- [ ] 4.3.1 Add tests for `if.cpp` — if/else branch codegen
- [ ] 4.3.2 Add tests for `loop.cpp` — loop codegen (while, for, loop)
- [ ] 4.3.3 Add tests for `when.cpp` — when (match) codegen
- [ ] 4.3.4 Add tests for `return.cpp` — return codegen

## Phase 5: Codegen MIR & Builtins (MEDIUM priority)

### 5.1 MIR Codegen

- [ ] 5.1.1 Add tests for `helpers.cpp` (codegen/mir) — MIR-to-LLVM helpers
- [ ] 5.1.2 Add tests for `instructions.cpp` — MIR instruction lowering
- [ ] 5.1.3 Add tests for `instructions_method.cpp` — method instruction lowering
- [ ] 5.1.4 Add tests for `instructions_misc.cpp` — misc instruction lowering
- [ ] 5.1.5 Add tests for `terminators.cpp` — terminator instruction lowering
- [ ] 5.1.6 Add tests for `types.cpp` (codegen/mir) — MIR type lowering

### 5.2 Builtin Codegen

- [ ] 5.2.1 Add tests for `intrinsics.cpp` — compiler intrinsics codegen
- [ ] 5.2.2 Add tests for `intrinsics_extended.cpp` — extended intrinsics
- [ ] 5.2.3 Add tests for `collections.cpp` (builtins) — collection builtin codegen
- [ ] 5.2.4 Add tests for `string.cpp` (builtins) — string builtin codegen
- [ ] 5.2.5 Add tests for `mem.cpp` (builtins) — memory builtin codegen
- [ ] 5.2.6 Add tests for `math.cpp` (builtins) — math builtin codegen
- [ ] 5.2.7 Add tests for `io.cpp` (builtins) — I/O builtin codegen
- [ ] 5.2.8 Add tests for `async.cpp` (builtins) — async builtin codegen
- [ ] 5.2.9 Add tests for `atomic.cpp` (builtins) — atomic builtin codegen
- [ ] 5.2.10 Add tests for `sync.cpp` (builtins) — sync builtin codegen
- [ ] 5.2.11 Add tests for `time.cpp` (builtins) — time builtin codegen
- [ ] 5.2.12 Add tests for `assert.cpp` (builtins) — assertion builtin codegen

## Phase 6: CLI Infrastructure Tests (LOW priority)

### 6.1 Linter

- [ ] 6.1.1 Add tests for linter `config.cpp` — lint rule configuration
- [ ] 6.1.2 Add tests for linter `discovery.cpp` — lint target discovery
- [ ] 6.1.3 Add tests for linter `semantic.cpp` — semantic lint rules
- [ ] 6.1.4 Add tests for linter `style.cpp` — style lint rules
- [ ] 6.1.5 Add tests for linter `run.cpp` — lint execution
- [ ] 6.1.6 Add tests for linter `helpers.cpp` — lint helper utilities

### 6.2 Tester

- [ ] 6.2.1 Add tests for `test_cache.cpp` — test result caching
- [ ] 6.2.2 Add tests for `discovery.cpp` — test file discovery
- [ ] 6.2.3 Add tests for `output.cpp` — test output formatting
- [ ] 6.2.4 Add tests for `helpers.cpp` (tester) — test helper utilities
- [ ] 6.2.5 Add tests for `coverage.cpp` — coverage data collection
- [ ] 6.2.6 Add tests for `library_coverage.cpp` — library function coverage tracking

### 6.3 Commands

- [ ] 6.3.1 Add tests for `cmd_build.cpp` — build command option parsing
- [ ] 6.3.2 Add tests for `cmd_test.cpp` — test command option parsing
- [ ] 6.3.3 Add tests for `cmd_lint.cpp` — lint command option parsing
- [ ] 6.3.4 Add tests for `cmd_format.cpp` — format command option parsing

### 6.4 Builder

- [ ] 6.4.1 Add tests for `build.cpp` — build orchestration
- [ ] 6.4.2 Add tests for `dependency_resolver.cpp` — module dependency resolution
- [ ] 6.4.3 Add tests for `build_config.cpp` — build configuration
- [ ] 6.4.4 Add tests for `rlib.cpp` — TML library format

## Phase 7: Support Components (LOW priority)

### 7.1 Doc Generator

- [ ] 7.1.1 Add tests for `doc_parser.cpp` — doc comment parsing
- [ ] 7.1.2 Add tests for `extractor.cpp` — doc extraction from AST
- [ ] 7.1.3 Add tests for `signature.cpp` — function signature rendering
- [ ] 7.1.4 Add tests for `generators.cpp` — doc output generation

### 7.2 Other Components

- [ ] 7.2.1 Add tests for `preprocessor.cpp` — conditional compilation directives
- [ ] 7.2.2 Add tests for `thir_lower.cpp` — THIR lowering
- [ ] 7.2.3 Add tests for `exhaustiveness.cpp` — pattern exhaustiveness checking
- [ ] 7.2.4 Add tests for `solver.cpp` / `solver_builtins.cpp` — trait solving

## Phase 8: Test Helpers & Infrastructure

- [ ] 8.1.1 Create shared MIR builder helpers for pass tests
- [ ] 8.1.2 Create shared LLVM module builder helpers for codegen tests
- [ ] 8.1.3 Create shared AST builder helpers for frontend tests
- [ ] 8.1.4 Document test patterns and conventions in compiler/tests/README.md
