# Proposal: MCP Research Infrastructure — Enhanced Logging, Analysis & Visualization

## Why

The current MCP call logger captures basic tool usage (name, params, duration, is_error) but misses critical data for understanding LLM debugging behavior: test assertion failures are invisible (is_error only captures crashes), sessions aren't tagged with task context, and analysis requires ad-hoc Python scripts. With 3241 calls across 298 sessions, we have enough data to justify building proper research infrastructure — but the data quality gaps limit what we can learn.

Key gaps identified from 10 days of organic data collection:
- **97% of test failures are invisible** — assertion failures don't set is_error
- **No task context** — can't correlate tool patterns with feature type (codegen vs library vs test)
- **No visualizations** — all analysis is manual Python one-liners
- **No inline access** — LLM must Read files + run scripts instead of querying an MCP tool
- **No causal measurement** — can't A/B test prompt engineering interventions

## What Changes

### C++ (compiler/src/mcp/)
- Enhanced log entries: test results, error classification, token counts, preceding tool
- Session tagging: task ID, model name, CLAUDE.md hash
- SQLite backend alongside JSONL
- New `mcp__tml__analyze` tool for inline research queries

### Python (docs/papers/llm-ir-debugging/scripts/)
- SQLite schema + JSONL backfill migration
- Derived metrics engine (8 metrics: time-to-pass, retry ratio, diversity score, etc.)
- Visualization suite (8 chart types: timeline, heatmap, Sankey, box plot, etc.)
- HTML dashboard generator (plotly.js, self-contained)
- Weekly digest automation
- A/B testing framework for prompt engineering

### Research (docs/papers/llm-ir-debugging/)
- Updated experiment protocol (Condition D)
- Prompt engineering meta-analysis (rule half-life, wording effect, position bias)
- Paper sections with statistical analysis

## Impact
- Affected code: compiler/src/mcp/ (C++ MCP server), docs/papers/ (research scripts)
- Breaking change: NO (additive fields in JSONL, backward-compatible)
- User benefit: Better understanding of how LLMs use debugging tools → better tool design → faster development cycles
- Research benefit: Publishable paper with rigorous methodology and visualizations

## Data

Current dataset: 3,241 calls, 298 sessions, 17 tools, 10 days
Key findings so far (12): check adoption 8.8%→25.3%, emit-ir 3.9%→7.2%, test dominance 60.3%→44.0%
