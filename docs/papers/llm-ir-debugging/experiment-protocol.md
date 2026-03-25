# Experiment Protocol

## Objective

Measure whether multi-layer IR output in compiler diagnostics reduces the number of LLM interactions needed to diagnose and fix bugs.

## Variables

### Independent Variable
- **Debug output level**: A (baseline), B (enhanced errors), C (full --debug-layers)

### Dependent Variables
- **Tool call count**: Total MCP tool calls to reach correct fix
- **Diagnosis tool ratio**: emit-ir + emit-mir + check calls / total calls
- **Navigation tool ratio**: Read + Grep + Glob calls / total calls
- **Fix attempts**: Number of edit→test cycles before passing
- **Time to fix**: Wall clock from first failure to passing test (measured via call log timestamps)

### Controlled Variables
- Same LLM model (Claude Opus 4.6)
- Same bug set (curated from real TML compiler bugs)
- Same system prompt (CLAUDE.md + rules)
- Same MCP tool set (only debug-layers output differs)

## Bug Classification

Each bug is classified by the compilation layer where the root cause lives:

| Layer | Example Bug | Expected Best Condition |
|-------|------------|------------------------|
| **Lexer/Parser** | Missing token, wrong precedence | A (baseline sufficient) |
| **Type System** | Wrong type inferred, missing impl | B (type info in error) |
| **HIR** | Wrong desugaring, bad monomorphization | B or C |
| **MIR** | Wrong SSA, bad optimization pass | C (needs MIR output) |
| **Codegen** | ABI mismatch, wrong LLVM instruction | C (needs LLVM IR) |
| **Runtime** | C function bug, memory corruption | C (needs IR + runtime) |

## Bug Set (Curated)

Source: Real bugs from TML development history (git log). Each bug has:
- Commit hash of the fix
- Files changed
- Root cause layer
- Minimal reproduction

### Selection Criteria
1. Bug must be reproducible from a failing test
2. Bug must have a clear root cause in ONE layer
3. Bug must be fixable without architectural changes
4. Bug should be representative of its layer (not edge cases)

### Target: 30 bugs total
- 5 per layer (Lexer/Parser, Type, HIR, MIR, Codegen, Runtime)
- Each bug tested under all 3 conditions = 90 sessions

## Session Protocol

### Setup
1. Fresh conversation (no prior context about the bug)
2. Load CLAUDE.md and rules
3. Start MCP call logger

### Task
"The following test is failing. Diagnose the root cause and fix it."

### Data Collection
1. MCP call log (automatic via NDJSON logger)
2. Session transcript (manual save)
3. Bug classification (pre-labeled)
4. Success/failure (did the LLM fix the bug?)
5. Fix correctness (does the fix match the known-good fix?)

### Termination
- **Success**: Test passes with a correct fix
- **Failure**: 20+ tool calls without progress, or LLM gives up
- **Timeout**: 30 minutes wall clock

## Analysis Plan

### Per-Bug Metrics
```python
{
  "bug_id": "str-repeat-abi",
  "layer": "codegen",
  "condition": "C",
  "total_calls": 4,
  "diagnosis_calls": 1,    # emit-ir
  "navigation_calls": 0,   # no Read/Grep needed
  "execution_calls": 2,    # test (before + after fix)
  "edit_calls": 1,
  "fix_attempts": 1,
  "success": true,
  "time_to_fix_s": 45,
  "ir_preference": 1.0     # used IR, not navigation
}
```

### Aggregate Analysis
1. **Per-condition means**: avg tool calls, avg fix attempts, success rate
2. **Per-layer breakdown**: Which condition works best for which layer?
3. **Tool transition heatmap**: What tool follows what? (Markov chain)
4. **IR preference curve**: How does IR preference change by bug layer?

### Expected Results

| Metric | Condition A | Condition B | Condition C |
|--------|------------|------------|------------|
| Avg tool calls | 8-12 | 5-8 | 3-5 |
| Avg fix attempts | 2-3 | 1-2 | 1 |
| Success rate | 70-80% | 80-90% | 90-95% |
| IR preference | 0.2-0.3 | 0.3-0.5 | 0.6-0.8 |

### Statistical Tests
- Paired t-test per bug (same bug across conditions)
- ANOVA for cross-condition comparison
- Effect size (Cohen's d) for practical significance

## Timeline

- **Week 1**: Instrument MCP (call logger) — DONE
- **Week 2**: Curate bug set from git history
- **Week 3**: Implement --debug-layers Phase 1 (LLVM IR on failure)
- **Week 4**: Run Condition A (baseline) sessions
- **Week 5**: Run Condition B (enhanced errors) sessions
- **Week 6**: Implement --debug-layers Phase 2-3 (MIR + HIR)
- **Week 7**: Run Condition C (full debug-layers) sessions
- **Week 8**: Analysis and paper writing
