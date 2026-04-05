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

## Condition D: Enhanced Logging

Condition D extends the JSONL schema with richer per-call context to enable
deeper analysis (prompt engineering effects, per-test-result attribution,
session-level behavioral metrics).

### New Schema Fields

**`session_start` additions:**
```json
{
  "event": "session_start",
  "session": "1742902141234-a3f2",
  "ts": "2026-04-04T12:00:00Z",
  "model": "claude-opus-4-6",
  "condition": "enhanced",
  "claude_md_hash": "sha256:abc123...",
  "rule_versions": {
    "check-before-test": 3,
    "debug-layers": 2
  }
}
```

**`tool_call` additions:**
```json
{
  "event": "tool_call",
  "session": "...",
  "seq": 4,
  "ts": "2026-04-04T12:00:05Z",
  "tool": "test",
  "params": { "suite": "core/str", "debug_layers": true },
  "duration_ms": 1240,
  "is_error": false,
  "error_class": null,
  "preceded_by": "check",
  "test_result": {
    "total": 42,
    "passed": 41,
    "failed": 1,
    "failures": ["test_split_empty"]
  }
}
```

**New fields (all optional for backward compatibility):**

| Field | Type | Description |
|-------|------|-------------|
| `model` | string | LLM model used (e.g. `claude-opus-4-6`) |
| `condition` | string | A/B condition name (`baseline`, `debug-layers`, `enhanced`) |
| `claude_md_hash` | string | SHA-256 of CLAUDE.md at session start |
| `rule_versions` | object | Per-rule version counters from CLAUDE.md |
| `error_class` | string\|null | Error classification: `type_error`, `codegen`, `runtime`, `test_failure`, `timeout` |
| `preceded_by` | string\|null | Tool called immediately before this one |
| `test_result` | object\|null | Structured test output (for `test` tool calls only) |

### Analysis Pipeline (JSONL → SQLite → Query → Visualize)

```
mcp-call-log.jsonl
    │
    ▼  migrate_to_sqlite.py
docs/papers/llm-ir-debugging/mcp_research.db   (SQLite)
    │
    ├─ sessions table: session metadata + aggregated metrics
    ├─ tool_calls table: every call with classifications
    └─ transitions table: tool-to-tool edges for Markov analysis
    │
    ▼  query.py
Structured metrics: tool adoption rates, error classification,
                    session-level behavioral summaries
    │
    ▼  visualize.py / generate_dashboard.py
HTML charts, markdown tables, PNG figures for paper
```

**Key queries:**
- `query.py --adoption --by-week` — weekly adoption time series
- `query.py --error-class --by-condition` — error rate by category and condition
- `query.py --transitions --heatmap` — tool transition probabilities
- `query.py --compare baseline enhanced` — side-by-side condition stats

### Activation Criteria

Condition D is active when:
1. MCP server emits `model`, `claude_md_hash`, `preceded_by` in every `tool_call`
2. `test` tool calls include `test_result` in structured output
3. `error_class` is populated by the MCP error classifier

Until then, Condition C (debug-layers) remains active and Condition D fields
are collected opportunistically from sessions where they appear.

## Condition B Activation Log

**Date**: 2026-03-26
**Commit**: `--debug-layers` enabled as default in MCP test tool
**Change**: `debug_layers` default flipped from `false` to `true` in `mcp_tools.cpp`
**Logger**: Condition tag flipped from `baseline` to `debug-layers` in `mcp_server.cpp`
**Revert**: Set `TML_DEBUG_LAYERS=0` env var to return to Condition A

### Condition A Data Collection Period
- Start: 2026-03-24 (session with `--debug-layers` flag implementation)
- End: 2026-03-26 (this commit)
- Sessions: ~5-8 organic debugging sessions (codegen fixes)
- Key bugs fixed under A: MIR print dispatch, missing args, template literals, heap corruption

### Condition B Data Collection Period
- Start: 2026-03-26 (this commit)
- End: TBD (until sufficient organic data collected)
- All MCP `test` calls now include `--debug-layers` by default
- On failure, output includes HIR + MIR + LLVM IR + diagnosis hints

## Timeline

- **Week 1**: Instrument MCP (call logger) — DONE
- **Week 2**: Implement --debug-layers Phases 1-3 (LLVM IR + MIR + HIR) — DONE
- **Week 3-4**: Condition A (baseline) organic data collection — DONE
- **Week 5+**: Condition B (debug-layers default ON) organic data collection — ACTIVE
- **TBD**: Post-hoc analysis, bug classification, paper writing
