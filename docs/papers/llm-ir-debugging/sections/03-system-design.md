# 3. System Design

## 3.1 Instrumentation Architecture

We instrumented the TML MCP server to log every tool invocation to an append-only NDJSON file (`mcp-call-log.jsonl`). Each log entry contains:

```json
{
  "event": "tool_call",
  "session": "1774678866829",
  "seq": 4,
  "ts": "2026-04-01T14:30:05Z",
  "tool": "test",
  "params": { "suite": "core/str", "structured": true },
  "duration_ms": 37200,
  "is_error": false
}
```

The logging is transparent to the LLM -- it does not see or modify its behavior based on the log. Session identifiers are generated at conversation start and persist across all tool calls within a conversation.

## 3.2 Data Pipeline

The analysis pipeline processes raw NDJSON logs through three stages:

1. **Raw collection** (`mcp-call-log.jsonl`): Append-only, immutable, every tool call recorded with parameters and duration.
2. **Structured storage** (SQLite): Sessions table (metadata, aggregated metrics), tool_calls table (classified by category), transitions table (tool-to-tool edges for sequential analysis).
3. **Derived metrics**: Computed from the structured data -- adoption rates, error rates, transition probabilities, longitudinal trends.

## 3.3 Tool Taxonomy

We classify the 17 MCP tools into five categories based on their role in the debugging workflow:

| Category | Tools | Purpose |
|----------|-------|---------|
| Execution | `test`, `run`, `build`, `compile` | Verify hypotheses by running code |
| Diagnosis | `check`, `emit-ir`, `emit-mir`, `explain` | Inspect compiler internals at specific layers |
| Documentation | `docs/search`, `docs/get`, `docs/list`, `docs/resolve` | Research APIs and language features |
| Maintenance | `format`, `lint`, `cache/invalidate` | Code quality and cache management |
| Project | `project/coverage`, `project/structure`, `debug`, `profile`, `inspect` | Project-level operations and runtime debugging |

This taxonomy enables analysis at the category level (e.g., "What fraction of calls are diagnostic?") while preserving tool-level granularity.

## 3.4 Metrics Definitions

We define the following metrics for analyzing LLM debugging behavior:

- **Check adoption rate**: `check_calls / (check_calls + test_calls)`. Measures the fraction of validation effort spent on fast type-checking versus full test execution.
- **Diagnosis ratio**: `diagnosis_calls / total_calls`. Measures overall diagnostic engagement.
- **IR preference**: `(emit_ir + emit_mir) / total_calls`. Measures direct IR inspection frequency.
- **Error rate**: `error_calls / total_calls`. Measures how often tool invocations produce errors (compilation failures, test failures, invalid parameters).
- **Test granularity**: Distribution of test calls across single-file, suite-level, and full-suite invocations.
- **debug_layers adoption**: Fraction of test calls that include the `debug_layers=true` parameter.
