# 4. Methodology

## 4.1 Study Design

This is an observational study of LLM debugging behavior during organic compiler development. Unlike controlled experiments with curated bug sets, we observe the LLM (Claude Opus 4.6 [17]) as it develops the TML compiler and standard library in real-time, encountering and fixing bugs as they arise naturally.

The study covers 30 days of active development (2026-03-05 to 2026-04-04), during which the LLM implemented standard library modules (string operations, iterators, collections, SIMD intrinsics, database bindings), fixed compiler codegen bugs, and maintained test coverage.

## 4.2 Experimental Conditions

The study encompasses two primary conditions, with transitions occurring during the observation period:

**Condition A (Baseline)**: Standard error messages only. The LLM must manually invoke `check`, `emit-ir`, or `emit-mir` to inspect compilation layers. Active from 2026-03-05 to 2026-03-26. Approximately 6 sessions, ~238 calls.

**Condition B (Debug-Layers Default)**: The `--debug-layers` flag is enabled by default on all test calls. On test failure, the output automatically includes HIR, MIR, and LLVM IR for the failing function, along with diagnosis hints. Active from 2026-03-26 onward. 292 sessions, ~3,013 calls.

Additionally, the system prompt (`CLAUDE.md`) was iteratively updated with behavioral rules throughout the study period. Key interventions include:

- **INT-001** (2026-03-26): Added "Use `check` BEFORE `test`" rule with quantitative justification ("check is 10x faster").
- **INT-002** (2026-03-28): Added "ALWAYS use `debug_layers=true` on the FIRST test failure" rule.
- **INT-003** (2026-03-29): Added "NEVER read source files to understand APIs -- use MCP docs tools" rule.

## 4.3 Data Collection

All data was collected from organic development -- the LLM was not given curated bugs or artificial tasks. This design choice prioritizes ecological validity over experimental control. The LLM encountered real bugs during real development, used tools as it chose, and was free to adopt or ignore prompt-based rules.

Data collection is automatic and transparent. The MCP server logs every tool call without LLM awareness. No calls were filtered or excluded from analysis. The total dataset comprises 3,251 tool calls across 300 sessions using 17 distinct tools.

## 4.4 Threats to Internal Validity

The observational design introduces several confounds:

1. **Rule accumulation**: The system prompt grew over time, so later sessions have more behavioral guidance. We address this by tracking longitudinal trends and comparing early vs. late sessions.
2. **Task variation**: Different development tasks (library work vs. codegen debugging) naturally produce different tool distributions. We address this through the case study (Section 5.4).
3. **Learning effects**: The LLM may improve through accumulated experience within and across sessions. Session-level analysis partially controls for this.
