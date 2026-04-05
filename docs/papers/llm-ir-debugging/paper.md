# LLM Debugging Behavior in Compiler Development: An Empirical Study of Tool Usage Patterns via the Model Context Protocol

**Andre Ferreira**

HiveLLM Project, 2026

---

## Abstract

Large Language Models (LLMs) are increasingly deployed as autonomous coding agents for complex systems engineering, yet little empirical data exists on how they use debugging tools when developing compilers. We present the first observational study of LLM debugging tool usage during organic compiler development, collecting 3,251 tool invocations across 300 sessions over 30 days via the Model Context Protocol (MCP). Our analysis reveals that LLMs are overwhelmingly test-centric (52.7% of calls), underutilize intermediate representation (IR) diagnostics (7.5%), and respond measurably to prompt-based behavioral interventions -- with type-checking adoption rising from 8.8% to 25.3% over 10 days following explicit rules. We identify a structural shift from "run and see" toward "analyze then verify" debugging strategies, driven by prompt engineering and domain experience. These findings have implications for MCP tool design, LLM prompt engineering, and the emerging field of LLM-assisted systems programming.

---

## 1. Introduction

The application of Large Language Models to software engineering has expanded rapidly from code completion [1] to autonomous multi-step development workflows [2, 3]. While code generation quality has received substantial attention, the *debugging behavior* of LLMs -- how they diagnose and fix errors in complex systems -- remains poorly understood. This gap is especially acute for systems programming tasks like compiler development, where bugs span multiple abstraction layers (lexer, parser, type checker, intermediate representations, machine code generation) and require structured diagnostic reasoning.

Compiler development presents a uniquely challenging debugging domain. A single bug may manifest as a runtime crash, but its root cause could lie in type inference, intermediate representation (IR) construction, optimization passes, or code generation. Effective debugging requires navigating these layers systematically -- a task that tests whether LLMs can execute structured multi-step diagnostic workflows rather than relying on trial-and-error iteration.

The Model Context Protocol (MCP) [4] provides a standardized interface between LLMs and development tools, enabling fine-grained instrumentation of tool usage. By logging every MCP tool invocation during organic compiler development, we can observe LLM debugging behavior in situ -- without the artificial constraints of benchmarks or the confounds of contrived tasks.

This paper makes the following contributions:

1. **The first empirical dataset** of LLM debugging tool usage in production compiler development: 3,251 calls across 300 sessions, 17 distinct tools, spanning 30 days of organic development on the TML compiler.

2. **Quantitative analysis of tool usage patterns**, revealing that LLMs are overwhelmingly test-centric (52.7%), underutilize IR diagnostics (7.5%), and strongly prefer fine-grained over comprehensive testing (74.9% single-file tests).

3. **Evidence that prompt-based interventions measurably change LLM debugging behavior**, with type-checking adoption accelerating from 8.8% to 25.3% over 10 days following explicit rules with quantitative justification.

4. **Design recommendations for MCP tool ecosystems**, including default-on diagnostics, auto-suggestion of pre-validation steps, and latency reduction as the primary lever for tool adoption.

---

## 2. Background

### 2.1 The Model Context Protocol

The Model Context Protocol (MCP) [4] is an open standard for connecting LLMs to external tools and data sources. MCP defines a client-server architecture where the LLM (client) invokes tools on a server via JSON-RPC. Each tool has a typed schema, and the server returns structured results. MCP enables LLMs to interact with compilers, test runners, documentation systems, and other development infrastructure through a uniform interface.

Unlike ad-hoc tool integrations (e.g., shell command execution), MCP provides a structured boundary where every invocation can be logged, classified, and analyzed. This property makes MCP an ideal instrumentation point for studying LLM behavior.

### 2.2 The TML Compiler

TML (To Machine Language) is a systems programming language designed for LLM code generation and analysis [5]. Its compiler is implemented in C++ with an embedded LLVM backend and follows a query-based demand-driven pipeline:

```
Source -> Lexer -> Parser -> Type Checker -> Borrow Checker
      -> HIR -> THIR -> MIR -> LLVM IR -> Object Code -> Executable
```

The compiler exposes 17 MCP tools spanning compilation, testing, diagnostics, documentation, and project management. These tools provide the LLM with access to every compilation layer, from type checking (`check`) through intermediate representations (`emit-ir`, `emit-mir`) to test execution (`test`).

The TML standard library contains 500+ types and 5,000+ functions, with active development across core data structures, SIMD intrinsics, networking, and database bindings. This breadth ensures that debugging sessions cover diverse domains and bug categories.

### 2.3 Multi-Layer Debug Output

A key system innovation in this study is the `--debug-layers` flag, which causes the compiler to emit diagnostic information from multiple compilation layers when a test fails. Rather than showing only the assertion failure message, `--debug-layers` provides:

