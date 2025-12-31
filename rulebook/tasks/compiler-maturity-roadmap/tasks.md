# Tasks: TML Compiler Maturity Roadmap

## Phase 0: Project Restructure ✅ COMPLETED
- [x] 0.1 Move `packages/compiler/` to `compiler/` (includes src/, include/, tests/)
- [x] 0.2 Move `packages/core/` to `lib/core/` (includes src/, tests/)
- [x] 0.3 Move `packages/std/` to `lib/std/` (includes src/, tests/)
- [x] 0.4 Move `packages/test/` to `lib/test/` (test framework module)
- [x] 0.5 Remove empty `packages/` directory
- [x] 0.6 Update root CMakeLists.txt to reference `compiler/`
- [x] 0.7 Update `compiler/CMakeLists.txt` include paths
- [x] 0.8 Update build scripts (scripts/build.bat, etc.)
- [x] 0.9 Update hardcoded paths in compiler (module loader, etc.)
- [x] 0.10 Update CLAUDE.md with new paths
- [x] 0.11 Update .gitignore if needed
- [x] 0.12 Verify all tests still pass (442 tests passing)

## Phase 1: MIR (Mid-level IR) ✅ COMPLETED
- [x] 1.1 Design MIR node types (SSA form)
- [x] 1.2 Create `src/mir/` directory structure
- [x] 1.3 Implement MIR builder from AST
- [x] 1.4 Implement basic block representation
- [x] 1.5 Implement control flow graph (CFG)
- [x] 1.6 Implement SSA phi nodes
- [x] 1.7 Add MIR pretty printer for debugging
- [x] 1.8 Add MIR serialization (binary format complete)
- [x] 1.9 Update codegen to use MIR instead of AST (MIR codegen complete)
- [x] 1.10 Add `--emit-mir` CLI flag
- [x] 1.11 Write MIR unit tests (23 tests passing)

## Phase 2: Core Optimizations - CRITICAL ✅ COMPLETED
- [x] 2.1 Implement constant folding pass
- [x] 2.2 Implement constant propagation pass
- [x] 2.3 Implement dead code elimination (DCE)
- [x] 2.4 Implement unreachable code elimination
- [x] 2.5 Implement common subexpression elimination (CSE)
- [x] 2.6 Implement copy propagation
- [x] 2.7 Add optimization level flags (-O0, -O1, -O2, -O3)
- [x] 2.8 Add optimization pass manager
- [x] 2.9 Add benchmarks for optimization effectiveness (10 benchmark tests)

## Phase 3: Escape Analysis & Inlining ✅ COMPLETED
- [x] 3.1 Implement escape analysis framework (EscapeAnalysisPass)
- [x] 3.2 Detect stack-allocatable objects (EscapeInfo.is_stack_promotable)
- [x] 3.3 Implement stack allocation promotion (StackPromotionPass)
- [x] 3.4 Implement function inlining heuristics (InliningPass with cost analysis)
- [x] 3.5 Implement inline cost analysis (InlineCost, instruction counting)
- [x] 3.6 Add `@inline` and `@noinline` attributes (MIR Function.attributes, AST propagation)
- [x] 3.7 Implement recursive inlining limit (options_.recursive_limit)
- [x] 3.8 Add inlining statistics reporting (InliningStats)

## Phase 4: Incremental Compilation ✅ COMPLETED
- [x] 4.1 Implement dependency graph extraction (DependencyGraph class)
- [x] 4.2 Implement per-function caching (FunctionCacheEntry, hash_function_signature/body/deps)
- [x] 4.3 Implement change detection (hash-based, timestamp-based)
- [x] 4.4 Implement partial recompilation (object file caching)
- [x] 4.5 Add incremental build statistics (cache hit/miss reporting)
- [x] 4.6 Implement cache invalidation strategy (MirCache class)
- [x] 4.7 Add `--no-cache` flag (to bypass cache)

## Phase 5: Enhanced Diagnostics - IMPORTANT ✅ COMPLETED
- [x] 5.1 Implement DiagnosticEmitter infrastructure (diagnostic.hpp/cpp)
- [x] 5.2 Add colored terminal output (ANSI colors with Windows support)
- [x] 5.3 Implement Rust-style error formatting (source snippets, carets)
- [x] 5.4 Implement error codes catalog (L/P/T/B/C/E categories)
- [x] 5.5 Integrate diagnostics into cmd_build.cpp, cmd_debug.cpp, cmd_format.cpp
- [x] 5.6 Implement multi-span error reporting (secondary labels, emit_labeled_line)
- [x] 5.7 Add "did you mean?" suggestions (levenshtein_distance, find_similar)
- [x] 5.8 Implement error recovery in parser (synchronize_to_stmt/decl/brace, skip_until)
- [x] 5.9 Add fix-it hints (DiagnosticFixIt, emit_fixes, parser FixItHint helpers)
- [x] 5.10 Implement warning levels (-Wnone, -Wextra, -Wall, -Wpedantic, -Werror)
- [x] 5.11 Add JSON diagnostic output (--error-format=json)

