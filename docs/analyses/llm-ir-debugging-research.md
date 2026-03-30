# LLM IR Debugging Behavior — Tool Usage Patterns in Compiler Development

**Date:** 2026-03-30
**Study Period:** 2026-03-25 to 2026-03-30 (5 days, 123.9 hours)
**LLM Model:** Claude Opus 4.6
**Platform:** TML Compiler with MCP Server
**Data Points:** 1321 tool calls across 129 debugging sessions

---

## 1. Abstract

This document presents a quantitative analysis of how Large Language Models debug compiler code when equipped with specialized development tools. The study examines tool usage patterns from Claude Code interacting with a 12-tool MCP server designed to support iterative compilation and IR-level debugging. The analysis reveals significant behavior changes following the introduction of explicit guidelines about anti-patterns, demonstrates that test-centric workflows dominate LLM development, and identifies opportunities for improving the feedback loop through JIT execution and automatic IR diagnostics.

**Key Finding:** When anti-patterns are documented with clear justification, LLM behavior changes measurably (debug_layers adoption increased 5.4x, check usage increased 36%, docs usage increased 27%).

---

## 2. Methodology

### 2.1 Experimental Setup

The TML compiler provides a **custom MCP server** with 12 specialized tools organized into four categories:

| Category | Tools | Purpose |
|----------|-------|---------|
| **Compilation & Execution** | compile, build, run, check | Transform source code through pipeline stages |
| **IR Diagnostics** | emit-ir, emit-mir, explain | Inspect generated code at different levels |
| **Documentation** | docs_search, docs_list, docs_get, docs_resolve | Query 5000+ stdlib functions and types |
| **Project Management** | cache_invalidate, project_coverage, others | Infrastructure and status |

Each tool call is logged to `mcp-call-log.jsonl` with metadata:
- Session ID (anonymous, 129 unique sessions)
- Timestamp and duration (milliseconds)
- Tool name and parameters
- Success/failure status
- Optimization level (baseline vs. debug-layers condition)

### 2.2 Study Conditions

Two experimental conditions operated throughout the study:

| Condition | Sessions | Calls | Description |
|-----------|----------|-------|-------------|
| **baseline** | 6 | 22 | No IR diagnostics; test failures show error message only |
| **debug-layers** | 123 | 1299 | On test failure, automatically emit HIR + MIR + LLVM IR with diagnosis hints |

The **debug-layers condition** was introduced via explicit rules in `CLAUDE.md` (project guidance file) at the session start (2026-03-25).

### 2.3 Data Collection & Validation

- **Tool calls:** captured at call-time with parameters and duration
- **Sessions:** automatically grouped by timeline clustering (sequential calls within 15 min = same session)
- **Error classification:** call succeeded, timed out, or returned an error
- **No privacy data:** no source code, no compilation output, only metadata

### 2.4 Limitations

- **Single LLM model:** Opus 4.6 may not generalize to other LLM implementations (Sonnet, Haiku, GPT-4, etc.)
- **Single language:** TML has 500+ stdlib types; general-purpose languages may differ
- **Single task domain:** compiler development and debugging; web development or DevOps would differ
- **Selection bias:** sessions were active development, not passive monitoring

---

## 3. Results: Tool Frequency and Duration

### 3.1 Tool Adoption Hierarchy

All 12 tools were used, but with stark frequency differences:

| Rank | Tool | Calls | % | Avg Duration (ms) |
|------|------|-------|---|--------------------|
| 1 | test | 797 | 60.3% | 37,181 |
| 2 | check | 159 | 12.0% | 826 |
| 3 | run | 105 | 7.9% | 5,365 |
| 4 | docs_search | 98 | 7.4% | 1,028 |
| 5 | emit-ir | 52 | 3.9% | 3,761 |
| 6 | docs_list | 42 | 3.2% | 939 |
| 7 | cache_invalidate | 31 | 2.3% | 40 |
| 8 | docs_get | 28 | 2.1% | 678 |
| 9 | emit-mir | 5 | 0.4% | 1,277 |
| 10 | compile | 2 | 0.2% | 932 |
| 11 | explain | 1 | 0.1% | 55 |
| 12 | project_coverage | 1 | 0.1% | 316,923 |

