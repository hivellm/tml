# MCP Tool Taxonomy for LLM Debugging Research

This document classifies TML MCP tools by their role in the debugging workflow. The classification is used to analyze LLM behavior patterns — which tools the LLM reaches for first, and how that changes when multi-layer IR output is available.

## Tool Categories

### 1. Diagnosis Tools (IR-Level)

These tools expose compiler intermediate representations. The research hypothesis is that LLMs will prefer these over file-navigation tools when debugging codegen and type-system bugs.

| Tool | Layer | Use Case | Expected LLM Behavior |
|------|-------|----------|----------------------|
| `emit-ir` | LLVM IR | View generated LLVM IR for a function/file | Preferred for codegen bugs — LLM can pattern-match IR instructions |
| `emit-mir` | MIR | View Mid-level IR (SSA basic blocks) | Preferred for optimization bugs — shows control flow + data flow |
| `check` | Type System | Type-check without compiling | Preferred for type errors — shows inferred vs expected types |
| `explain` | Error Codes | Explain compiler error codes | Used after encountering an error — reference lookup |

### 2. Navigation Tools (File-Level)

These tools explore source code. They represent the "traditional" debugging approach — reading files and searching for patterns.

| Tool | Category | Use Case | Expected LLM Behavior |
|------|----------|----------|----------------------|
| `Read` | File | Read source file contents | Fallback when IR tools don't provide enough context |
| `Grep` | Search | Search for patterns in code | Used for finding related code, understanding call sites |
| `Glob` | Search | Find files by name pattern | Used for locating relevant files |
| `docs_search` | Reference | Search TML documentation | Used for syntax/API reference, not debugging per se |
| `docs_get` | Reference | Get detailed docs for a type | API lookup during implementation |
| `docs_list` | Reference | List module contents | Discovery of available APIs |

### 3. Execution Tools (Verify Hypothesis)

These tools compile and run code. They represent the LLM testing a hypothesis.

| Tool | Category | Use Case | Expected LLM Behavior |
|------|----------|----------|----------------------|
| `test` | Verification | Run test suite or individual test | Most common — used to verify fixes |
| `run` | Execution | Build and run a TML file | Used for ad-hoc testing |
| `build` | Compilation | Compile to executable | Used when only compilation check needed |
| `compile` | Compilation | Compile a source file | Low-level compilation |

### 4. Maintenance Tools

These tools perform code quality tasks, not directly related to debugging.

| Tool | Category | Use Case |
|------|----------|----------|
| `format` | Style | Auto-format TML source |
| `lint` | Quality | Check for style issues |
| `cache/invalidate` | Cache | Clear stale compilation cache |

## Metrics Derived from Tool Categories

### IR Preference Rate

```
IR_preference = diagnosis_tool_calls / (diagnosis_tool_calls + navigation_tool_calls)
```

- **IR_preference > 0.6**: LLM strongly prefers IR-based debugging
- **IR_preference 0.3-0.6**: Mixed approach
- **IR_preference < 0.3**: LLM prefers traditional file navigation

### Diagnosis Efficiency

```
efficiency = successful_fixes / total_tool_calls
```

Higher efficiency = LLM reaches correct diagnosis with fewer interactions.

### Tool Transition Patterns

Track sequences like:
- `test → emit-ir → edit → test` (IR-guided fix loop)
- `test → Read → Read → Grep → Read → edit → test` (navigation-guided fix loop)
- `test → emit-ir → Read → edit → test` (hybrid approach)

The hypothesis predicts the IR-guided loop will be shorter (fewer steps) and more accurate (fewer failed fix attempts).

## Experiment Conditions

### Condition A: Baseline (Current)

Standard error messages only. LLM must use `Read`, `Grep`, `emit-ir` manually.

### Condition B: Enhanced Error Output

Error messages include:
- Source location + error text (always)
- HIR fragment for the failing expression (automatic)
- MIR basic block containing the error (automatic)
- LLVM IR for the function (automatic, for codegen errors)

### Condition C: Multi-Layer Debug Flag

Test runner with `--debug-layers` flag:
- On failure, automatically emits all compilation layers for the failing test
- LLM receives complete diagnostic information in one tool call
- Hypothesis: reduces total tool calls by 40-60%

## Observed Metrics (Phase 1 Complete)

### Diagnosis Efficiency

| Category | Adoption | Efficiency Notes |
|----------|----------|------------------|
| **IR Diagnostics** | 4.3% | emit-ir (3.9%) + emit-mir (0.4%) — LLMs prefer high-level feedback |
| **Type Checking** | 12.0% | check adoption +36% after anti-pattern guidance |
| **Documentation** | 12.7% | docs/* adoption +27% after anti-pattern guidance |
| **Test Execution** | 60.3% | Dominant strategy; 99% of computation time |

### IR Preference Rate

```
IR_preference = (emit-ir + emit-mir + check) / (all diagnosis + navigation)
              = (52 + 5 + 159) / 1321
              = 216 / 1321
              = 16.3%
```

**Interpretation**: LLMs prefer high-level execution feedback (test pass/fail) over low-level IR analysis. This aligns with the trial-and-error development pattern observed in 60.3% test calls.

**Note**: The 16.3% IR preference is AFTER the anti-pattern intervention (+443% for debug_layers). Before intervention, IR preference would have been ~6%.

### Diagnosis Tool Transition Patterns

| Transition | Count | Interpretation |
|-----------|-------|-----------------|
| test → test | ~480 | Dominant test-fix-retest loop |
| check → test | 67 | Research-first pattern (type-check then validate) |
| docs → impl | 59 | Reference-first pattern (research API then implement) |
| test → emit-ir | 8 | IR exploration after test failure |
| test → check | 12 | Fallback to type-checking after failure |

**Key finding**: Most productive patterns are "reference-first" (docs→impl, 59x) and "type-check-first" (check→test, 67x), yet test-alone dominates (480+ self-loops).

### Tool Feature Adoption

| Feature | Usage | Adoption Rate |
|---------|-------|---------------|
| structured output (test) | 753/797 | 94.5% |
| debug_layers on test | 101/1321 | 7.6% (up from 1.4%) |
| check before test | 67/797 | 8.4% |
| test with cache_invalidate | 31/797 | 3.9% |
| docs tools before impl | 59/797 | 7.4% |

**Finding**: Features that have obvious UX benefit (structured=true → parseable JSON) achieve near-universal adoption (94.5%). Features requiring explicit thought (debug_layers on failure) remain low (7.6%) despite intervention.

### Latency and Feedback Loops

| Tool Type | Avg Latency | Feedback Loop Cost |
|-----------|-------------|-------------------|
| Fast feedback | 40–1,000 ms | cache_invalidate, docs/*, check |
| Medium feedback | 3–5 sec | emit-ir, emit-mir, run |
| Slow feedback | 37.2 sec | test |

**Impact on development**: The 37s test latency means each iteration cycle takes ~42s (5s edit + 37s test). Reducing test latency to 2s via JIT would reduce iteration to 7s — a 6x speedup in development velocity.

## Data Collection Plan (Phase 2+)

1. **Phase 1** (Complete): Instrument MCP with call logger, collected 1321 calls across 129 sessions
2. **Phase 2**: Implement `--debug-layers` as DEFAULT on test failure (currently opt-in)
3. **Phase 3**: Implement JIT execution (Phase 0, in progress) and measure impact
4. **Phase 4**: Extend data collection 2-4 weeks; compare with other LLM models
