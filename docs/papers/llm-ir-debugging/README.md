# LLM-Assisted Debugging Through Multi-Layer IR Exposure

**Status**: Infrastructure Complete — Baseline Data Collection Active
**Author**: TML Project
**Date**: 2026-03-25

## Abstract

This paper investigates whether Large Language Models (LLMs) achieve faster and more accurate debugging when compiler error messages include intermediate representation (IR) data from the compilation layer where the error originates. We hypothesize that exposing HIR, THIR, MIR, and LLVM IR alongside traditional error messages reduces the number of LLM interactions required to identify root causes, particularly for codegen and type-system bugs.

## Thesis

> LLMs debug more effectively when given structured IR context at the appropriate compilation layer, because IR is a formal, unambiguous representation that maps directly to the LLM's strength in pattern matching over structured data — unlike natural language error messages which require inference about compiler internals.

## Research Questions

1. **RQ1**: Does exposing IR data alongside error messages reduce the number of tool calls needed to diagnose a bug?
2. **RQ2**: Which IR layer (HIR, THIR, MIR, LLVM IR) is most useful for which class of bugs?
3. **RQ3**: When given both simple error messages and detailed IR dumps, do LLMs prefer the IR-based debugging path or the traditional read-file-and-guess approach?
4. **RQ4**: What is the optimal granularity of IR exposure — full module IR, function-scoped IR, or instruction-level IR?

## Methodology

### Data Collection Infrastructure

1. **MCP Call Logger** — Records every tool invocation (tool name, parameters, timestamp, duration) in NDJSON format. No output content stored (privacy + size). Located at `build/debug/mcp-call-log.jsonl`.

2. **Multi-Layer Debug Output** — Optional `--debug-layers` flag for test/compile that emits:
   - Source location + error message (always)
   - HIR fragment for the failing expression (opt-in)
   - MIR basic block containing the error (opt-in)
   - LLVM IR for the function (opt-in)

3. **A/B Comparison** — Run debugging sessions with and without IR exposure, measure:
   - Number of tool calls to reach correct diagnosis
   - Number of file reads (proxy for "LLM is guessing")
   - Time to first correct fix attempt
   - Total interaction count

### Tool Call Taxonomy

| Category | Tools | Indicates |
|----------|-------|-----------|
| **Diagnosis** | `emit-ir`, `emit-mir`, `check` | LLM is investigating compilation layers |
| **Navigation** | `Read`, `Grep`, `Glob` | LLM is searching for context (traditional approach) |
| **Execution** | `test`, `run`, `build` | LLM is verifying a hypothesis |
| **Documentation** | `docs_search`, `docs_get` | LLM needs API/syntax reference |

### Metrics

- **Diagnosis Efficiency** = correct_diagnosis / total_tool_calls
- **IR Preference Rate** = ir_tool_calls / (ir_tool_calls + navigation_tool_calls)
- **Fix Accuracy** = first_fix_correct / total_fix_attempts
- **Layer Accuracy** = correct_layer_identified / total_bugs

## Expected Contributions

1. **Empirical data** on LLM debugging behavior with compiler IR exposure
2. **Design guidelines** for compiler error messages optimized for LLM consumption
3. **Tool usage patterns** showing how LLMs choose between IR-based and file-navigation-based debugging
4. **Practical implementation** of multi-layer debug output in a production compiler

## Data Files

| File | Format | Contents |
|------|--------|----------|
| `data/mcp-call-log.jsonl` | NDJSON | All MCP tool calls (tool, params, timestamp, duration) |
| `data/sessions.json` | JSON | Session boundaries and bug classifications |
| `data/analysis.json` | JSON | Computed metrics per session |

## Paper Structure (Planned)

1. Introduction — Problem statement, motivation
2. Background — LLMs for code, compiler IRs, MCP protocol
3. System Design — TML compiler, MCP tools, multi-layer debug
4. Methodology — Data collection, metrics, experimental design
5. Results — Tool usage patterns, IR preference, diagnosis efficiency
6. Discussion — When IR helps, when it doesn't, design implications
7. Related Work — LLM debugging, compiler diagnostics, IDE integration
8. Conclusion — Guidelines for LLM-optimized compiler diagnostics
