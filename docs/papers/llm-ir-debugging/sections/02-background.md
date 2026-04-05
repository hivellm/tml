# 2. Background

## 2.1 The Model Context Protocol

The Model Context Protocol (MCP) [4] is an open standard for connecting LLMs to external tools and data sources. MCP defines a client-server architecture where the LLM (client) invokes tools on a server via JSON-RPC. Each tool has a typed schema, and the server returns structured results. MCP enables LLMs to interact with compilers, test runners, documentation systems, and other development infrastructure through a uniform interface.

Unlike ad-hoc tool integrations (e.g., shell command execution), MCP provides a structured boundary where every invocation can be logged, classified, and analyzed. This property makes MCP an ideal instrumentation point for studying LLM behavior.

## 2.2 The TML Compiler

TML (To Machine Language) is a systems programming language designed for LLM code generation and analysis [5]. Its compiler is implemented in C++ with an embedded LLVM backend and follows a query-based demand-driven pipeline:

```
Source -> Lexer -> Parser -> Type Checker -> Borrow Checker
      -> HIR -> THIR -> MIR -> LLVM IR -> Object Code -> Executable
```

The compiler exposes 17 MCP tools spanning compilation, testing, diagnostics, documentation, and project management. These tools provide the LLM with access to every compilation layer, from type checking (`check`) through intermediate representations (`emit-ir`, `emit-mir`) to test execution (`test`).

The TML standard library contains 500+ types and 5,000+ functions, with active development across core data structures, SIMD intrinsics, networking, and database bindings. This breadth ensures that debugging sessions cover diverse domains and bug categories.

## 2.3 Multi-Layer Debug Output

A key system innovation in this study is the `--debug-layers` flag, which causes the compiler to emit diagnostic information from multiple compilation layers when a test fails. Rather than showing only the assertion failure message, `--debug-layers` provides:

- **Source**: The exact failing source line
- **HIR**: The desugared, type-resolved expression
- **MIR**: The SSA-form basic blocks with explicit control flow
- **LLVM IR**: The final IR before machine code generation
- **Diagnosis hints**: Compiler-generated suggestions about which layer likely contains the bug

This multi-layer output is designed to reduce the number of tool calls needed to diagnose a bug by providing complete diagnostic context in a single response. The hypothesis is that LLMs can pattern-match across IR layers more efficiently than executing sequential tool calls to gather the same information.

## 2.4 Prior Work

Research on LLM tool use has focused primarily on benchmarks measuring whether LLMs can correctly invoke tools [6, 7] rather than observing how they use tools in practice. Studies of LLM code generation [1, 8, 9] have examined output quality but not the iterative debugging process. Work on automated program repair [10, 11] has studied fix strategies but typically with constrained tool sets (edit + test).

The closest related work is studies of human debugging behavior [12, 13], which established that expert programmers use systematic diagnostic strategies while novices rely on trial-and-error. Our study asks whether LLMs exhibit similar patterns and whether their strategies can be shaped through prompt engineering.

Recent work on LLM agents [2, 3, 14] has demonstrated multi-step tool use in software engineering tasks, but these studies typically use curated benchmarks (SWE-bench [15], HumanEval [16]) rather than observing organic development. Our study fills this gap with longitudinal, in-situ data from production compiler development.