- **Source**: The exact failing source line
- **HIR**: The desugared, type-resolved expression
- **MIR**: The SSA-form basic blocks with explicit control flow
- **LLVM IR**: The final IR before machine code generation
- **Diagnosis hints**: Compiler-generated suggestions about which layer likely contains the bug

This multi-layer output is designed to reduce the number of tool calls needed to diagnose a bug by providing complete diagnostic context in a single response. The hypothesis is that LLMs can pattern-match across IR layers more efficiently than executing sequential tool calls to gather the same information.

### 2.4 Prior Work

Research on LLM tool use has focused primarily on benchmarks measuring whether LLMs can correctly invoke tools [6, 7] rather than observing how they use tools in practice. Studies of LLM code generation [1, 8, 9] have examined output quality but not the iterative debugging process. Work on automated program repair [10, 11] has studied fix strategies but typically with constrained tool sets (edit + test).

The closest related work is studies of human debugging behavior [12, 13], which established that expert programmers use systematic diagnostic strategies while novices rely on trial-and-error. Our study asks whether LLMs exhibit similar patterns and whether their strategies can be shaped through prompt engineering.

Recent work on LLM agents [2, 3, 14] has demonstrated multi-step tool use in software engineering tasks, but these studies typically use curated benchmarks (SWE-bench [15], HumanEval [16]) rather than observing organic development. Our study fills this gap with longitudinal, in-situ data from production compiler development.

---

## 3. System Design

### 3.1 Instrumentation Architecture

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

### 3.2 Data Pipeline

The analysis pipeline processes raw NDJSON logs through three stages:

1. **Raw collection** (`mcp-call-log.jsonl`): Append-only, immutable, every tool call recorded with parameters and duration.
2. **Structured storage** (SQLite): Sessions table (metadata, aggregated metrics), tool_calls table (classified by category), transitions table (tool-to-tool edges for sequential analysis).
3. **Derived metrics**: Computed from the structured data -- adoption rates, error rates, transition probabilities, longitudinal trends.

### 3.3 Tool Taxonomy

We classify the 17 MCP tools into five categories based on their role in the debugging workflow:

| Category | Tools | Purpose |
|----------|-------|---------|
| Execution | `test`, `run`, `build`, `compile` | Verify hypotheses by running code |
| Diagnosis | `check`, `emit-ir`, `emit-mir`, `explain` | Inspect compiler internals at specific layers |
| Documentation | `docs/search`, `docs/get`, `docs/list`, `docs/resolve` | Research APIs and language features |
| Maintenance | `format`, `lint`, `cache/invalidate` | Code quality and cache management |
| Project | `project/coverage`, `project/structure`, `debug`, `profile`, `inspect` | Project-level operations and runtime debugging |

This taxonomy enables analysis at the category level (e.g., "What fraction of calls are diagnostic?") while preserving tool-level granularity.

### 3.4 Metrics Definitions

We define the following metrics for analyzing LLM debugging behavior:

- **Check adoption rate**: `check_calls / (check_calls + test_calls)`. Measures the fraction of validation effort spent on fast type-checking versus full test execution.
- **Diagnosis ratio**: `diagnosis_calls / total_calls`. Measures overall diagnostic engagement.
- **IR preference**: `(emit_ir + emit_mir) / total_calls`. Measures direct IR inspection frequency.
- **Error rate**: `error_calls / total_calls`. Measures how often tool invocations produce errors (compilation failures, test failures, invalid parameters).
- **Test granularity**: Distribution of test calls across single-file, suite-level, and full-suite invocations.
- **debug_layers adoption**: Fraction of test calls that include the `debug_layers=true` parameter.

---

## 4. Methodology

### 4.1 Study Design

This is an observational study of LLM debugging behavior during organic compiler development. Unlike controlled experiments with curated bug sets, we observe the LLM (Claude Opus 4.6 [17]) as it develops the TML compiler and standard library in real-time, encountering and fixing bugs as they arise naturally.

The study covers 30 days of active development (2026-03-05 to 2026-04-04), during which the LLM implemented standard library modules (string operations, iterators, collections, SIMD intrinsics, database bindings), fixed compiler codegen bugs, and maintained test coverage.

### 4.2 Experimental Conditions

The study encompasses two primary conditions, with transitions occurring during the observation period:

**Condition A (Baseline)**: Standard error messages only. The LLM must manually invoke `check`, `emit-ir`, or `emit-mir` to inspect compilation layers. Active from 2026-03-05 to 2026-03-26. Approximately 6 sessions, ~238 calls.

**Condition B (Debug-Layers Default)**: The `--debug-layers` flag is enabled by default on all test calls. On test failure, the output automatically includes HIR, MIR, and LLVM IR for the failing function, along with diagnosis hints. Active from 2026-03-26 onward. 292 sessions, ~3,013 calls.

