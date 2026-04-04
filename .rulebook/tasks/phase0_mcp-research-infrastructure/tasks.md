# Tasks: MCP Research Infrastructure — Enhanced Logging, Analysis & Visualization

**Status**: Planning. 0% (0/52).
**Goal**: Transform MCP call logging from basic NDJSON into a full research platform with rich data, automated analysis, visualizations, and meta-insights on LLM debugging behavior.
**Location**: compiler/src/mcp/ (C++ server), docs/papers/llm-ir-debugging/scripts/ (analysis), docs/papers/llm-ir-debugging/dashboards/ (viz)

## Phase 1: Enhanced Data Collection — Fill Logging Gaps (7 items)

- [ ] 1.1 **Log test results** — capture `{total, passed, failed, crashed, timeouts}` from structured test output into MCP log entry (currently only `is_error` bool)
- [ ] 1.2 **Log normalized test target** — extract `target_file` or `suite` from params into top-level field for easy querying
- [ ] 1.3 **Log debug_layers source** — add `debug_layers_auto: true/false` to distinguish LLM-requested vs default-on-failure
- [ ] 1.4 **Log output token count** — add `output_tokens: N` to measure cost per tool (emit-ir can produce 50K+ tokens)
- [ ] 1.5 **Log error classification** — add `error_type: "compile" | "crash" | "assertion" | "timeout" | "none"` parsed from test result
- [ ] 1.6 **Log preceding tool** — add `preceded_by: "check"` to test calls for measuring check→test pipeline adoption
- [ ] 1.7 **Log files modified** — add `files_changed: ["path"]` by diffing working tree between tool calls (requires git status hook)

## Phase 2: Session Context & Tagging (5 items)

- [ ] 2.1 **Tag sessions with task ID** — `session_start` includes `task: "phase9a_ia-autograd"` from active Rulebook task
- [ ] 2.2 **Tag sessions with model** — `session_start` includes `model: "opus"` or `"sonnet"` (from agent context)
- [ ] 2.3 **Auto-summary on session_end** — compute and log `{total_calls, tools_used, error_rate, duration_sec, dominant_tool}`
- [ ] 2.4 **Snapshot CLAUDE.md hash** — log hash of CLAUDE.md at session_start for correlating rule changes with behavior
- [ ] 2.5 **Link commits to sessions** — git hook logs `{commit_hash, session_id, files_changed}` on each commit

## Phase 3: Storage & Query Upgrade (4 items)

- [ ] 3.1 **SQLite backend** — write MCP logs to SQLite in addition to JSONL (table: `tool_calls`, `sessions`, `test_results`)
- [ ] 3.2 **Schema migration** — backfill existing 3241 JSONL entries into SQLite
- [ ] 3.3 **CSV/Parquet export** — `scripts/export_data.py` for pandas/jupyter analysis
- [ ] 3.4 **Query helpers** — `scripts/query.py` with common queries: tool_distribution, error_rate_over_time, check_before_test_rate

## Phase 4: MCP Analysis Tool — Inline Research Access (5 items)

- [ ] 4.1 **`mcp__tml__analyze` tool** — new MCP tool returning JSON stats: `analyze(metric, period, group_by)`
- [ ] 4.2 **Metrics**: `tool_distribution`, `error_rate`, `check_adoption`, `debug_layers_usage`, `test_target_hotspots`
- [ ] 4.3 **Period filters**: `"7d"`, `"30d"`, `"session:ID"`, `"task:phase9a"`, `"all"`
- [ ] 4.4 **Group-by**: `"day"`, `"session"`, `"tool"`, `"module"`, `"task"`
- [ ] 4.5 **Integration**: register in MCP server, add to CLAUDE.md tool reference, add to docs

## Phase 5: Derived Metrics Engine (8 items)

- [ ] 5.1 **Time-to-first-pass** — time from first test call to first passing test per session
- [ ] 5.2 **Retry ratio** — number of test calls before first pass per file/module
- [ ] 5.3 **Research-before-action ratio** — % of sessions using docs/check before first test
- [ ] 5.4 **Tool diversity score** — Shannon entropy of tool distribution per session
- [ ] 5.5 **Diagnostic depth** — max IR layers inspected per session (check→emit-ir→emit-mir→debug_layers)
- [ ] 5.6 **Fix efficiency** — lines changed per successful test (requires git hook from 2.5)
- [ ] 5.7 **Cache miss rate** — cache_invalidate calls / total calls (incr cache health)
- [ ] 5.8 **Module hotspot score** — test failures per module / total tests per module

## Phase 6: Visualizations — Graphs & Dashboards (8 items)

- [ ] 6.1 **Tool adoption timeline** — line chart (X=day, Y=% per tool) with intervention markers showing when CLAUDE.md rules were added
- [ ] 6.2 **Markov transition heatmap** — tool×tool probability matrix showing workflow patterns
- [ ] 6.3 **Session workflow Sankey** — flow diagram showing check→test→emit-ir→fix paths
- [ ] 6.4 **Error rate stacked area** — error rate by tool over time
- [ ] 6.5 **Session duration histogram** — distribution + CDF of session complexity
- [ ] 6.6 **Test latency box plot** — compilation time by module (identify slow modules)
- [ ] 6.7 **Bug discovery scatter** — timeline of codegen bugs found vs resolved
- [ ] 6.8 **HTML dashboard generator** — `scripts/generate_dashboard.py` producing self-contained HTML with all charts (plotly.js)

## Phase 7: Automated Reports & Anomaly Detection (4 items)

- [ ] 7.1 **Weekly digest script** — `scripts/weekly_report.py` generates markdown report with all metrics + deltas
- [ ] 7.2 **Anomaly detection** — alert when error_rate > 2σ above rolling mean or tool adoption drops >30%
- [ ] 7.3 **Auto-save to docs/** — weekly report auto-saved to `docs/papers/llm-ir-debugging/reports/week-YYYY-WW.md`
- [ ] 7.4 **Regression guard** — CI check that error_rate hasn't regressed >5% from previous week

## Phase 8: Prompt Engineering Meta-Analysis (5 items)

- [ ] 8.1 **Rule half-life** — measure how long each CLAUDE.md rule maintains adoption before context compression "forgets" it
- [ ] 8.2 **Wording effect** — A/B test "NEVER" vs "avoid" vs "prefer" — which wording produces highest compliance
- [ ] 8.3 **Data citation effect** — compare adoption for rules with "observed: 8.8%" vs rules without data
- [ ] 8.4 **Position bias** — measure if rules at top of CLAUDE.md are followed more than rules at bottom
- [ ] 8.5 **Cross-model comparison** — same rules on Opus vs Sonnet vs Haiku — measure compliance delta

## Phase 9: A/B Testing Framework (3 items)

- [ ] 9.1 **Condition alternator** — automatically alternate between conditions across sessions (50/50 split)
- [ ] 9.2 **Condition registry** — define named conditions with specific CLAUDE.md rule sets and defaults
- [ ] 9.3 **Statistical comparator** — compute paired t-test, Cohen's d, and confidence intervals between conditions

## Phase 10: Documentation & Paper (3 items)

- [ ] 10.1 **Update experiment-protocol.md** — add Condition D (enhanced logging) protocol
- [ ] 10.2 **Write paper sections** — Introduction, Background, System Design, Methodology, Results, Discussion
- [ ] 10.3 **Finalize and review paper** — camera-ready with all graphs and statistical analysis
