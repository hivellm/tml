# LLM Debugging Behavior: An Empirical Study of Tool Usage Patterns in Compiler Development

## Paper Outline

### 1. Introduction
- LLMs increasingly used as coding assistants for complex systems (compilers, databases)
- Debugging requires multi-step tool chains: type check → emit IR → compare → fix
- Research question: How do LLMs use debugging tools, and how can we improve their efficiency?
- Contribution: First empirical study of LLM debugging tool usage in production compiler development

### 2. Background
- **Model Context Protocol (MCP)**: Tool interface between LLMs and development environments
- **TML Compiler**: A from-scratch compiler with 17+ MCP tools (check, test, emit-ir, emit-mir, debug, docs)
- **Debug layers**: Multi-layer IR diagnostics (HIR → MIR → LLVM IR) with inline diagnosis hints
- **Prior work**: LLM code generation studies, tool-use benchmarks, software debugging research

### 3. System Design
- **Enhanced logging**: NDJSON with test results, error classification, session context, preceding tool
- **Three-tier data pipeline**: JSONL (raw) → SQLite (structured) → Derived metrics (research)
- **Session tagging**: Active task ID, model name, CLAUDE.md rule hash
- **8 derived metrics**: time-to-pass, retry ratio, research-before-action, tool diversity, diagnostic depth, fix efficiency, cache miss rate, module hotspot score

### 4. Methodology
- **Conditions**: A (baseline), B (debug-layers), C (doc-search rules), D (enhanced logging)
- **Data collection**: Organic usage over 30+ days of active compiler development
- **Metrics**: Tool adoption rates, error rates, diagnostic depth, time-to-first-pass
- **Statistical analysis**: Paired t-tests, Cohen's d effect sizes, rolling averages

### 5. Results

#### 5.1 Tool Usage Patterns
- Tool distribution timeline (Figure 1)
- Tool transition Markov chain (Figure 2)
- Dominant workflow patterns: test-dominated vs diagnostic-heavy sessions

#### 5.2 Rule Effectiveness
- Check-before-test adoption: baseline → intervention → decay curve
- Debug layers adoption: default-on-failure vs explicit request rates
- Rule half-life: how long CLAUDE.md rules maintain compliance

#### 5.3 Error Analysis
- Error classification distribution: compile > assertion > crash > timeout
- Module hotspots: which code areas generate most debugging effort
- Fix efficiency: lines changed per successful test pass

#### 5.4 Prompt Engineering Insights
- Wording effect: "NEVER" vs "avoid" vs "prefer"
- Data citation effect: rules with observed metrics vs without
- Position bias: compliance by rule position in prompt
- Cross-model comparison: Opus vs Sonnet vs Haiku compliance

### 6. Discussion
- **Tool design implications**: Which tools are underutilized and why
- **Prompt engineering**: What makes debugging rules effective for LLMs
- **Feedback loops**: How tool output quality affects subsequent tool choices
- **Limitations**: Single project, single language, organic (not controlled) data collection

### 7. Threats to Validity
- **Internal**: Rules changed over time (mitigated by CLAUDE.md hash tracking)
- **External**: Single developer, single project, single LLM family
- **Construct**: Token count is estimated (chars/4), not exact

### 8. Conclusion
- Summary of key findings
- Recommendations for MCP tool designers
- Future work: multi-project study, controlled experiments, real-time adaptive rules

### Appendices
- A. Complete JSONL schema
- B. CLAUDE.md rule catalog with effectiveness scores
- C. Full statistical tables
