# 5. Results

## 5.1 Tool Usage Patterns

### 5.1.1 Overall Distribution

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

### 5.1.2 Category Breakdown

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

### 5.1.3 Test Granularity

The LLM strongly prefers targeted testing over comprehensive validation:

**Table 3: Test Granularity (N=1,712)**

| Granularity | Calls | Percentage |
|-------------|-------|------------|
| Single file (path parameter) | ~1,282 | 74.9% |
| Module suite (suite parameter) | ~403 | 23.5% |
| Full test suite | ~27 | 1.6% |

This preference for fine-grained testing aligns with rapid feedback cycles: single-file tests complete faster than suite runs, enabling tighter edit-test loops.

### 5.1.4 Most-Tested Modules

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

## 5.2 Rule Effectiveness

### 5.2.1 Check-Before-Test Adoption

The most impactful behavioral intervention was the "Use `check` BEFORE `test`" rule (INT-001), which includes the justification that `check` is 10x faster than `test` for catching type errors. Table 5 shows the adoption trajectory.

**Table 5: Check Adoption Over Time**

| Measurement Point | Date | Check % | Check/Test Ratio | Dataset Size |
|-------------------|------|---------|-----------------|--------------|
| Baseline | 2026-03-25 | 8.8% | 1:6.9 | ~238 calls |
| Post INT-001 | 2026-03-30 | 12.0% | 1:5.0 | 1,321 calls |
| Overall (Apr 4) | 2026-04-04 | 17.5% | 1:3.0 | 3,251 calls |
| Last 50 sessions | 2026-04-04 | 25.3% | 1:1.7 | ~838 calls |

The check/test ratio improved from 1:6.9 (baseline) to 1:1.7 (recent sessions) -- a 4x improvement. Critically, the adoption rate is *accelerating*, not plateauing: the period-over-period increase grew from +36% (baseline to Mar 30) to +46% (Mar 30 to Apr 4 overall) to +45% (overall to recent). This compounding effect suggests the LLM is internalizing the rationale, not merely complying with the rule text.

### 5.2.2 Debug Layers Adoption

The `debug_layers` parameter, which provides multi-layer IR output on test failure, showed slower but steady adoption:

**Table 6: debug_layers Adoption Over Time**

| Measurement Point | Adoption Rate | Notes |
|-------------------|---------------|-------|
| Baseline | 1.4% (3/216) | Before any rule |
| Post INT-002 | 7.6% (101/1,321) | After explicit rule |
| Overall (Apr 4) | 9.6% (~311/3,251) | Continued growth |
| Last 50 sessions | 11.1% | Most recent data |

Unlike `check` adoption, `debug_layers` growth is linear rather than exponential. We attribute this to higher cognitive friction: using `check` before `test` is a simple sequencing change, while `debug_layers` requires recognizing a test failure and then switching to diagnostic mode rather than immediately editing code.

### 5.2.3 Structured Output (Unguided Adoption)

For comparison, the `structured=true` parameter on test calls -- which returns machine-parseable JSON instead of text -- achieved 95.7% adoption without any explicit rule. This demonstrates that features providing obvious UX benefit achieve near-universal adoption, while features requiring behavioral change require explicit prompting.

**Table 7: Feature Adoption Comparison**

| Feature | Adoption Rate | Rule Required? | Cognitive Friction |
|---------|---------------|----------------|--------------------|
| structured output | 95.7% | No | Low (better format) |
| check before test | 25.3% (recent) | Yes (INT-001) | Medium (new step) |
| debug_layers | 11.1% (recent) | Yes (INT-002) | High (mode switch) |
| docs before impl | ~15.5% (recent) | Yes (INT-003) | Medium (research step) |

### 5.2.4 Test Dominance Decline

A structural shift is evident in the declining share of `test` calls over time:

**Table 8: Test Dominance Trend**

| Period | test % | check % | emit-ir % |
|--------|--------|---------|-----------|
| Baseline | ~60% | 8.8% | ~2% |
| Mar 30 | 60.3% | 12.0% | 3.9% |
| Apr 4 overall | 52.7% | 17.5% | 7.2% |
| Last 50 sessions | 44.0% | 25.3% | 9.2% |

