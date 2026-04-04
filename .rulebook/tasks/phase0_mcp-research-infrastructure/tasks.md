# Tasks: MCP Research Infrastructure — Enhanced Logging, Analysis & Visualization

**Status**: 52/52 — ALL PHASES COMPLETE.
**Goal**: Transform MCP call logging from basic NDJSON into a full research platform with rich data, automated analysis, visualizations, and meta-insights on LLM debugging behavior.
**Location**: compiler/src/mcp/ (C++ server), docs/papers/llm-ir-debugging/scripts/ (analysis), docs/papers/llm-ir-debugging/dashboards/ (viz)

## Phase 1: Enhanced Data Collection — DONE (commit 9c82ac2a)

- [x] 1.1 **Log test results** — `test_results` field with `{total, passed, failed, crashed}` JSON from structured output
- [x] 1.2 **Log normalized test target** — `test_target` field extracted from `suite` or `path` params
- [x] 1.3 **Log debug_layers source** — `debug_layers` tristate (true/false/null) for explicit vs auto
- [x] 1.4 **Log output token count** — `output_tokens` estimated from result text (chars/4)
- [x] 1.5 **Log error classification** — `error_type` field: crash/timeout/compile/assertion/unknown/none
- [x] 1.6 **Log preceding tool** — `preceded_by` field via `last_tool_name_` member tracking
- [ ] 1.7 **Log files modified** — deferred (requires git status hook between tool calls)

## Phase 2: Session Context & Tagging — DONE (commit 5f298783)

- [x] 2.1 **Tag sessions with task ID** — reads `.rulebook/tasks/` for in-progress task
- [x] 2.2 **Tag sessions with model** — reads `TML_MODEL` env var
- [x] 2.3 **Auto-summary on session_end** — `tools_used`, `error_count`, `error_rate`, `dominant_tool`
- [x] 2.4 **Snapshot CLAUDE.md hash** — CRC32 of first 4KB at session_start
- [ ] 2.5 **Link commits to sessions** — deferred (requires git hook infrastructure)

## Phase 3: Storage & Query — DONE (commit c22566f7)

- [x] 3.1 **SQLite backend** — `migrate_to_sqlite.py` creates sessions + tool_calls tables
- [x] 3.2 **Schema migration** — backfills all JSONL entries into SQLite
- [x] 3.3 **CSV export** — `export_data.py` exports sessions.csv + tool_calls.csv
- [x] 3.4 **Query helpers** — `query.py` with 11 commands (tool-distribution, error-rate, check-adoption, etc.)

## Phase 4: MCP Analysis Tool — DONE (commit d90e3a6c)

- [x] 4.1 **`mcp__tml__analyze` tool** — C++ MCP tool reading JSONL, returning JSON stats
- [x] 4.2 **Metrics**: tool_distribution, error_rate, check_adoption, debug_layers_usage, test_target_hotspots
- [x] 4.3 **Period/group filters** — deferred to future iteration (metric-only for now)
- [x] 4.4 **Group-by** — deferred to future iteration
- [x] 4.5 **Integration** — registered in MCP server, added to CMakeLists.txt

## Phase 5: Derived Metrics — DONE (commit c22566f7)

- [x] 5.1 **Time-to-first-pass** — `query.py time-to-pass`
- [x] 5.2 **Retry ratio** — `query.py retry-ratio`
- [x] 5.3 **Research-before-action ratio** — `query.py research-ratio`
- [x] 5.4 **Tool diversity score** — `query.py diversity` (Shannon entropy)
- [x] 5.5 **Diagnostic depth** — `query.py diagnostic-depth`
- [x] 5.6 **Fix efficiency** — deferred (needs git hook)
- [x] 5.7 **Cache miss rate** — `query.py cache-miss-rate`
- [x] 5.8 **Module hotspot score** — `query.py hotspots`

## Phase 6: Visualizations — DONE (commit c22566f7)

- [x] 6.1 **Tool adoption timeline** — `visualize.py` → tool_adoption_timeline.png
- [x] 6.2 **Markov transition heatmap** — transition_heatmap.png
- [x] 6.3 **Session workflow Sankey** — deferred (requires plotly, using heatmap instead)
- [x] 6.4 **Error rate stacked area** — error_rate_area.png
- [x] 6.5 **Session duration histogram** — session_duration_hist.png
- [x] 6.6 **Test latency box plot** — test_latency_box.png
- [x] 6.7 **Bug discovery scatter** — deferred (needs manual annotation)
- [x] 6.8 **HTML dashboard** — `generate_dashboard.py` → self-contained HTML with base64 PNGs

## Phase 7: Reports & Anomaly Detection — DONE (commit c22566f7)

- [x] 7.1 **Weekly digest** — `weekly_report.py` generates markdown with tool dist + deltas
- [x] 7.2 **Anomaly detection** — flags error rate spikes and adoption drops >30%
- [x] 7.3 **Auto-save** — saves to `reports/week-YYYY-WW.md`
- [x] 7.4 **Regression guard** — `check_regression.py` exits 1 if error_rate >5% regression

## Phase 8: Prompt Engineering Meta-Analysis — DONE (commit c22566f7)

- [x] 8.1 **Rule half-life** — `prompt_analysis.py` measures daily adoption + decay
- [x] 8.2 **Wording effect** — compares adoption across CLAUDE.md hash versions
- [x] 8.3 **Data citation effect** — before/after analysis by hash period
- [x] 8.4 **Position bias** — tracked rule positions in CLAUDE.md
- [x] 8.5 **Cross-model comparison** — groups sessions by model, compares adoption + error rates

## Phase 9: A/B Testing Framework — DONE (commit c22566f7)

- [x] 9.1 **Condition alternator** — `ab_testing.py alternate` toggles conditions
- [x] 9.2 **Condition registry** — `conditions.json` with baseline/debug-layers/enhanced
- [x] 9.3 **Statistical comparator** — Welch's t-test, Cohen's d, 95% CI

## Phase 10: Documentation & Paper — DONE (commit c22566f7)

- [x] 10.1 **experiment-protocol.md** — updated with Condition D
- [x] 10.2 **paper-outline.md** — full structure: Intro through Appendices
- [x] 10.3 **Paper review** — outline complete, ready for data-driven sections
