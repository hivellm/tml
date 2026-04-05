# LLM-Assisted Debugging Through Multi-Layer IR Exposure

**Status**: Phase 1 Complete — 1321 calls analyzed across 129 sessions
**Author**: TML Project
**Date**: 2026-03-30
**Data Period**: 2026-03-25 to 2026-03-30 (5 days, 123.9 hours)

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

## Completed Contributions

1. **Empirical data** on LLM debugging behavior (1321 tool calls, 129 sessions, 5 days)
2. **Quantified impact of explicit rules** — documented anti-patterns increase adoption by +27% to +443%
3. **Tool usage patterns** — test dominates (60.3%), docs adoption increased 27%, debug_layers adoption increased 443%
4. **Infrastructure outcome** — findings directly motivated LLVM ORC JIT implementation (Phase 0, completion 2026-04-15)

## Key Findings (Phase 1)

- **Test-centric workflow**: 60.3% of calls are test runs; test accounts for 99% of computation time
- **Anti-pattern interventions work**: Explicit rules with justification increase adoption by average 168%
- **Fine-grained testing preferred**: 74.9% of tests target specific files (path parameter), not full suite (1.4%)
- **Diagnostic tools underutilized**: Only 4.3% of calls use emit-ir + emit-mir, despite 443% intervention increase
- **Structured output is default**: 94.5% adoption shows good UX matters more than explicit rules
- **JIT execution projected impact**: Current 37.2s average test latency → 2s with ORC JIT (18.5x speedup)

## Data Files

| File | Format | Contents |
|------|--------|----------|
| `data/mcp-call-log.jsonl` | NDJSON | All MCP tool calls (tool, params, timestamp, duration) |
| `data/sessions.json` | JSON | Session boundaries and bug classifications |
| `data/analysis.json` | JSON | Computed metrics per session |

## Analysis Documents

1. **`preliminary-analysis.md`** — Full data analysis (9 sections, 1321 calls, before/after metrics)
2. **`tool-taxonomy.md`** — Tool classification with observed metrics
3. **`experiment-protocol.md`** — Experimental design and methodology
4. **`debug-layers-design.md`** — Multi-layer IR debug output architecture
5. **`data/`** — Raw data files (mcp-call-log.jsonl, sessions, bugs)

## Next Steps (Phase 2)

1. **Extend data collection** — Gather another 2-4 weeks of data to reach larger sample
2. **Make debug_layers default** — Auto-emit IR on assertion failure (projected: 90%+ adoption)
3. **Compare with other LLM models** — Sonnet, Haiku, GPT-4o to test generalization
4. **Track time-to-resolution** — Measure debugging session effectiveness
5. **Publish findings** — Contribute to LLM+compiler research literature
