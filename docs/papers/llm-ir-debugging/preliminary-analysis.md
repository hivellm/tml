# Preliminary Analysis — MCP Call Log Data

**Date**: 2026-03-28
**Data period**: 2026-03-25 to 2026-03-28
**Log file**: `mcp-call-log.jsonl`

## 1. Dataset Overview

| Metric | Value |
|--------|-------|
| Total sessions | 60 |
| Total tool calls | 238 |
| Condition A (baseline) sessions | 6 |
| Condition B (debug-layers) sessions | 54 |
| Unique tools used | 12 |
| Date range | ~3 days |

## 2. Tool Frequency Distribution

| Tool | Calls | % | Avg Duration |
|------|-------|---|-------------|
| test | 144 | 60.5% | ~146s* |
| docs/search | 25 | 10.5% | 605ms |
| run | 25 | 10.5% | 5.5s |
| check | 21 | 8.8% | 1.1s |
| docs/list | 6 | 2.5% | 870ms |
| emit-ir | 5 | 2.1% | 525ms |
| emit-mir | 4 | 1.7% | 679ms |
| docs/get | 3 | 1.3% | 5ms |
| compile | 2 | 0.8% | 932ms |
| explain | 1 | 0.4% | 55ms |
| project/coverage | 1 | 0.4% | 317s |
| cache/invalidate | 1 | 0.4% | 59ms |

*test duration skewed by one 14M ms outlier (likely a timeout/crash)

**Key finding**: `test` alone accounts for 60.5% of all tool usage. The LLM's primary interaction with the compiler is running tests repeatedly.

## 3. Error Rates

| Tool | Errors/Total | Rate |
|------|-------------|------|
| run | 9/25 | 36.0% |
| compile | 1/2 | 50.0% |
| emit-mir | 2/4 | 50.0% |
| check | 6/21 | 28.6% |
| test | 3/144 | 2.1% |
| docs/* | 0/34 | 0.0% |
| emit-ir | 0/5 | 0.0% |

**Key finding**: `run` and `check` have high error rates (36%, 29%), consistent with iterative development where the LLM submits code that initially fails type checking or compilation. `test` has low error rate (2.1%) — tests compile but may crash at runtime (reported as crashes, not errors).

## 4. Tool Transition Patterns

Top transitions (Markov chain, first-order):

| From | To | Count | Interpretation |
|------|----|-------|----------------|
| test | test | 114 | **Dominant pattern**: test-fix-retest loop |
| docs/search | docs/search | 17 | Documentation exploration |
| run | run | 15 | Iterative execution loop |
| check | check | 5 | Type check iteration |
| check | emit-ir | 2 | **Diagnostic narrowing**: type check fails → inspect IR |
| emit-ir | emit-mir | 2 | **Diagnostic narrowing**: IR → MIR drill-down |
| run | test | 2 | Execution → validation |
| docs/list | docs/search | 2 | Module browse → keyword search |

**Key finding**: The `test → test` self-loop (114 out of 178 total transitions = 64%) is the dominant behavior pattern. The LLM operates primarily in a tight test-fix-retest cycle. Diagnostic tools (emit-ir, emit-mir) are used rarely, and only in a clear "narrowing" pattern (check → emit-ir → emit-mir).

## 5. Condition Comparison (A vs B)

### Condition A — Baseline (no debug-layers default)

| Metric | Value |
|--------|-------|
| Sessions | 6 |
| Total calls | 22 |
| Avg calls/session | 3.7 |
| Avg session duration | 65.9s |
| Error rate | 4.5% |
| Top tools | docs/search (55%), test (18%), docs/list (18%) |
| debug_layers used | 0 times |

### Condition B — Debug-layers enabled

| Metric | Value |
|--------|-------|
| Sessions | 54 |
| Total calls | 216 |
| Avg calls/session | 4.0 |
| Avg session duration | 390.5s |
| Error rate | 9.7% |
| Top tools | test (65%), run (12%), check (10%) |
| debug_layers used | 3 times |

### Comparison Notes

The two conditions are **not directly comparable** due to:
1. **Severe sample imbalance**: 6 vs 54 sessions
2. **Different task types**: Baseline sessions were exploratory (documentation browsing), while debug-layers sessions were implementation/debugging
3. **Temporal confound**: Baseline sessions were all from a single early period; debug-layers sessions span the full development period

The `debug_layers=true` parameter was used only **3 times out of 216 calls** (1.4%) in Condition B. This suggests the LLM does not proactively use the multi-layer IR diagnostic even when available, preferring simpler tools (check, test with default output).

## 6. Session Size Distribution

| Metric | Value |
|--------|-------|
| Min calls/session | 1 |
| Max calls/session | 27 |
| Average | 4.0 |
| Median | 1 |

The median of 1 indicates many sessions are ephemeral (single tool call, likely MCP reconnections). Substantive debugging sessions have 5-27 calls.

## 7. Key Findings for Paper

### Finding 1: Test-dominated workflow
The LLM spends 60.5% of its tool interactions running tests. The `test → test` self-loop (64% of all transitions) is the defining behavior pattern. This suggests the LLM's debugging strategy is overwhelmingly "trial and error" — modify code, run test, observe result, repeat.

### Finding 2: Diagnostic tools are underutilized
Despite having `emit-ir`, `emit-mir`, `check`, and `debug_layers` available, the LLM uses IR-level tools in only 3.8% of calls. Even when `debug_layers` is the default condition, it's explicitly requested only 3 times. The LLM prefers high-level feedback (test pass/fail) over low-level diagnostic data.

### Finding 3: Diagnostic narrowing exists but is rare
When the LLM does use diagnostic tools, it follows a logical narrowing pattern: `check → emit-ir → emit-mir`. This suggests the LLM understands the compilation pipeline hierarchy but only engages it when simpler strategies fail.

### Finding 4: Documentation as pre-implementation research
`docs/search` is the second most common tool (10.5%), and transitions show `docs/search → docs/search` (17x) and `docs/list → docs/search` (2x) — the LLM performs documentation research in bursts before implementation, consistent with the CLAUDE.md instruction to "consult language reference before implementing."

### Finding 5: High error rates in development tools
`run` (36%) and `check` (29%) have high error rates, confirming that the LLM submits incomplete or incorrect code during iterative development. This is normal behavior — the tools serve as fast feedback loops rather than "submit when ready" validation.

## 8. Limitations

1. **Small sample**: 60 sessions, 238 calls is insufficient for robust statistical analysis
2. **Condition imbalance**: 6 baseline vs 54 debug-layers prevents fair comparison
3. **Single LLM**: All data from one model (Claude Opus 4.5/4.6), no cross-model comparison
4. **Task confound**: Different sessions involve different task types (exploration, implementation, debugging)
5. **No ground truth**: We cannot definitively link tool usage patterns to debugging success without manual session labeling

## 9. Recommendations

1. **Focus on qualitative case studies** — Select 3-5 sessions with clear debugging episodes and trace the tool usage narrative
2. **Increase baseline data** — Run deliberate baseline sessions (disable debug-layers) on equivalent tasks
3. **Investigate debug_layers underuse** — Why doesn't the LLM use it? Is the output too verbose? Too slow? Not actionable?
4. **Label sessions by task type** — Classify each session as exploration/implementation/debugging to enable within-type comparison
5. **Extend data collection period** — Continue collecting for 2-4 more weeks to reach statistical significance