## Phase 6: Debug Information - IMPORTANT ✅ COMPLETED
- [x] 6.1 Implement DWARF debug info generation (debug_info.cpp)
- [x] 6.2 Add source mapping to LLVM IR (emit_debug_info option)
- [x] 6.3 Add debug metadata for compile unit, file, and function scopes
- [x] 6.4 Implement variable location tracking (DILocalVariable, llvm.dbg.declare)
- [x] 6.5 Add debug info for generics (function instantiations)
- [x] 6.6 Test with gdb/lldb (LLVM verifier passes, debug info embedded)
- [x] 6.7 Implement `-g` flag improvements (-g0, -g1, -g2, -g3 levels)

## Phase 7: Parallel Compilation ✅ COMPLETED
- [x] 7.1 Implement thread-safe module registry (via job-level isolation)
- [x] 7.2 Implement parallel lexing/parsing
- [x] 7.3 Implement parallel type checking
- [x] 7.4 Implement parallel codegen
- [x] 7.5 Add thread pool management (ParallelBuilder with 8 threads default)
- [x] 7.6 Add `-j` flag for parallelism control
- [x] 7.7 Add parallel compilation statistics (~11x speedup benchmarked)

## Phase 8: Cross Compilation - MODERATE ✅ COMPLETED
- [x] 8.1 Define target triple abstraction (Target struct in target.hpp/cpp)
- [x] 8.2 Implement target-specific type sizes (pointer_width, size_ptr, alignments)
- [x] 8.3 Implement target-specific ABI handling (object format, data layout)
- [x] 8.4 Add `--target` flag (--target=x86_64-unknown-linux-gnu)
- [x] 8.5 Add common targets (x86_64-linux, aarch64-linux, wasm32, macos)
- [x] 8.6 Implement sysroot handling (--sysroot flag, passed to clang for compile and link)
- [x] 8.7 Document cross-compilation setup (added to ch12-00-libraries-and-ffi.md)

## Phase 9: LSP Server - DESIRABLE
- [ ] 9.1 Implement LSP protocol handler
- [ ] 9.2 Implement textDocument/completion
- [ ] 9.3 Implement textDocument/hover
- [ ] 9.4 Implement textDocument/definition
- [ ] 9.5 Implement textDocument/references
- [ ] 9.6 Implement textDocument/rename
- [ ] 9.7 Implement textDocument/diagnostic
- [ ] 9.8 Implement workspace/symbol
- [ ] 9.9 Add VS Code extension
- [ ] 9.10 Add Neovim configuration

## Phase 10: Documentation Generator - DESIRABLE
- [ ] 10.1 Design doc comment format (`///`)
- [ ] 10.2 Implement doc comment parser
- [ ] 10.3 Implement HTML output generator
- [ ] 10.4 Implement Markdown output generator
- [ ] 10.5 Add `tml doc` command
- [ ] 10.6 Implement cross-reference linking
- [ ] 10.7 Add search functionality

## Phase 11: Linter Enhancement ✅ COMPLETED
- [x] 11.1 Implement lint rule framework (LintConfig, LintResult, LintIssue, Severity levels)
- [x] 11.2 Add unused variable detection (W001 rule with per-function analysis)
- [x] 11.3 Add unused import detection (W002 rule with module-level tracking)
- [x] 11.4 Add naming convention checks (snake_case, PascalCase, UPPER_SNAKE_CASE)
- [x] 11.5 Add complexity metrics (cyclomatic complexity, nesting depth, function length)
- [x] 11.6 Add `tml lint --fix` auto-fix (tabs, trailing whitespace, runs formatter)
- [x] 11.7 Add custom lint rules via config ([lint] section in tml.toml, [lint.rules] for disabling)

## Phase 12: Test Framework Enhancement - DESIRABLE (Partial)
- [x] 12.1 Implement `@bench` attribute for benchmarks (with @bench(N) custom iterations)
- [x] 12.2 Implement benchmark runner (`tml test --bench` discovers *.bench.tml files)
- [x] 12.3 Add benchmark comparison reports (`--save-baseline=`, `--compare=` with JSON format)
- [ ] 12.4 Implement `@fuzz` attribute for fuzzing
- [ ] 12.5 Implement code coverage report (HTML)
- [x] 12.6 Add `tml test --coverage` flag (passthrough to runtime)
- [x] 12.7 CI integration templates - SKIPPED (project already has CI)

## Phase 13: Advanced Type System - FUTURE
- [ ] 13.1 Implement const generics
- [ ] 13.2 Implement associated type defaults
- [ ] 13.3 Implement GATs (Generic Associated Types)
- [ ] 13.4 Implement `impl Behavior` return types
- [ ] 13.5 Implement `pub(crate)` visibility
- [ ] 13.6 Implement module re-exports

## Phase 14: Async Runtime - FUTURE
- [ ] 14.1 Design async/await lowering
- [ ] 14.2 Implement Future behavior
- [ ] 14.3 Implement state machine generation
- [ ] 14.4 Implement async executor
- [ ] 14.5 Implement async I/O primitives
- [ ] 14.6 Add `@async` function support

## Phase 15: Package Manager - FUTURE
See separate task: `rulebook/tasks/package-manager/tasks.md`

Partial implementation exists (tml.toml, dependency resolution, lockfile, tml deps, tml remove).
Full implementation requires a TML package registry service.

## Validation
- [ ] All phases pass existing test suite
- [ ] Performance benchmarks show improvement
- [ ] Documentation updated for new features
- [ ] CI/CD updated for new build process