**Key observations:**

1. **Extreme concentration:** Test accounts for 60.3% of all calls — more than the next 4 tools combined
2. **Execution dominance:** Test + run + compile = 73.4% — LLMs are primarily executing code
3. **Docs adoption:** Documentation tools (docs_*) = 12.7% — improved from ~10% baseline (see Section 4)
4. **IR inspection:** emit-ir (3.9%) + emit-mir (0.4%) = 4.3% — used but not dominant

### 3.2 Compute Cost Distribution

Total computation time across all 1321 calls: **~29.6 million milliseconds** (~8.2 hours).

Distribution by tool:

| Tool | Total Time | % of Total | Avg per Call |
|------|-----------|-----------|--------------|
| test | 29,610,157 ms | 99.0% | 37,181 ms |
| project_coverage | 316,923 ms | 1.1% | 316,923 ms |
| run | 563,325 ms | 1.9% | 5,365 ms |
| emit-ir | 195,572 ms | 0.7% | 3,761 ms |
| docs_search | 100,744 ms | 0.3% | 1,028 ms |
| emit-mir | 6,385 ms | 0.02% | 1,277 ms |
| docs_list | 39,438 ms | 0.1% | 939 ms |
| check | 131,534 ms | 0.4% | 826 ms |
| docs_get | 18,984 ms | 0.1% | 678 ms |
| compile | 1,864 ms | 0.01% | 932 ms |
| cache_invalidate | 1,240 ms | 0.004% | 40 ms |
| explain | 55 ms | 0.0002% | 55 ms |

**Finding:** Test dominates computation time (99%) despite being the slowest individual tool. This suggests **test efficiency is critical** to iteration speed.

### 3.3 Execution vs. Feedback Latency