Additionally, the system prompt (`CLAUDE.md`) was iteratively updated with behavioral rules throughout the study period. Key interventions include:

- **INT-001** (2026-03-26): Added "Use `check` BEFORE `test`" rule with quantitative justification ("check is 10x faster").
- **INT-002** (2026-03-28): Added "ALWAYS use `debug_layers=true` on the FIRST test failure" rule.
- **INT-003** (2026-03-29): Added "NEVER read source files to understand APIs -- use MCP docs tools" rule.

### 4.3 Data Collection

All data was collected from organic development -- the LLM was not given curated bugs or artificial tasks. This design choice prioritizes ecological validity over experimental control. The LLM encountered real bugs during real development, used tools as it chose, and was free to adopt or ignore prompt-based rules.

Data collection is automatic and transparent. The MCP server logs every tool call without LLM awareness. No calls were filtered or excluded from analysis. The total dataset comprises 3,251 tool calls across 300 sessions using 17 distinct tools.

### 4.4 Threats to Internal Validity

The observational design introduces several confounds:

1. **Rule accumulation**: The system prompt grew over time, so later sessions have more behavioral guidance. We address this by tracking longitudinal trends and comparing early vs. late sessions.
2. **Task variation**: Different development tasks (library work vs. codegen debugging) naturally produce different tool distributions. We address this through the case study (Section 5.4).
3. **Learning effects**: The LLM may improve through accumulated experience within and across sessions. Session-level analysis partially controls for this.

---

## 5. Results

### 5.1 Tool Usage Patterns

#### 5.1.1 Overall Distribution

Table 1 presents the complete tool usage distribution across all 3,251 calls.

**Table 1: Tool Usage Distribution (N=3,251)**

| Tool | Calls | Percentage | Error Rate |
|------|-------|------------|------------|
| test | 1,712 | 52.7% | 3.0% |
| check | 566 | 17.4% | 30.4% |
| docs/search | 283 | 8.7% | 0.7% |
| emit-ir | 233 | 7.2% | 2.6% |
| run | 175 | 5.4% | 52.0% |
| docs/list | 93 | 2.9% | 0.0% |
| cache/invalidate | 75 | 2.3% | 21.3% |
| docs/get | 58 | 1.8% | 0.0% |
| build | 17 | 0.5% | 64.7% |
| compile | 15 | 0.5% | 26.7% |
| emit-mir | 10 | 0.3% | 80.0% |
| explain | 4 | 0.1% | 0.0% |
| project/coverage | 3 | 0.1% | 100.0% |
| docs/resolve | 3 | 0.1% | 0.0% |
| debug | 2 | 0.1% | 0.0% |
| profile | 1 | <0.1% | 0.0% |
| inspect | 1 | <0.1% | 0.0% |
| **Total** | **3,251** | **100%** | **11.2%** |

The distribution is heavily skewed: two tools (`test` and `check`) account for 70.1% of all calls. The top five tools account for 91.4%.

#### 5.1.2 Category Breakdown

Table 2 shows the distribution by functional category.

**Table 2: Tool Usage by Category (N=3,251)**

| Category | Calls | Percentage | Description |
|----------|-------|------------|-------------|
| Execution | 1,919 | 59.0% | test, run, build, compile |
| Diagnosis | 813 | 25.0% | check, emit-ir, emit-mir, explain |
| Documentation | 437 | 13.4% | docs/search, docs/get, docs/list, docs/resolve |
| Maintenance | 75 | 2.3% | cache/invalidate |
| Project | 7 | 0.2% | project/coverage, debug, profile, inspect |

Execution dominates at 59.0%, but diagnosis constitutes a substantial 25.0% -- one in four tool calls is diagnostic. Documentation tools represent 13.4%, indicating that the LLM regularly researches APIs before or during implementation. Maintenance and project tools are rarely used.

#### 5.1.3 Test Granularity

The LLM strongly prefers targeted testing over comprehensive validation:

**Table 3: Test Granularity (N=1,712)**

| Granularity | Calls | Percentage |
|-------------|-------|------------|
| Single file (path parameter) | ~1,282 | 74.9% |
| Module suite (suite parameter) | ~403 | 23.5% |
| Full test suite | ~27 | 1.6% |

This preference for fine-grained testing aligns with rapid feedback cycles: single-file tests complete faster than suite runs, enabling tighter edit-test loops.

#### 5.1.4 Most-Tested Modules

Table 4 shows the modules receiving the most test attention, indicating active development areas and persistent debugging hotspots.

**Table 4: Top 10 Test Targets**

| Module | Test Calls | Domain |
|--------|-----------|--------|
| core/str | 114 | String operations |
| std/db | 61 | Database bindings |
| core/iter | 47 | Iterator system |
| core/fmt | 44 | Formatting |
| std/collections | 42 | Data structures |
| core/option | 23 | Option type |
| std/json | 20 | JSON parsing |
| core/num | 19 | Numeric types |
| core/ops | 19 | Operator overloading |
| std/http | 19 | HTTP framework |

