# Tasks: Self-Hosting Preparation + Cranelift Completion

**Status**: Future (0%)
**Priority**: Low — non-trivial, depends on stable compiler infrastructure
**Extracted from**: `compiler-infrastructure` Phases 8.11-8.12 and 12

## Phase 1: Cranelift Backend Completion

- [ ] 1.1 Verify 80%+ of tests pass with Cranelift backend
- [ ] 1.2 Benchmark: compile time LLVM -O0 vs Cranelift (target: 3x faster)

## Phase 2: Self-Hosting Preparation

- [ ] 2.1 Design bootstrap plan: Stage 0 (C++) -> Stage 1 (partial TML) -> Stage 2 (full TML)
- [ ] 2.2 Create `compiler-tml/` directory
- [ ] 2.3 Rewrite lexer in TML (core, string, number, operator, ident, token)
- [ ] 2.4 Tests: TML lexer produces identical tokens to C++ lexer
- [ ] 2.5 Rewrite parser in TML (Pratt parser, declarations, statements, patterns, types)
- [ ] 2.6 Tests: TML parser produces identical AST to C++ parser
- [ ] 2.7 Cross-validate: compile test suite with Stage 0 and Stage 1, compare outputs
- [ ] 2.8 Benchmark: TML lexer/parser performance vs C++ (target: < 2x overhead)