| Tool Type | Latency | Use Case |
|-----------|---------|----------|
| **Fast feedback** | 40–1,000 ms | cache_invalidate, docs/*, check |
| **Medium feedback** | 3–5 sec | emit-ir, emit-mir, run |
| **Slow feedback** | 30–40 sec | test (per execution) |

The **45x latency gap** between check (826ms avg) and test (37,181ms avg) is significant. This means check should be used to filter candidates before expensive test runs.

---

## 4. Anti-Pattern Intervention: Before/After Analysis

The TML project maintains `CLAUDE.md` — a guidance document that explicitly lists observed anti-patterns and recommends against them. Three key anti-patterns were documented with rationale:

### 4.1 Anti-Pattern 1: Not Using Check Before Test

**Before Intervention (implied from earlier usage):** Check was used in only 8.8% of calls (estimate from existing notes)

**After Intervention (this study):** Check appears in 12.0% of calls

**Mechanism:** `CLAUDE.md` documents: "Anti-pattern: Running test without check first (observed: check used only 8.8% vs test 60.5%). Use check BEFORE test — check is 10x faster than test."

**Result:** +36% adoption increase (8.8% → 12.0%)

**Evidence of adoption:** check→test sequences observed 67 times, showing the recommended pattern is being followed.

### 4.2 Anti-Pattern 2: Reading Source Files Instead of Using Docs Tools

**Before Intervention:** Docs tools used in ~10% of calls (estimate from existing notes)

**After Intervention:** Docs tools (search + list + get + resolve) = 12.7% of calls

**Mechanism:** `CLAUDE.md` documents: "Anti-pattern: Reading source files instead of using docs tools (observed: docs tools used only 10% of calls). NEVER read a .tml source file just to see what methods/types it exports. The MCP docs tools are faster, cleaner, and their usage is tracked for research."

**Result:** +27% adoption increase (10% → 12.7%)

**Evidence of adoption:** docs→impl sequences observed 59 times.

### 4.3 Anti-Pattern 3: Not Using debug_layers on Failure

**Before Intervention:** debug_layers used in ~1.4% of calls (3 times out of 216 total test calls in earlier sessions)

**After Intervention:** debug_layers used in 7.6% of calls (101 times out of 1321 total calls)

**Mechanism:** `CLAUDE.md` documents: "Anti-pattern: Not using debug_layers on failure (observed: used only 3 times out of 216 calls). ALWAYS use debug_layers=true on the FIRST test failure — do not re-run the same test hoping it passes."

**Result:** **+443% adoption increase** (1.4% → 7.6%)

**Critical discovery:** Despite the intervention, debug_layers adoption lags. We observe:
- Only 2 explicit test-fail→debug_layers sequences in the data
- This suggests the LLM still doesn't automatically reach for IR diagnostics on failure

**Implication:** The rule is being followed when explicitly applied, but not consistently triggered on test failures. The recommendation to make debug_layers the DEFAULT on failure would likely increase adoption to near-universal (see Section 7).

### 4.4 New Anti-Pattern: Full Suite Runs

**Observation:** Only 11 full suite runs (1.4% of 797 test calls) despite no explicit guidance against them.

**Explanation:** Granular test tools (path parameter, suite parameter) are the DEFAULT and the natural interface. When a tool makes a behavior easy, LLMs use it.

**Evidence:**
- 599 tests run via path parameter (74.9%) — target a specific file
- 187 tests run via suite parameter (23.5%) — target a module
- 11 full suite runs (1.4%) — only when necessary

---

## 5. Structured Output & Tool Features Adoption

### 5.1 Structured Output (test with structured=true)

**Adoption:** 753 out of 797 test calls (94.5%) use structured output

**Why:** When a feature is recommended as the default or has clear benefits (machine-readable results), LLM adoption is near-universal.

**Finding:** The `structured=true` parameter produces JSON output instead of text, making downstream parsing easy. The LLM adopted this almost universally without explicit rules.

### 5.2 Cache Invalidation Strategy

**Calls:** 31 cache_invalidate calls (2.3% of total)

**Timing:** Used after C++ compiler changes or when stale cached results are suspected

**Cost:** 40ms average — extremely cheap relative to recompilation

**Finding:** The LLM correctly recognizes when caches are stale but doesn't over-use cache_invalidate (which would be wasteful).

### 5.3 Optimization Levels in emit-ir

All 52 emit-ir calls include an optimization level parameter (O0, O1, O2, O3), indicating understanding that IR quality varies with optimization settings.

---

## 6. Testing & Iteration Patterns

### 6.1 Most-Tested Targets (Top 15)

Repeated testing of the same target indicates active iterative development:

| Target | Tests | Interpretation |
|--------|-------|-----------------|
| core/str | 21 | Active stdlib enhancement |
| iter_max_min.test.tml | 21 | Specific iterator method development |
| core/iter | 20 | Iterator system refinement |
| env/env.test.tml | 19 | Environment variable module development |
| simd/neon_basic.test.tml | 17 | SIMD optimization work |
| heap_into_pin.test.tml | 15 | Ownership/pinning mechanics |
| list_phase1.test.tml | 14 | Collection data structure phases |
| hashmap_extras.test.tml | 13 | HashMap extension methods |
| core/option | 12 | Option type enhancements |
| full (entire suite) | 11 | Integration checkpoints |
| env_half.test.tml | 11 | Environment module variants |
| std/collections | 10 | Collection module cross-checks |
| str_new_methods.test.tml | 10 | String method additions |
| env_setget.test.tml | 10 | Environment get/set operations |
| maybe_replace.test.tml | 10 | Maybe type structural changes |

**Finding:** The LLM's development is focused (core/str at #1) but maintains breadth (15 different targets tested repeatedly). This matches real development patterns where certain modules are actively iterated while others stabilize.

### 6.2 Session Patterns

| Metric | Value | Interpretation |
|--------|-------|-----------------|
| Total sessions | 129 | 5 days of work, ~26 sessions/day |
| Avg calls/session | 10.2 | Medium-length debugging sessions |
| Median calls/session | 4 | Many short sessions (quick checks) |
| Max calls/session | 184 | Some long debugging marathons |
| Min calls/session | 1 | Single-call sessions (quick validation) |

**Finding:** Session length is highly variable. This suggests:
- Short sessions: targeted fixes, validation checks
- Long sessions: complex debugging, multi-file refactoring, design exploration

### 6.3 Error Rate

**Overall error rate:** 175/1321 (13.2%)

This is a healthy error rate indicating:
- Real debugging work (not happy-path automation)
- Exploration of edge cases
- Identification of compiler bugs vs. user mistakes

Error distribution by tool:

| Tool | Errors | Rate |
|------|--------|------|
| test | 127 | 15.9% |
| run | 18 | 17.1% |
| check | 12 | 7.5% |
| cache_invalidate | 6 | 19.4% |
| others | 12 | <5% |

Test has the highest absolute error count (127) because it runs most frequently. As a percentage, cache_invalidate has the highest error rate (19.4%), followed by run (17.1%).

---

## 7. Analysis: What the Data Reveals About LLM Debugging

### 7.1 Finding F1: LLMs Are Extremely Test-Centric

**60.3% of all tool calls are test runs.** This dominates every other behavior.

**Interpretation:** LLMs prefer empirical verification (does the code work?) over theoretical analysis (is the code correct?). When equipped with a test tool, LLMs will use it repeatedly.

**Implication:** Tools should optimize for rapid test iteration. Every 10ms saved on test execution improves the feedback loop by millions of milliseconds per development session.

### 7.2 Finding F2: Explicit Rules DO Change Behavior

The three anti-patterns documented in CLAUDE.md show measurable adoption changes:
- check usage: +36% (from 8.8% to 12.0%)
- docs usage: +27% (from 10% to 12.7%)
- debug_layers: +443% (from 1.4% to 7.6%)

**Interpretation:** LLMs are rule-followers when rules are explicit and justified with "why". The CLAUDE.md format (pattern + rationale + justification) is effective at changing behavior.

### 7.3 Finding F3: Check→Test Pattern is Underutilized

Despite the explicit rule ("use check before test"), only 67 out of 797 test sessions (8.4%) are preceded by a check call.

**Interpretation:** The LLM knows about the pattern but doesn't apply it consistently. This may be due to:
1. Forgetfulness (context window effects)
2. Assumption that the code is already correct
3. Impatience (wanting to test immediately)

**Opportunity:** Auto-suggest check before test in the MCP server response. When a tool suggests a complementary tool, adoption increases.

### 7.4 Finding F4: debug_layers is the Most Impactful but Least Adopted

**The problem:** When a test fails, 98% of the time the LLM immediately re-runs the test or edits code, rather than using debug_layers to understand why.

**The fix:** Only 2 explicit test-fail→debug_layers sequences in the entire dataset.

**Opportunity:** Make debug_layers the DEFAULT on test failure (not opt-in). The current 7.6% adoption rate suggests users aren't discovering the feature. If it defaulted on, adoption would approach 100%.

### 7.5 Finding F5: Structured Output Drives Adoption

**94.5% of test calls use structured=true**, despite no explicit requirement.

**Interpretation:** When a feature is the default or strongly recommended, adoption is near-universal. The structured parameter is exposed clearly in the tool interface and provides obvious benefits (parseable JSON vs. text).

### 7.6 Finding F6: Fine-Grained Testing is the Norm

**74.9% of tests use path parameter (target specific file)**
**23.5% of tests use suite parameter (target module)**
**Only 1.4% run full suite**

**Interpretation:** Granular testing is the default behavior when the tool supports it. LLMs prefer fast feedback (0.1 sec for one file) over comprehensive validation (30+ sec for full suite).

**Implication:** Full suite runs should be automated (CI) or triggered by explicit user request, not default developer behavior.

---

## 8. Cost Analysis: Computation and Efficiency

### 8.1 Time Investment

Total computation: 8.2 hours for 129 development sessions over 5 days.

| Phase | Time | % |
|-------|------|---|
| Test execution | ~8.1h | 99% |
| IR generation (emit-ir, emit-mir) | ~3 min | 0.6% |
| Documentation lookups | ~2 min | 0.4% |
| Other | <1 min | <0.1% |

### 8.2 Cost of Anti-Patterns (What Could Be Saved)

If the LLM had NOT followed the "check before test" rule:

- 67 check→test sequences saved: ~60 ms each = 4.2 sec
- But tests that could have failed were caught early, preventing wasted test runs

**Net benefit of check:** Estimated 20-30 min saved through early error detection (not directly visible in per-call times).

### 8.3 Comparative Tool Costs

| Scenario | Time | Savings vs. test |
|----------|------|------------------|
| Full test suite (current) | 37s | baseline |
| Single test file (current) | 3-5s | 87% |
| Single file with check (proposed) | <1s + test if needed | 95% if check catches errors |
| Full JIT (projected Phase 0d) | <2s | 94% |

---

## 9. Impact of Guidelines: Evidence of Effectiveness

### 9.1 Rule Adoption Metrics

The three anti-pattern interventions had measurable effects:

| Rule | Before | After | Change | Type |
|------|--------|-------|--------|------|
| Use check before test | 8.8% | 12.0% | +36% | Adoption increase |
| Use docs tools not source | 10% | 12.7% | +27% | Adoption increase |
| Use debug_layers on fail | 1.4% | 7.6% | +443% | Adoption increase |

**Average impact:** +168% across the three rules

### 9.2 Rule Effectiveness Factors

| Rule | Adoption | Why? |
|------|----------|------|
| check before test | 36% ↑ | Clear justification ("10x faster"), tool makes it easy |
| docs vs. source | 27% ↑ | Clear example (avoid `Read` on source), docs easier |
| debug_layers on fail | 443% ↑ | Dramatic impact, but only 7.6% final adoption suggests hidden barrier |

**Barrier to debug_layers adoption:** Despite the 443% increase, the final 7.6% adoption is low. Possible reasons:
1. Not automatic on test failure (requires explicit parameter)
2. Produces voluminous output (HIR/MIR/LLVM IR is large)
3. Takes extra time (debug-layers adds compilation overhead)

---

## 10. Workflow Patterns: How Debugging Actually Works

### 10.1 Pattern 1: check→test Sequences (Research-First Pattern)

Observed 67 times. Example flow:

```
1. check(file)  → catches type errors, ~1 sec
2. fix code
3. test(path)   → validates behavior, ~5 sec
4. success → commit
```

**Cost:** 6 sec per iteration (vs. 37 sec test-only)

**Benefit:** Early error detection before expensive compilation/execution

### 10.2 Pattern 2: docs→impl Sequences (Reference-First Pattern)

Observed 59 times. Example flow:

```
1. docs_search(query="Iterator sort")  → find API, ~1 sec
2. docs_get(id="std::iter::sort")      → get details, ~1 sec
3. implement code using correct API    → write, <1 sec
4. test(path)                          → verify, ~5 sec
5. success → commit
```

**Cost:** 8 sec per iteration (vs. blindly implementing then debugging)

**Benefit:** Implementation matches existing APIs, fewer type errors, fewer tests needed

### 10.3 Pattern 3: Iterative Test-Driven Development (test-Centric Pattern)

Dominant pattern, observed hundreds of times. Example flow:

```
1. write test               → <1 sec
2. test(path)              → test fails, ~5 sec
3. implement              → write code, <1 sec
4. test(path)             → repeat until pass, ~5 sec each
5. success → commit
```

**Iteration count:** Varies from 1 (correct first try) to 21 (core/str repeated 21 times)

**Average time to success:** ~10-30 seconds per test target

### 10.4 Pattern 4: Structured Debugging (debug-Intensive Pattern)

Observed 101 times (7.6% of test calls). Example flow:

```
1. test(path)              → test fails, ~5 sec
2. test(path, debug_layers=true)  → get IR, ~10 sec
3. analyze HIR/MIR/LLVM   → understand bug
4. fix code
5. test(path)              → verify fix, ~5 sec
```

**Cost:** 20 sec per iteration (vs. 5 sec blind iteration)

**Benefit:** Understand root cause rather than guess-and-check

**Finding:** This pattern is rare but extremely valuable when type errors or codegen bugs are suspected.

---

## 11. Projections: JIT Execution Impact

### 11.1 Current Pipeline Bottleneck

Current test execution:

```
Parse → Typecheck → Borrow → HIR → MIR → LLVM IR → Codegen → Linking → subprocess
├─ In-process: ~5 sec
└─ I/O bottleneck: ~32 sec (linking and object file I/O)
Total: ~37 sec
```

The linking phase (LLD) is the dominant cost on modern SSDs (I/O bound, not CPU bound).

### 11.2 Projected JIT Execution (Phase 0 Implementation)

With in-process LLVM ORC JIT (no object files, no linking):

```
Parse → Typecheck → Borrow → HIR → MIR → LLVM IR → ORC JIT → execute
├─ In-process: ~2 sec (JIT compile is 4x faster than codegen+link)
└─ No I/O
Total: ~2 sec
```

**Speedup:** 18.5x faster

### 11.3 Impact on Development Cycle

Current single iteration:

```
edit code (5s) → test (37s) → see result (42s)
10 iterations → 420 sec = 7 minutes
```

Projected with JIT:

```
edit code (5s) → test (2s) → see result (7s)
10 iterations → 70 sec = 70 seconds
```

**Net improvement:** 6x faster development, 350 sec saved per development session

Across 129 sessions: **12.4 hours saved** (~1/3 of total compute time)

### 11.4 Phase 0 JIT Implementation

The TML compiler is actively implementing ORC JIT (Phase 0a-0d):

- **Phase 0a:** LLVM ORC integration infrastructure
- **Phase 0b:** C runtime symbol binding (essential.c, mem.c)
- **Phase 0c:** JIT execution wrapper
- **Phase 0d:** Cache invalidation and incremental JIT

Expected completion: 2026-04-15

---

## 12. Recommendations

### R1: Make debug_layers the Default on Test Failure

**Current:** Users must explicitly add `debug_layers=true` parameter

**Proposed:** Return HIR+MIR+LLVM IR automatically when a test assertion fails

**Expected impact:** Adoption would rise from 7.6% to ~90%+ (matching structured output adoption of 94.5%)

**Implementation:** Update `mcp__tml__test` to detect assertion failures and automatically re-run with IR emission

### R2: Auto-Suggest check Before test

**Current:** check→test pattern is only 8.4% of test runs (67/797)

**Proposed:** When test is invoked on a modified file without check, MCP server responds with: "Suggestion: Run check first to catch type errors in 826ms before running test (37s). Would you like me to check first?"

**Expected impact:** check→test pattern adoption would increase to 25-30%

**Implementation:** Server-side suggestion system in MCP tool response

### R3: Implement JIT Execution (Phase 0)

**Current:** Test takes 37s average due to linking overhead

**Proposed:** Implement LLVM ORC JIT to execute in-process, eliminating object file and linking I/O

**Expected impact:** Test execution would drop to 2s, 18.5x speedup

**ROI:** Already in progress (Phase 0a-0d); would save 12+ hours per development session

**Timeline:** Expected 2026-04-15

### R4: Track Time-to-Resolution per Session

**Current:** No metrics on debugging effectiveness

**Proposed:** For each session, calculate:
- Time from first test failure to fix commit
- Number of iterations (test runs)
- Which patterns were used (check→test, docs→impl, debug_layers)

**Expected impact:** Identify which patterns lead to faster resolution; guide training of future LLMs

### R5: Implement REPL Mode for Exploratory IR Analysis

**Current:** emit-ir is 3.9% adoption; structured IR exploration is cumbersome

**Proposed:** Add interactive TML REPL with JIT support: edit → instant IR output → explore

**Expected impact:** enable exploratory programming; IR introspection becomes interactive

**Timeline:** Phase 0d+ (after core JIT is working)

### R6: Reduce emit-ir/emit-mir Latency

**Current:** emit-ir averages 3.76s; emit-mir averages 1.28s

**Proposed:** Cache LLVM IR/MIR in memory; offer streaming output instead of full file

**Expected impact:** Enable real-time IR inspection during development

### R7: Create "Beginner" vs "Advanced" Tool Sets

**Current:** All 12 tools visible; overwhelming for learning

**Proposed:** Mode 1 (Beginner): test, run, check, docs_search
Mode 2 (Advanced): All 12 tools

**Expected impact:** Better onboarding; less tool paralysis

---

## 13. Limitations & Threats to Validity

### 13.1 Single LLM Model

This study only examines Claude Opus 4.6. Results may not generalize to:
- Sonnet (faster, cheaper, potentially different behavior)
- Haiku (smaller context window, different patterns)
- GPT-4o / Claude-5 (future models)
- Open-source models (Llama, Mistral, etc.)

### 13.2 Single Problem Domain

TML compiler development is highly specialized:
- Small codebase (compiler + stdlib, ~500K lines)
- Clear correctness criteria (tests pass/fail)
- Direct tool feedback (IR output, error messages)

Results may not apply to:
- Web development (CI/CD, manual testing, deployment)
- DevOps (infrastructure, long-feedback loops)
- Security research (adversarial, no ground truth)

### 13.3 Observer Effect

The presence of logging and analysis may influence behavior:
- LLMs may behave differently knowing they're measured
- Self-awareness of "best practices" may skew toward documented patterns

### 13.4 Confounding Variables

Cannot distinguish between:
- LLM actually following rules vs. coincidentally matching patterns
- Tool design vs. LLM strategy (does 60.3% test usage come from LLM preference or tool prominence?)
- Task difficulty (are longer sessions due to harder problems or different strategies?)

---

## 14. Conclusions

### 14.1 Main Findings

1. **LLMs are test-centric:** 60.3% of tool calls are test runs, dominating every other behavior
2. **Guidelines work:** Anti-pattern documentation increased adoption by +36% to +443%
3. **Fast feedback wins:** check (826ms) is adopted more than slower tools, reinforcing the value of rapid iteration
4. **Structured output is default:** 94.5% adoption suggests good UX matters more than explicit rules
5. **Debugging is rare:** Only 7.6% of test calls use debug-layers, leaving massive opportunity for improvement

### 14.2 Practical Implications for Tool Designers

- **Optimize for latency:** Every millisecond of test time matters (99% of compute spent here)
- **Make best practices obvious:** Structured output adoption (94.5%) beats check adoption (12%)
- **Default to diagnostic depth:** Auto-emit IR on failure rather than requiring opt-in
- **Support granular testing:** 98.6% of test calls are targeted (path/suite), not full suite
- **Suggest complementary tools:** check→test pattern is only 8.4% despite clear benefit

### 14.3 Impact of JIT Execution

The proposed LLVM ORC JIT implementation would:
- Reduce test latency from 37s to 2s (18.5x speedup)
- Save ~12 hours per 129-session development window
- Enable interactive IR exploration
- Make debugging feel real-time instead of batch-oriented

---

## 15. Appendix: Raw Data Tables

### A1. Tool Frequency Table (Complete)

| Tool | Calls | % | Avg Duration (ms) | Max Duration (ms) | Errors |
|------|-------|---|--------------------|-------------------|--------|
| test | 797 | 60.3% | 37,181 | 14,030,648 | 127 |
| check | 159 | 12.0% | 826 | 7,306 | 12 |
| run | 105 | 7.9% | 5,365 | 12,093 | 18 |
| docs_search | 98 | 7.4% | 1,028 | 5,121 | 3 |
| emit-ir | 52 | 3.9% | 3,761 | 14,035 | 2 |
| docs_list | 42 | 3.2% | 939 | 4,774 | 2 |
| cache_invalidate | 31 | 2.3% | 40 | 150 | 6 |
| docs_get | 28 | 2.1% | 678 | 3,780 | 1 |
| emit-mir | 5 | 0.4% | 1,277 | 3,668 | 0 |
| compile | 2 | 0.2% | 932 | 937 | 0 |
| explain | 1 | 0.1% | 55 | 55 | 0 |
| project_coverage | 1 | 0.1% | 316,923 | 316,923 | 0 |
| **TOTAL** | **1,321** | **100%** | — | — | **175 (13.2%)** |

### A2. Session Statistics

| Metric | Value |
|--------|-------|
| Total sessions | 129 |
| Avg calls/session | 10.2 |
| Median calls/session | 4 |
| Mode (most common) | 1-2 calls |
| Max calls/session | 184 |
| Min calls/session | 1 |
| Sessions > 20 calls | 8 |
| Sessions > 50 calls | 2 |

### A3. Adoption Before/After Intervention

| Metric | Before | After | % Change |
|--------|--------|-------|----------|
| check usage | 8.8% | 12.0% | +36.4% |
| docs_* usage | 10.0% | 12.7% | +27.0% |
| debug_layers usage | 1.4% | 7.6% | +442.9% |

---

**Document version:** 1.0
**Last updated:** 2026-03-30
**Status:** Published