The `core/str` module received by far the most testing (114 calls), reflecting its central role in the standard library and the frequency of string-related codegen bugs.

### 5.2 Rule Effectiveness

#### 5.2.1 Check-Before-Test Adoption

The most impactful behavioral intervention was the "Use `check` BEFORE `test`" rule (INT-001), which includes the justification that `check` is 10x faster than `test` for catching type errors. Table 5 shows the adoption trajectory.

**Table 5: Check Adoption Over Time**

| Measurement Point | Date | Check % | Check/Test Ratio | Dataset Size |
|-------------------|------|---------|-----------------|--------------|
| Baseline | 2026-03-25 | 8.8% | 1:6.9 | ~238 calls |
| Post INT-001 | 2026-03-30 | 12.0% | 1:5.0 | 1,321 calls |
| Overall (Apr 4) | 2026-04-04 | 17.5% | 1:3.0 | 3,251 calls |
| Last 50 sessions | 2026-04-04 | 25.3% | 1:1.7 | ~838 calls |

The check/test ratio improved from 1:6.9 (baseline) to 1:1.7 (recent sessions) -- a 4x improvement. Critically, the adoption rate is *accelerating*, not plateauing: the period-over-period increase grew from +36% (baseline to Mar 30) to +46% (Mar 30 to Apr 4 overall) to +45% (overall to recent). This compounding effect suggests the LLM is internalizing the rationale, not merely complying with the rule text.

#### 5.2.2 Debug Layers Adoption

The `debug_layers` parameter, which provides multi-layer IR output on test failure, showed slower but steady adoption:

**Table 6: debug_layers Adoption Over Time**

| Measurement Point | Adoption Rate | Notes |
|-------------------|---------------|-------|
| Baseline | 1.4% (3/216) | Before any rule |
| Post INT-002 | 7.6% (101/1,321) | After explicit rule |
| Overall (Apr 4) | 9.6% (~311/3,251) | Continued growth |
| Last 50 sessions | 11.1% | Most recent data |

Unlike `check` adoption, `debug_layers` growth is linear rather than exponential. We attribute this to higher cognitive friction: using `check` before `test` is a simple sequencing change, while `debug_layers` requires recognizing a test failure and then switching to diagnostic mode rather than immediately editing code.

#### 5.2.3 Structured Output (Unguided Adoption)

For comparison, the `structured=true` parameter on test calls -- which returns machine-parseable JSON instead of text -- achieved 95.7% adoption without any explicit rule. This demonstrates that features providing obvious UX benefit achieve near-universal adoption, while features requiring behavioral change require explicit prompting.

**Table 7: Feature Adoption Comparison**

| Feature | Adoption Rate | Rule Required? | Cognitive Friction |
|---------|---------------|----------------|--------------------|
| structured output | 95.7% | No | Low (better format) |
| check before test | 25.3% (recent) | Yes (INT-001) | Medium (new step) |
| debug_layers | 11.1% (recent) | Yes (INT-002) | High (mode switch) |
| docs before impl | ~15.5% (recent) | Yes (INT-003) | Medium (research step) |

#### 5.2.4 Test Dominance Decline

A structural shift is evident in the declining share of `test` calls over time:

**Table 8: Test Dominance Trend**

| Period | test % | check % | emit-ir % |
|--------|--------|---------|-----------|
| Baseline | ~60% | 8.8% | ~2% |
| Mar 30 | 60.3% | 12.0% | 3.9% |
| Apr 4 overall | 52.7% | 17.5% | 7.2% |
| Last 50 sessions | 44.0% | 25.3% | 9.2% |

Test share declined 16 percentage points from baseline to recent sessions. The freed capacity was absorbed by `check` (+16.5pp) and `emit-ir` (+7.2pp). This represents a shift from "run and see" to "analyze then verify" -- the diagnostic strategy associated with expert human programmers [12].

### 5.3 Error Analysis

#### 5.3.1 Overall Error Rate

The overall error rate across all tools was 11.2% (365/3,251), declining from 13.2% at the Mar 30 measurement point -- a 15% improvement.

**Table 9: Error Rate by Tool Category**

| Category | Error Rate | Interpretation |
|----------|------------|--------------|
| Execution | ~8.2% | Reflects real bugs in code under development |
| Diagnosis | ~26.5% | check errors = type errors being diagnosed (expected) |
| Documentation | ~0.5% | Highly reliable tools |
| Maintenance | 21.3% | cache invalidation failures |

The high error rate for `check` (30.4%) does not indicate tool unreliability. Rather, `check` is used *specifically* to find type errors -- errors are the expected output, not failures. Similarly, `run` errors (52.0%) reflect runtime crashes in code under development.