Test share declined 16 percentage points from baseline to recent sessions. The freed capacity was absorbed by `check` (+16.5pp) and `emit-ir` (+7.2pp). This represents a shift from "run and see" to "analyze then verify" -- the diagnostic strategy associated with expert human programmers [12].

## 5.3 Error Analysis

### 5.3.1 Overall Error Rate

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

### 5.3.2 Error Rate Trend

The declining error rate suggests genuine behavioral improvement:

**Table 10: Error Rate Over Time**

| Period | Error Rate | Change |
|--------|------------|--------|
| Baseline | ~15% (est.) | -- |
| Mar 30 | 13.2% | -1.8pp |
| Apr 4 | 11.2% | -2.0pp |

At the observed rate of improvement (-1.8-2.0pp per 5-day period), the error rate may approach 9% within the next 10 days. Contributing factors include: (1) check-first filtering catches errors before expensive test runs, (2) proactive documentation research reduces type errors, (3) accumulated prompt rules provide more guidance per session.

### 5.3.3 Tool-Specific Error Analysis

Two tools exhibit notably high error rates that warrant discussion:

- **emit-mir** (80.0%, 8/10 errors): The MIR printer has limited coverage for certain IR constructs. When the LLM requests MIR for functions using unsupported patterns, the tool fails. The small sample (N=10) also inflates this rate.
- **build** (64.7%, 11/17 errors): Build errors reflect compilation failures during iterative development, often occurring when the LLM attempts to compile partially-implemented modules.

## 5.4 Case Study: SIMD Library Session

The IA (Instruction Architecture) SIMD library session on 2026-04-03 provides a detailed view of mature LLM debugging behavior in a codegen-heavy context. This session involved implementing SIMD intrinsics (`F64x2`, `I32x4`) and produced frequent codegen bugs requiring IR-level diagnosis.

### 5.4.1 Session Characteristics

Two representative sessions from this work:

**Table 11: SIMD Session Tool Distribution**

| Session | Total | check | emit-ir | test | docs | Pattern |
|---------|-------|-------|---------|------|------|---------|
| Session A | 59 | 17 (29%) | 16 (27%) | 11 (19%) | 12 (20%) | Balanced diagnostic |
| Session B | 63 | 33 (52%) | 5 (8%) | 16 (25%) | 10 (16%) | Check-dominant |
| Typical | ~10.9 | ~1.9 | ~0.8 | ~5.7 | ~1.5 | Test-dominant |

These sessions show qualitatively different behavior from the overall distribution. Session B achieved 52% check usage -- the highest observed -- demonstrating that the check-first pattern is achievable in complex sessions when the task requires it.

### 5.4.2 Debugging Workflow Pattern

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

### 5.4.3 debug_layers Impact

When a closure codegen bug produced incorrect runtime behavior (not a type error), the LLM used `test(path="ia_closure.test.tml", debug_layers=true)`. The multi-layer output showed that MIR correctly captured the closure variable, but LLVM IR generated a stale stack copy instead of the live heap reference. This immediately identified the bug as a MIR-to-LLVM codegen issue, directing the fix to `mir_codegen.cpp` rather than the type checker or MIR builder.

Without `debug_layers`, the LLM would have investigated the wrong compilation layer first, requiring an estimated 3-5 additional diagnostic calls.

## 5.5 Latency and Development Velocity

Tool latency has a measurable impact on tool selection:

**Table 12: Tool Latency Tiers**

| Tier | Latency | Tools | Usage Share |
|------|---------|-------|-------------|
| Fast | 40-1,000 ms | cache/invalidate, docs/*, check | 24.4% |
| Medium | 3-5 sec | emit-ir, emit-mir, run | 12.9% |
| Slow | ~37 sec | test | 52.7% |

The dominant tool (`test`) is also the slowest at approximately 37 seconds per invocation. With test calls accounting for 52.7% of all invocations, test latency is the primary development bottleneck. A projected in-process JIT execution mode would reduce test latency from 37 seconds to approximately 2 seconds (18.5x speedup), reducing the edit-test iteration cycle from 42 seconds to 7 seconds.

## 5.6 Tool Transition Patterns

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
