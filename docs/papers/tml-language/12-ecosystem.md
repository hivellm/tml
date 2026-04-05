# 12. TML Ecosystem and Development Environment

## 12.1 Overview

TML ecosystem extends beyond compiler and standard library to include specialized tooling for LLM-assisted development, task management, and active research into how LLMs debug and implement code. This chapter describes: Model Context Protocol (MCP) integration, Rulebook system, and LLM-IR-Debugging research.

---

## 12.2 Model Context Protocol (MCP) Integration

### 12.2.1 MCP Server Implementation

TML provides a complete MCP server exposing 17 tools for compiler and development operations. MCP is a standardized interface allowing Claude, ChatGPT, Gemini, and other LLM tools to invoke operations.

The protocol uses request-response JSON-RPC 2.0 format. An LLM sends requests; the server executes operations and returns JSON results.

### 12.2.2 Tool Taxonomy

The 17 tools classify into four categories.

Diagnosis tools expose compiler intermediate representations: emit-ir, emit-mir, check, explain.

Navigation tools explore source code: Read, Grep, Glob, docs_search, docs_get, docs_list.

Execution tools compile and run: test, run, build, compile.

Maintenance tools handle quality: format, lint, cache_invalidate.

### 12.2.3 Tool Logging and Research

Every MCP tool invocation is logged to mcp-call-log.jsonl with metadata. This infrastructure enables research into LLM tool-use patterns.

The log contains no output content (privacy), only: tool name, parameters, duration, session ID, model. Lightweight logging allows longitudinal analysis.

As of April 2026, log contains 3,241 tool calls across 298 sessions, providing empirical data on LLM compiler tool use.

---

## 12.3 Rulebook System for Task Management

### 12.3.1 Architecture

Rulebook (npm package @hivehub/rulebook) provides persistent task management and memory for AI agents. TML uses Rulebook to organize work, persist progress across sessions, and enable Ralph (autonomous AI iteration loops).

Tasks are in rulebook/tasks/ directory. Each contains:
- tasks.md: Simple checklist
- proposal.md: Detailed specification
- notes.md: Session-by-session progress

### 12.3.2 Persistent Memory

The mcp__rulebook__rulebook_memory tools provide memory across sessions. Developers save key discoveries: architectural decisions, bug fixes, patterns, insights.

Example: Integer literals infer as I32 by default, causing index errors in 64-bit loops. Saved with key type_inference_i32_i64. In subsequent sessions, LLM retrieves context and avoids pitfall.

Memory uses semantic search (embeddings) to retrieve relevant past context. This reduces documentation overhead.

### 12.3.3 Ralph: Autonomous Iteration Loops

Ralph orchestrates multi-agent teams to complete tasks. Developer provides high-level goal; Ralph decomposes into subtasks, dispatches agents, aggregates results, reports back.

Ralph is currently in alpha but demonstrates potential for AI-first development: humans define goals; AI handles implementation.

---

## 12.4 LLM-IR-Debugging Research

### 12.4.1 Research Question

Does exposing compiler intermediate representations (HIR, MIR, LLVM IR) alongside error messages reduce LLM interactions needed to diagnose and fix bugs?

TML hypothesizes the opposite of conventional wisdom: LLMs excel at pattern matching over structured data and may debug more effectively with formal IR representations.

### 12.4.2 Methodology

Research collects empirical data from organic TML development. As developers use MCP tools, system logs every call. Calls classify into diagnosis, navigation, and execution.

Metrics: IR preference rate, diagnosis efficiency, fix accuracy, tool transition patterns.

### 12.4.3 Preliminary Findings (Phase 1)

Data: 3,241 tool calls, 298 sessions, 10 days (March 25 - April 4, 2026).

Key findings:
- Test tool: 52.7 percent (down from 60.3)
- Check tool: 17.5 percent (up 98 percent from 8.8)
- Emit-IR: 7.2 percent (up 85 percent from 3.9)
- Debug-layers: 9.0 percent (up 443 percent from 1.4)
- Error rate improved: 13.2 to 11.2 percent

The 443 percent increase in debug-layers adoption is the largest effect.

### 12.4.4 Projected Impact (Phase 2)

Phase 2 will make debug-layers default-on-failure. Projected: 90 percent adoption.

Phase 3 will implement LLVM ORC JIT (Phase 0, in progress), reducing test latency from 37.2 to 2-3 seconds. This 12x speedup will increase iterations and efficiency.

---

## 12.5 Build System

### 12.5.1 CMake-Based with Custom Scripts

TML uses CMake for portability but provides custom scripts handling critical environment setup.

Key outputs: tml.exe (monolithic, 100MB with LLVM) or modular build with tml_compiler.dll and tml_codegen_x86.dll.

Build time: approximately 100 seconds (Intel i7/Ryzen 7), I/O bound (37s linking, 63s compilation).

### 12.5.2 Zig CC as C/C++ Compiler

TML uses Zig CC instead of MSVC, GCC, or Clang. Zig CC provides cross-platform C compilation without platform-specific installation, improving CI/CD reliability.

---

## 12.6 Comparison with Other Language Ecosystems

Rust plus Cargo plus Crates.io emphasizes decentralized package management. Tradeoff: careful dependency management required.

Go plus Go Module System has built-in integration. Benefits from integration but lacks LLM-specific features.

Python plus Pip/Poetry plus PyPI is mature but fragmented. Multiple package managers and ad-hoc version management.

TML takes different approach with integrated MCP tooling, persistent task management, and research infrastructure. Emphasis is on enabling LLMs to be effective development partners, not on building large package ecosystem.

This reflects TML target use case: building AI-adjacent services where LLM-assisted development is norm.

---

## 12.7 Summary

TML ecosystem reflects deliberate design philosophy: optimize for LLM-assisted development through integrated MCP tooling, persistent task management, and research-driven tool design.

The MCP server and tool logging create closed-loop feedback: LLM data informs tool design, improves LLM effectiveness, generates better research data. This virtuous cycle is unique and positions TML as testbed for LLM-centric language design.
