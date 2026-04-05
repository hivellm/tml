# LLM Debugging Behavior in Compiler Development: An Empirical Study of Tool Usage Patterns via the Model Context Protocol

**Andre Ferreira**

HiveLLM Project, 2026

---

## Table of Contents

1. [Abstract](sections/00-abstract.md)
2. [Introduction](sections/01-introduction.md)
3. [Background](sections/02-background.md)
4. [System Design](sections/03-system-design.md)
5. [Methodology](sections/04-methodology.md)
6. [Results](sections/05-results.md)
7. [Discussion](sections/06-discussion.md)
8. [Threats to Validity](sections/07-threats-validity.md)
9. [Related Work](sections/08-related-work.md)
10. [Conclusion](sections/09-conclusion.md)
11. [References](sections/10-references.md)

---

## Paper Statistics

| Metric | Value |
|--------|-------|
| Total sections | 11 |
| Total lines | ~574 |
| Estimated word count | ~5,940 |
| Data collection period | 30 days |
| Tool invocations analyzed | 3,251 |
| Sessions observed | 300 |
| Distinct tools studied | 17 |

---

## How to Read This Paper

This paper is organized as a collection of self-contained sections, each in its own file for easier navigation and review. The sections follow a logical progression:

- **Section 1** (Introduction): Motivates the study — why LLM debugging behavior in compiler development is understudied and what MCP instrumentation enables.
- **Section 2** (Background): Covers the TML compiler, the MCP tool set, and the data collection infrastructure.
- **Section 3** (System Design): Describes the MCP server architecture, the 17 tools exposed, and the logging mechanism.
- **Section 4** (Methodology): Defines the metrics, the 30-day observation window, and the behavioral intervention design.
- **Section 5** (Results): The core empirical findings — tool distribution, adoption curves, the SIMD case study, latency impact, and transition patterns.
- **Section 6** (Discussion): Implications for tool design, prompt engineering, and the shift from trial-and-error to diagnostic reasoning.
- **Section 7** (Threats to Validity): Internal, external, and construct validity — what limits the generalizability of the findings.
- **Section 8** (Related Work): Situates the study relative to LLM code generation, tool use, automated program repair, and human debugging research.
- **Section 9** (Conclusion): Five key findings, recommendations for tool designers, prompt engineers, and researchers, and future work directions.
- **Section 10** (References): Full bibliography [1]–[20].

Each section can be read independently, though the full paper provides the most comprehensive understanding.

---

## Key Findings

### 1. LLMs Are Overwhelmingly Test-Centric (Section 5)
52.7% of all tool calls are `test` invocations. LLMs prefer definitive pass/fail feedback over incremental diagnostic analysis, mirroring trial-and-error patterns seen in novice human programmers.

### 2. Prompt Interventions Produce Measurable Behavioral Change (Sections 5–6)
A single rule ("use `check` before `test`") raised type-checking adoption from 8.8% to 25.3% over 10 days, with an accelerating (not plateauing) trajectory.

### 3. Feature Adoption Correlates Inversely with Cognitive Friction (Section 6)
Default-on features (`structured=true`) reach 95.7% adoption. Optional features requiring mode-switching (`debug_layers`) reach only 11.1% despite explicit rules.

### 4. A Structural Shift Toward Diagnostic Reasoning Is Underway (Section 6)
Test share fell from 60% to 44% over 30 days as `check` and `emit-ir` usage grew. Late sessions show `check` exceeding `test` in frequency.

### 5. Tool Latency Is the Primary Development Bottleneck (Section 5)
Tests run at ~37 seconds per call and account for 52.7% of all calls. Projected JIT execution (~2 seconds) would reduce the edit-test cycle from 42 to 7 seconds.

---

## Citation

```
A. Ferreira, "LLM Debugging Behavior in Compiler Development: An Empirical Study
of Tool Usage Patterns via the Model Context Protocol," HiveLLM Project, 2026.
```
