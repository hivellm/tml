# Proposal: TML Compiler Maturity Roadmap

## Status
- **Created**: 2025-12-29
- **Status**: Draft
- **Priority**: High

## Why

The TML compiler is currently at v0.1.0 with 80/84 tests passing and approximately 34,180 lines of C++ code. While the architecture is well-designed and modular, there are significant gaps compared to production-grade compilers like Go and Rust that need to be addressed to make TML viable for real-world use.

### Current State Analysis

| Component | Status | Lines |
|-----------|--------|-------|
| Lexer | Complete | ~2,000 |
| Parser (LL(1)) | Complete | ~2,500 |
| Type Checker (3-pass) | Complete | ~8,000 |
| Borrow Checker (NLL) | Complete | ~1,500 |
| IR Builder | Basic | ~2,000 |
| Codegen (LLVM IR) | Functional | ~12,000 |
| **Total** | **95% tests** | **~34,180** |

### Problems

1. **No intermediate optimization layer** - Code goes directly from AST to LLVM IR without TML-level optimizations
2. **Missing critical compiler features** - No escape analysis, limited inlining, no dead code elimination at IR level
3. **Poor developer experience** - No LSP server, limited error recovery, no package manager
4. **Project structure confusion** - Compiler code in `packages/compiler/` can be confused with TML library modules

## What Changes

This roadmap introduces a phased approach to bring TML compiler to production quality:

### Phase 1: Project Restructure
Move compiler from `packages/compiler/` to project root for clarity

### Phase 2: Intermediate Representation (MIR)
Add SSA-based MIR between AST and LLVM IR to enable optimizations

### Phase 3: Compiler Optimizations
Implement essential optimization passes

### Phase 4: Developer Tooling
Add LSP server, package manager, and improved diagnostics

### Phase 5: Production Features
Cross-compilation, parallel compilation, advanced debugging

## Impact

- **Breaking**: Project structure change requires build system updates
- **Performance**: Optimizations will significantly improve generated code
- **DX**: Developer experience will match modern language toolchains
- **Adoption**: Production-ready compiler enables real-world usage

## References

- Go compiler: https://go.dev/src/cmd/compile/
- Rust compiler: https://rustc-dev-guide.rust-lang.org/
- LLVM IR: https://llvm.org/docs/LangRef.html