The very low error rate for documentation tools (0.0-0.7%) confirms their reliability and suggests they should be used more frequently as a pre-implementation step.

#### 5.3.2 Error Rate Trend

The declining error rate suggests genuine behavioral improvement:

**Table 10: Error Rate Over Time**

| Period | Error Rate | Change |
|--------|------------|--------|
| Baseline | ~15% (est.) | -- |
| Mar 30 | 13.2% | -1.8pp |
| Apr 4 | 11.2% | -2.0pp |

At the observed rate of improvement (-1.8-2.0pp per 5-day period), the error rate may approach 9% within the next 10 days. Contributing factors include: (1) check-first filtering catches errors before expensive test runs, (2) proactive documentation research reduces type errors, (3) accumulated prompt rules provide more guidance per session.

#### 5.3.3 Tool-Specific Error Analysis

Two tools exhibit notably high error rates that warrant discussion:

- **emit-mir** (80.0%, 8/10 errors): The MIR printer has limited coverage for certain IR constructs. When the LLM requests MIR for functions using unsupported patterns, the tool fails. The small sample (N=10) also inflates this rate.
- **build** (64.7%, 11/17 errors): Build errors reflect compilation failures during iterative development, often occurring when the LLM attempts to compile partially-implemented modules.

### 5.4 Case Study: SIMD Library Session

The IA (Instruction Architecture) SIMD library session on 2026-04-03 provides a detailed view of mature LLM debugging behavior in a codegen-heavy context. This session involved implementing SIMD intrinsics (`F64x2`, `I32x4`) and produced frequent codegen bugs requiring IR-level diagnosis.

#### 5.4.1 Session Characteristics

Two representative sessions from this work:

**Table 11: SIMD Session Tool Distribution**

| Session | Total | check | emit-ir | test | docs | Pattern |
|---------|-------|-------|---------|------|------|---------|
| Session A | 59 | 17 (29%) | 16 (27%) | 11 (19%) | 12 (20%) | Balanced diagnostic |
| Session B | 63 | 33 (52%) | 5 (8%) | 16 (25%) | 10 (16%) | Check-dominant |
| Typical | ~10.9 | ~1.9 | ~0.8 | ~5.7 | ~1.5 | Test-dominant |

These sessions show qualitatively different behavior from the overall distribution. Session B achieved 52% check usage -- the highest observed -- demonstrating that the check-first pattern is achievable in complex sessions when the task requires it.

#### 5.4.2 Debugging Workflow Pattern

A recurring pattern emerged across multiple bugs in the SIMD session:

```
check -> emit-ir(function="...") -> [fix compiler C++]
      -> cache_invalidate -> check -> test
```

This six-step pattern appeared in three separate bugs within a single session, suggesting pattern internalization. The LLM began with fast type-checking, escalated to IR inspection when needed, applied the fix, invalidated the cache, re-validated with `check`, and only then ran the full test.

**Example: F64x2.get() extractelement type mismatch**

The compiler emitted `extractelement <2 x double> %vec, i32 0` when the SIMD index type should have been `i64`. The diagnostic sequence was:

1. `check` -- caught the type inconsistency
2. `emit-ir(function="test_f64x2_get")` -- revealed `i32` vs expected `i64`
3. Fix applied to compiler C++ code
4. `cache_invalidate` -- cleared stale compilation cache
5. `check` -- verified fix compiled
6. `test(path="ia_f64x2.test.tml")` -- confirmed runtime behavior

This structured workflow is the diagnostic pattern that `--debug-layers` is designed to shortcut: steps 1-2 could be replaced by a single `test` call with `debug_layers=true`.

#### 5.4.3 debug_layers Impact

When a closure codegen bug produced incorrect runtime behavior (not a type error), the LLM used `test(path="ia_closure.test.tml", debug_layers=true)`. The multi-layer output showed that MIR correctly captured the closure variable, but LLVM IR generated a stale stack copy instead of the live heap reference. This immediately identified the bug as a MIR-to-LLVM codegen issue, directing the fix to `mir_codegen.cpp` rather than the type checker or MIR builder.

Without `debug_layers`, the LLM would have investigated the wrong compilation layer first, requiring an estimated 3-5 additional diagnostic calls.

### 5.5 Latency and Development Velocity

Tool latency has a measurable impact on tool selection:

**Table 12: Tool Latency Tiers**

