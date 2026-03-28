## 1. Infrastructure — MCP Call Logger
- [x] 1.1 Add NDJSON call logger to McpServer (tool, params, timestamp, duration, no output)
- [x] 1.2 Generate session_start/session_end markers with unique session ID
- [x] 1.3 Support TML_MCP_LOG_DIR env var for log path override
- [x] 1.4 Build compiler and verify logger compiles
- [x] 1.5 Create NDJSON schema in docs/papers/llm-ir-debugging/data/schema.json

## 2. Paper Structure
- [x] 2.1 Create docs/papers/llm-ir-debugging/README.md (abstract, thesis, research questions)
- [x] 2.2 Create tool-taxonomy.md (Diagnosis/Navigation/Execution/Maintenance categories)
- [x] 2.3 Create debug-layers-design.md (--debug-layers technical design)
- [x] 2.4 Create experiment-protocol.md (A/B/C conditions, metrics, statistical tests)

## 3. Data Collection (Organic)
- [x] 3.1 MCP call logger collects data automatically during normal development
- [x] 3.2 Create labeling schema for post-hoc bug classification (data/session-labels-schema.json)
- [x] 3.3 Create analysis script to parse NDJSON logs (scripts/analyze_logs.py — summary, sessions, metrics, transitions, export)

## 4. --debug-layers Phase 1 — LLVM IR on Failure
- [x] 4.1 Add --debug-layers flag to CLI argument parser (TestOptions + TestConfig + parse_args)
- [x] 4.2 On test failure, re-compile failing test with --emit-ir (emit_debug_layers_for_failures)
- [x] 4.3 Extract function-scoped LLVM IR (extract_function_ir with brace-depth tracking)
- [x] 4.4 Append IR to test failure output (appended to CoordinatorTestResult.error)
- [x] 4.5 Add debug_layers parameter to MCP test tool

## 5. --debug-layers Phase 2 — MIR on Failure
- [x] 5.1 Add function-scoped MIR printer (print_function_by_name in mir.hpp + extract_mir_function in coordinator)
- [x] 5.2 On test failure, emit MIR via tml build --emit-mir, append to error output
- [x] 5.3 Map test function name → MIR function name (fn test_<name> convention + substring match)

## 6. --debug-layers Phase 3 — HIR on Failure
- [x] 6.1 HIR printer already exists (HirPrinter::print_function, print_expr in hir_printer.hpp)
- [x] 6.2 Implement --emit-hir CLI flag (dispatcher.cpp + build.cpp → writes .hir file)
- [x] 6.3 On test failure, emit HIR via tml build --emit-hir, extract function, append to error
- [x] 6.4 Map test function name → HIR function (same "func <name>" pattern as MIR)
- N/A 6.5 THIR has no standalone printer (internal step between HIR→MIR, not separately emittable)

## 7. --debug-layers Phase 4 — Diagnosis Hints
- [x] 7.1 Implement generate_diagnosis_hints() with pattern matching on error + IR content
- [x] 7.2 Flag layer failures (HIR failed, MIR failed, IR failed) as structured hints
- [x] 7.3 Include layer name, symptom, possible causes for codegen/runtime/assertion patterns

## 8. Experiment — Organic Data Collection
- [x] 8.1 Condition A (baseline): MCP call logger active, --debug-layers NOT default (current state)
- [x] 8.2 Condition B (enhanced): --debug-layers enabled as default in MCP test tool (d805b08a, 2026-03-26)
- [x] 8.3 Post-hoc: classified 6 Condition A sessions (all exploration, no deep debugging in MCP logs)
- [x] 8.4 Compare tool usage patterns between conditions A and B (preliminary — see preliminary-analysis.md)
  - Note: Comparison limited by severe sample imbalance (6 vs 54 sessions) and task type confound
  - Key finding: debug_layers used only 3/216 calls (1.4%) even when default — underutilized
  - Need more baseline data for fair statistical comparison

## 8.5 Condition C — Prompt Reinforcement (2026-03-28)
- [x] 8.5.1 Analyzed preliminary data: docs tools 10.5%, check 8.8%, debug_layers 1.4%
- [x] 8.5.2 Added "Quick Decision Guide" table to .claude/rules/mcp-tool-reference.md
- [x] 8.5.3 Added "NEVER Read Source Files to Understand APIs" to CLAUDE.md + consult-language-reference.md
- [x] 8.5.4 Added "check BEFORE test" workflow rule to CLAUDE.md (check is 10x faster)
- [x] 8.5.5 Added "ALWAYS use debug_layers on FIRST failure" rule to CLAUDE.md
- [x] 8.5.6 All rules reference observed data percentages for transparency
- [ ] 8.5.7 Collect data for 1-2 weeks under Condition C, compare with A/B

## 9. Analysis & Paper
- [x] 9.1 Parse all NDJSON logs, compute per-session metrics (preliminary-analysis.md, 2026-03-28)
  - 60 sessions, 238 calls, 12 tools, test=60.5%, test→test loop=64% of transitions
- [ ] 9.2 Compute aggregate statistics per condition (means, success rates) — needs more baseline data
- [ ] 9.3 Run statistical tests (paired t-test, Cohen's d) — blocked: n too small for significance
- [x] 9.4 Generate tool transition analysis (Markov chain first-order, in preliminary-analysis.md)
  - Heatmap visualization deferred until more data collected
- [ ] 9.5 Write paper sections: Introduction, Background, System Design, Methodology, Results, Discussion
- [ ] 9.6 Review and finalize paper

## 12. Documentation
- [x] 12.1 Document --debug-layers in docs/user/ch13-04-debug-layers.md
- [x] 12.2 Update CHANGELOG.md with 0.2.4 entry