| Tier | Latency | Tools | Usage Share |
|------|---------|-------|-------------|
| Fast | 40-1,000 ms | cache/invalidate, docs/*, check | 24.4% |
| Medium | 3-5 sec | emit-ir, emit-mir, run | 12.9% |
| Slow | ~37 sec | test | 52.7% |

The dominant tool (`test`) is also the slowest at approximately 37 seconds per invocation. With test calls accounting for 52.7% of all invocations, test latency is the primary development bottleneck. A projected in-process JIT execution mode would reduce test latency from 37 seconds to approximately 2 seconds (18.5x speedup), reducing the edit-test iteration cycle from 42 seconds to 7 seconds.

### 5.6 Tool Transition Patterns

Sequential analysis of tool calls reveals characteristic workflows:

**Table 13: Tool Transition Patterns**

| Transition | Observed Count | Interpretation |
|------------|---------------|----------------|
| test -> test | ~480 | Retry loop (dominant pattern) |
| check -> test | ~150 | Research-first validation |
| docs/* -> implementation | ~130 | Reference-first development |
| test -> emit-ir | ~8 | IR exploration after failure |
| test -> check | ~12 | Fallback to type-checking |

The `test -> test` self-loop dominates (approximately 480 occurrences), representing the "run and see" pattern where the LLM edits code and reruns the test without intermediate diagnosis. The `check -> test` transition (approximately 150 occurrences) represents the promoted "research-first" pattern. The `docs -> implementation` transition (approximately 130 occurrences) shows proactive API research.

---

## 6. Discussion

### 6.1 Implications for Tool Design

Our findings suggest several principles for designing MCP tools for LLM consumers:

**Default-on diagnostics outperform opt-in.** The contrast between `structured=true` adoption (95.7%, no rule needed) and `debug_layers` adoption (11.1%, explicit rule required) demonstrates that LLMs adopt features that are on by default but rarely opt into features that require explicit activation. Tool designers should make diagnostic output the default on failure, not an optional parameter.

**Latency drives tool selection.** The LLM's strong preference for `test` (52.7%) over `check` (17.4%) persists despite `check` being 10x faster and explicitly promoted. We attribute this to the LLM's preference for *definitive* feedback (test passes or fails) over *partial* feedback (type-checks but may still crash at runtime). Reducing test latency via JIT execution would address this preference directly.

**Fine-grained tools are preferred.** The 74.9% single-file test rate suggests LLMs strongly prefer targeted over comprehensive feedback. Tool designers should optimize for fine-grained invocation (single function, single file) rather than batch operations.

**Documentation tools are underutilized but highly reliable.** With error rates of 0.0-0.7%, documentation tools are the most reliable in the toolkit, yet they represent only 13.4% of calls. Better integration (e.g., auto-suggesting relevant docs when a type error occurs) could increase adoption.

### 6.2 Implications for Prompt Engineering

Our longitudinal data provides evidence for several prompt engineering principles:

**Quantitative justification accelerates adoption.** The check-before-test rule included "check is 10x faster than test," providing a concrete reason. This rule produced compounding adoption (8.8% -> 25.3% over 10 days). Rules without quantitative justification (e.g., debug_layers) produced slower, linear growth.

**Rules produce compounding, not transient, behavioral change.** The check adoption curve is *accelerating* (period-over-period growth of +36%, +46%, +45%), not plateauing. This suggests the LLM internalizes the rationale and applies it more consistently with experience, rather than merely complying when the rule is salient.

**Cognitive friction determines adoption ceiling.** Features requiring simple sequencing changes (check before test) achieve higher adoption than features requiring mode switches (debug_layers on failure). Prompt engineers should minimize the cognitive steps between rule recognition and rule execution.

**New tool mentions produce rapid but shallow adoption.** Five new tools added to the system prompt (`build`, `debug`, `profile`, `inspect`, `docs_resolve`) appeared in logs within 1-2 sessions, but at very low rates (1-16 calls total). Initial tool awareness is high; sustained usage requires worked examples and clear triggers.

### 6.3 The Shift from Trial-and-Error to Diagnostic Reasoning

The most significant finding is the structural shift in debugging strategy over the observation period. The LLM's behavior evolved from a test-dominated "run and see" pattern (60% test, 8.8% check) toward a diagnostic "analyze then verify" pattern (44% test, 25.3% check, 9.2% emit-ir in recent sessions).

This shift mirrors the novice-to-expert trajectory observed in human debugging studies [12, 13]: novice programmers default to trial-and-error, while experts use systematic diagnosis. The LLM's trajectory suggests that prompt engineering and domain experience can drive this transition, though it occurs over days rather than the months or years typical of human skill development.

The SIMD case study (Section 5.4) demonstrates the endpoint of this trajectory: sessions where `check` exceeds `test` in frequency, and the LLM follows structured multi-tool diagnostic workflows. Whether this behavior persists across different task domains remains an open question.

### 6.4 Limitations

This study has several important limitations:

1. **Single project**: All data comes from one compiler project (TML). Tool usage patterns may differ for web development, embedded systems, or other domains.
2. **Single LLM**: All data comes from Claude Opus 4.6 [17]. Other models (GPT-4 [18], Gemini [19], open-source models) may exhibit different debugging behaviors.
3. **Organic data**: Without controlled experiments, we cannot establish causal relationships between interventions and behavioral changes. The observed trends may be confounded by task variation, learning effects, or prompt accumulation.
4. **Observer effect**: The LLM is aware that tools exist and that the system prompt promotes certain usage patterns. This awareness may influence behavior independently of the tools' intrinsic utility.
5. **Token estimation**: We do not have exact token counts for tool responses, limiting our ability to analyze information density per call.

---

## 7. Threats to Validity

### 7.1 Internal Validity

**Prompt accumulation**: The system prompt grew from approximately 2,000 to 8,000 words over the study period. Later sessions have more behavioral guidance, confounding comparisons between early and late periods. We mitigate this by tracking adoption rates relative to intervention dates rather than absolute time.

**Task confounding**: Different development tasks naturally produce different tool distributions. Codegen debugging sessions (e.g., SIMD work) produce more `emit-ir` calls, while library development sessions produce more `test` calls. The case study approach (Section 5.4) partially controls for this by analyzing within-task patterns.

**Session independence**: Sessions within the same conversation thread share context. A successful debugging pattern in one session may carry over to subsequent sessions, inflating adoption metrics. We mitigate this by computing metrics at the session level and reporting rolling averages.

### 7.2 External Validity

**Single developer**: All sessions involve one developer's workflow. Other developers may use different prompting strategies, task decompositions, or tool preferences.

**Single language**: TML's type system, compilation pipeline, and MCP tool set are specific to this project. Results may not generalize to languages with different error models (e.g., dynamically typed languages) or tool ecosystems.

**Single LLM family**: Claude models may have inherent preferences for certain tool patterns (e.g., preferring structured JSON) that do not transfer to other model families.

### 7.3 Construct Validity

**Error rate interpretation**: A high "error rate" for `check` (30.4%) reflects the tool being used as designed (finding type errors), not tool failure. Our metrics do not distinguish between expected errors (type errors found by check) and unexpected errors (tool crashes).

**Adoption rate as proxy for effectiveness**: We measure adoption (how often a tool is used) but not effectiveness (whether using the tool leads to faster bug resolution). Higher adoption does not necessarily mean better debugging outcomes.

---

## 8. Related Work

### 8.1 LLM Code Generation and Debugging

Chen et al. [1] introduced Codex and HumanEval, establishing benchmarks for LLM code generation. Subsequent work has improved pass rates through chain-of-thought prompting [8], self-repair [9], and iterative refinement [14]. Our work differs by studying the debugging *process* rather than the final output quality.

Jimenez et al. [15] introduced SWE-bench, a benchmark of real-world GitHub issues requiring multi-file edits. While SWE-bench studies include debugging, the focus is on end-to-end resolution rather than tool usage patterns. Our MCP instrumentation provides finer-grained behavioral data.

### 8.2 LLM Tool Use

Schick et al. [6] demonstrated that LLMs can learn to use tools through few-shot prompting (Toolformer). Qin et al. [7] proposed ToolLLM with a benchmark for complex tool use. These works focus on *capability* (can the LLM use the tool correctly?) rather than *behavior* (how does the LLM choose among tools?). Our study addresses the latter question.

Patil et al. [20] studied tool selection in LLM agents, finding that models tend to over-rely on familiar tools. Our finding that `test` dominates despite cheaper alternatives aligns with this observation.

### 8.3 Automated Program Repair

Le Goues et al. [10] and Monperrus [11] surveyed automated program repair, establishing the generate-and-validate paradigm. Our observation that LLMs follow a similar pattern (edit-test-retry) suggests that LLM debugging shares structural similarities with classical APR, though with the addition of diagnostic tool use.

### 8.4 Human Debugging Behavior

Katz and Anderson [12] studied expert vs. novice debugging strategies, finding that experts use systematic hypothesis testing while novices use trial-and-error. Our finding that LLM behavior shifts from test-dominated to diagnosis-heavy over time parallels this novice-to-expert trajectory.

Ko and Myers [13] identified that debugging difficulty correlates with the distance between symptom and cause. The multi-layer debug output (`--debug-layers`) is designed to reduce this distance by showing all compilation layers simultaneously.

---

## 9. Conclusion

### 9.1 Summary of Findings

This paper presented the first empirical study of LLM debugging tool usage in production compiler development. From 3,251 tool calls across 300 sessions, we identified five key findings:

1. **LLMs are overwhelmingly test-centric** (52.7% of calls), preferring definitive pass/fail feedback over diagnostic analysis. This mirrors the trial-and-error pattern observed in novice human programmers.

2. **Prompt-based interventions produce measurable, compounding behavioral change.** The check-before-test rule drove adoption from 8.8% to 25.3% over 10 days, with an accelerating (not plateauing) trajectory.

3. **Feature adoption correlates inversely with cognitive friction.** Default-on features (structured output) achieve 95.7% adoption. Opt-in features requiring behavioral change (debug_layers) reach only 11.1% despite explicit rules.

4. **A structural shift from trial-and-error to diagnostic reasoning is underway**, with test share declining from 60% to 44% as check and emit-ir usage increase. Advanced sessions show check exceeding test in frequency.

5. **Tool latency is the primary development bottleneck.** Test execution at 37 seconds per call dominates computation time. JIT execution (projected 2-second latency) would transform the development velocity.

### 9.2 Recommendations

For **MCP tool designers**: Make diagnostic output default-on for failure cases. Optimize for fine-grained invocation. Reduce latency as the highest-leverage improvement.

For **prompt engineers**: Include quantitative justification in behavioral rules. Minimize cognitive friction between rule recognition and execution. Expect compounding adoption over days, not immediate compliance.

For **researchers**: Instrument MCP servers for longitudinal studies. Compare tool usage across LLM families, programming domains, and developer experience levels.

### 9.3 Future Work

This study opens several directions for future research:

1. **Controlled experiments**: Curating a bug set stratified by compilation layer and measuring resolution time under different diagnostic conditions (baseline, debug-layers, auto-suggest).
2. **Cross-model comparison**: Replicating the study with GPT-4, Gemini, and open-source models to identify model-specific debugging preferences.
3. **Multi-project study**: Extending instrumentation to web development, embedded systems, and DevOps workflows to test generalizability.
4. **Adaptive prompting**: Using real-time tool usage data to dynamically adjust system prompts, reinforcing effective patterns and correcting ineffective ones.
5. **JIT impact measurement**: Quantifying how reduced test latency (via JIT execution) changes the tool distribution and debugging strategy.

[To be updated with additional data as the study continues.]

---

## References

[1] M. Chen, J. Tworek, H. Jun, et al., "Evaluating Large Language Models Trained on Code," arXiv:2107.03374, 2021.

[2] J. Yang, C. E. Jimenez, A. Wettig, et al., "SWE-agent: Agent-Computer Interfaces Enable Automated Software Engineering," arXiv:2405.15793, 2024.

[3] Q. Zhang, C. Fang, Y. Xie, et al., "A Survey on Large Language Models for Software Engineering," arXiv:2312.15223, 2023.

[4] Anthropic, "Model Context Protocol Specification," https://modelcontextprotocol.io, 2024.

[5] A. Ferreira, "TML: A Systems Programming Language for LLM Code Generation," HiveLLM Project, 2025.

[6] T. Schick, J. Dwivedi-Yu, R. Dessi, et al., "Toolformer: Language Models Can Teach Themselves to Use Tools," arXiv:2302.04761, 2023.

[7] Y. Qin, S. Liang, Y. Ye, et al., "ToolLLM: Facilitating Large Language Models to Master 16000+ Real-world APIs," arXiv:2307.16789, 2023.

[8] J. Wei, X. Wang, D. Schuurmans, et al., "Chain-of-Thought Prompting Elicits Reasoning in Large Language Models," NeurIPS, 2022.

[9] A. Olausson, J. P. Inala, C. Wang, et al., "Is Self-Repair a Silver Bullet for Code Generation?," ICLR, 2024.

[10] C. Le Goues, M. Pradel, A. Roychoudhury, "Automated Program Repair," Communications of the ACM, vol. 62, no. 12, pp. 56-65, 2019.

[11] M. Monperrus, "Automatic Software Repair: A Bibliography," ACM Computing Surveys, vol. 51, no. 1, 2018.

[12] I. R. Katz, J. R. Anderson, "Debugging: An Analysis of Bug-Location Strategies," Human-Computer Interaction, vol. 3, no. 4, pp. 351-399, 1987.

[13] A. J. Ko, B. A. Myers, "A Framework and Methodology for Studying the Causes of Software Errors in Programming Systems," Journal of Visual Languages and Computing, vol. 16, pp. 41-84, 2005.

[14] S. Wang, Z. Li, H. Qian, et al., "Large Language Models for Software Engineering: A Systematic Literature Review," ACM Transactions on Software Engineering and Methodology, 2024.

[15] C. E. Jimenez, J. Yang, A. Wettig, et al., "SWE-bench: Can Language Models Resolve Real-World GitHub Issues?," ICLR, 2024.

[16] M. Chen et al., "HumanEval: Hand-Written Evaluation Set for Code Generation," OpenAI, 2021.

[17] Anthropic, "Claude 4 Model Family Technical Report," 2026.

[18] OpenAI, "GPT-4 Technical Report," arXiv:2303.08774, 2023.

[19] Google DeepMind, "Gemini: A Family of Highly Capable Multimodal Models," arXiv:2312.11805, 2023.

[20] S. Patil, T. Zhang, X. Wang, J. Gonzalez, "Gorilla: Large Language Model Connected with Massive APIs," arXiv:2305.15334, 2023.
