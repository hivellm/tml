# Full Analysis — MCP Call Log Data (3241 Calls, 298 Sessions)

**Last updated**: 2026-04-04
**Original analysis**: 2026-03-30 (1321 calls, 129 sessions)
**Updated analysis**: 2026-04-04 (3241 calls, 298 sessions, +145% growth)
**Data period**: 2026-03-25 to 2026-04-04 (10 days)
**Log file**: `mcp-call-log.jsonl`
**LLM Model**: Claude Opus 4.6 / Claude Sonnet 4.6
**Platform**: TML Compiler with MCP Server

## 1. Dataset Overview

| Metric | Previous (Mar 30) | Current (Apr 4) | Change |
|--------|-------------------|-----------------|--------|
| Total tool calls | 1,321 | 3,241 | +145% |
| Total sessions | 129 | 298 | +131% |
| Baseline sessions | 6 | 6 | — |
| Debug-layers sessions | 123 | 292 | +137% |
| Unique tools used | 12 | 17 | +5 new tools |
| Date range | 5 days | 10 days | — |
| Total computation time | 8.2 hours | ~20 hours (est.) | — |
| Error rate | 13.2% (175/1321) | 11.2% (363/3241) | -15% |

## 2. Tool Frequency Distribution

| Tool | Mar 30 Calls | Mar 30 % | Apr 4 Calls | Apr 4 % | Trend |
|------|-------------|----------|-------------|---------|-------|
| test | 797 | 60.3% | ~1,708 | 52.7% | ↓ less test-centric |
| check | 159 | 12.0% | ~567 | 17.5% | ↑ +46% adoption |
| run | 105 | 7.9% | ~172 | 5.3% | ↓ |
| docs_search | 98 | 7.4% | ~282 | 8.7% | ↑ |
| emit-ir | 52 | 3.9% | ~233 | 7.2% | ↑ +85% (major increase) |
| docs_list | 42 | 3.2% | ~94 | 2.9% | ~ |
| cache_invalidate | 31 | 2.3% | ~75 | 2.3% | = |
| docs_get | 28 | 2.1% | ~58 | 1.8% | ~ |
| emit-mir | 5 | 0.4% | ~10 | 0.3% | ~ |
| compile | 2 | 0.2% | ~6 | 0.2% | ~ |
| build | — | — | ~16 | 0.5% | new |
| docs_resolve | — | — | ~3 | 0.1% | new |
| debug | — | — | ~2 | 0.1% | new |
| explain | 1 | 0.1% | ~3 | 0.1% | ~ |
| profile | — | — | ~1 | 0.0% | new |
| inspect | — | — | ~1 | 0.0% | new |
| project_coverage | 1 | 0.1% | ~3 | 0.1% | ~ |

**Key findings (Apr 4)**:
- `test` declining from 60.3% → 52.7%, with 44.0% in the last 50 sessions — strategy diversification underway
- `check` adoption accelerating: 8.8% (baseline) → 12.0% (Mar 30) → 17.5% (overall) → 25.3% (last 50 sessions)
- `emit-ir` nearly doubled from 3.9% to 7.2% — strongest growth of any diagnostic tool
- `docs_*` tools combined: 13.5% (from 12.7% at Mar 30, ~10% at baseline)
- 5 new tools now in use: `build`, `debug`, `profile`, `inspect`, `docs_resolve`
- Execution tools (test + run + compile): 58.2% of all calls (down from 73.4%)

## 3. Error Rates and Reliability

| Tool | Mar 30 Rate | Apr 4 Rate (est.) | Trend |
|------|-------------|-------------------|-------|
| cache_invalidate | 19.4% | ~18% | ~ |
| run | 17.1% | ~16% | ↓ |
| test | 15.9% | ~14.5% | ↓ improving |
| check | 7.5% | ~6.8% | ↓ |
| docs/* | 3.0% | ~2.8% | ↓ |
| emit-ir | 3.8% | ~3.5% | ↓ |

**Overall error rate**: 363/3241 (11.2%) — down from 13.2% at Mar 30 (-15% improvement)

**Key findings (Apr 4)**:
- Overall error rate declining: 13.2% → 11.2% (-15%)
- Improvement driven by: better check-first filtering, more docs research, accumulated CLAUDE.md rules
- High test error rate (est. 14.5%) continues to reflect real development work, not tool failure
- check error rate remains lowest of execution tools, confirming its role as a pre-filter

## 4. Testing and Iteration Patterns

### Most-Tested Targets (Top 10)

| Target | Test Count | Interpretation |
|--------|-----------|-----------------|
| core/str | 21 | Core string library enhancement |
| iter_max_min.test.tml | 21 | Iterator method development |
| core/iter | 20 | Iterator system refinement |
| env/env.test.tml | 19 | Environment variable module |
| simd/neon_basic.test.tml | 17 | SIMD optimization work |
| heap_into_pin.test.tml | 15 | Ownership/pinning mechanics |
| list_phase1.test.tml | 14 | Collection data structure phases |
| hashmap_extras.test.tml | 13 | HashMap extension methods |
| core/option | 12 | Option type enhancements |
| full suite | 11 | Integration checkpoints |

**Finding**: Development is focused on core stdlib (string, iterator, option) with active optimization work (SIMD, pinning, collections).

### Session Size Distribution

| Metric | Value |
|--------|-------|
| Total sessions | 129 |
| Avg calls/session | 10.2 |
| Median calls/session | 4 |
| Max calls/session | 184 |
| Min calls/session | 1 |
| Sessions > 20 calls | 8 |

**Finding**: Highly variable session length indicates mix of quick validation (median 4 calls) and deep debugging (max 184 calls).

### Test Granularity Preference

| Test Type | Count | % |
|-----------|-------|---|
| Path-targeted (single file) | 599 | 74.9% |
| Suite-targeted (module) | 187 | 23.5% |
| Full suite | 11 | 1.4% |

**Finding**: LLMs strongly prefer fine-grained testing (75%) over comprehensive validation (1.4%). This aligns with rapid feedback preference.

## 5. Anti-Pattern Intervention Results (Before/After Analysis)

The project introduced explicit anti-pattern guidance in `CLAUDE.md` with documented rationale. The data shows measurable adoption changes:

### Anti-Pattern 1: Using Test Without Check First

| Metric | Baseline | Mar 30 | Apr 4 Overall | Apr 4 Last 50 | Change (total) |
|--------|----------|--------|---------------|---------------|----------------|
| check usage | 8.8% | 12.0% | 17.5% | 25.3% | +188% |
| check→test sequences | unknown | 67 observed | 150+ observed | — | validated |

**Mechanism**: CLAUDE.md documents: "Use check BEFORE test — check is 10x faster than test and catches type errors without compilation overhead."

**Interpretation**: Adoption is accelerating, not plateauing. The check-first pattern is becoming a stable habit. In the most recent 50 sessions, check is used at a 1:1.7 ratio with test (was 1:5 initially). The check/test ratio improving from 0.15 to 0.58 suggests the LLM is internalizing the pattern.

### Anti-Pattern 2: Reading Source Instead of Using Docs Tools

| Metric | Baseline | Mar 30 | Apr 4 | Change |
|--------|----------|--------|-------|--------|
| docs/* tools usage | ~10% | 12.7% | 13.5% | +35% |
| docs→impl sequences | unknown | 59 observed | 130+ observed | validated |

**Mechanism**: CLAUDE.md documents: "NEVER read a .tml source file just to see what methods/types it exports. Use MCP docs tools — they're faster, cleaner, tracked for research."

**Interpretation**: Steady growth continues. docs usage in the last 50 sessions (15.5%) outpaces the overall rate (13.5%), suggesting ongoing adoption.

### Anti-Pattern 3: Not Using debug_layers on Failure

| Metric | Baseline | Mar 30 | Apr 4 Overall | Apr 4 Last 50 | Change (total) |
|--------|----------|--------|---------------|---------------|----------------|
| debug_layers usage | 1.4% (3/216) | 7.6% (101/1321) | 9.6% (~311/3241) | 11.1% | +586% |

**Mechanism**: CLAUDE.md documents: "ALWAYS use debug_layers=true on the FIRST test failure — get HIR + MIR + LLVM IR for diagnosis."

**Interpretation**: Continued growth from 7.6% to 9.6% (overall), with recent sessions at 11.1%. The upward trend is clear, yet the absolute rate remains low relative to test failure frequency. Evidence:
- The rule is applied when triggered but not yet a reflexive first response
- Recent IA library work showed deliberate debug_layers usage for codegen bugs (see Section 11)

**Implication**: Making debug_layers DEFAULT on test failure (not opt-in) would likely increase adoption to 90%+, matching structured output adoption (95.7%).

### Structured Output Adoption (Unguided)

| Feature | Mar 30 | Apr 4 | Notes |
|---------|--------|-------|-------|
| structured=true on test | 753/797 (94.5%) | ~1,635/1,708 (95.7%) | Near-universal, no rule required |

**Finding**: When features provide obvious UX benefit (parseable JSON), adoption is near-universal and continues to grow.

## 6. Key Findings

### Finding F1: LLMs Are Extremely Test-Centric

**60.3% of all tool calls are test runs.** Test dominates computation time (99% of 8.2 total hours).

**Implication**: Tools must optimize for rapid test iteration. The 37.2 sec average test latency is the primary development bottleneck.

### Finding F2: Check Filter Is Underutilized

**Only 67 out of 797 test calls (8.4%) are preceded by check**, despite the documented 36% adoption increase.

**Interpretation**: The LLM follows the rule when explicit but doesn't consistently self-trigger the pattern. Opportunity: auto-suggest check before test in MCP server response.

### Finding F3: Documentation Research Reduces Errors

**59 docs→impl sequences observed**, showing the LLM proactively researches APIs before implementation.

**Benefit**: Implementations using documented APIs have fewer type errors, reducing iteration count.

### Finding F4: Fine-Grained Testing Dominates

**74.9% of tests target specific files** (path parameter), not full suite (1.4%).

**Finding**: LLMs strongly prefer fast feedback (single-file test) over comprehensive validation.

### Finding F5: Structured Output Is Universal

**94.5% of test calls use structured=true**, despite no explicit requirement.

**Interpretation**: When features provide obvious UX benefit (parseable JSON), adoption is near-universal.

### Finding F6: IR Diagnostics Remain Underutilized (Updated)

**emit-ir (7.2%) + emit-mir (0.3%) = 7.5% of calls** (up from 4.3% at Mar 30).
**debug_layers used in 9.6% of test calls** (up from 7.6%).

**Problem**: LLMs still default to test-edit-test iteration. IR diagnosis is a secondary strategy, not primary.

**Positive trend**: emit-ir nearly doubled (+85%), suggesting the IA library codegen work pushed real IR debugging into practice. The trend is toward IR-first debugging, but adoption is still sub-10%.

**Opportunity**: Make debug_layers DEFAULT on assertion failure to eliminate opt-in friction. With 9.6% current adoption and a clear upward trend, system-level defaults could push this to 90%+.

### Finding F7: Check-First Pattern Is Accelerating

**check adoption: 8.8% (baseline) → 12.0% (Mar 30) → 17.5% (overall Apr 4) → 25.3% (last 50 sessions)**

The most recent 50 sessions show check at a 1:1.7 ratio with test (was 1:5 initially). The prompt reinforcement in CLAUDE.md ("use check BEFORE test") is having a cumulative, accelerating effect — not plateauing.

**Implication**: Explicit rules with justification produce compounding behavioral change over weeks, not just sessions. The LLM internalizes the rationale ("check is 10x faster") and applies it more consistently with experience.

### Finding F8: emit-ir Usage Nearly Doubled

**emit-ir grew from 3.9% to 7.2% (+85%)** — the strongest percentage growth of any non-new tool.

232 total emit-ir calls vs 52 previously. This is correlated with the IA SIMD library work, which produced frequent codegen bugs (F64x2.get(), I32/I64 inference, closure codegen, extractelement type mismatches). When bugs require IR-level analysis, the LLM reaches for emit-ir.

**Pattern**: IR diagnostic adoption is domain-driven. Sessions with compiler/codegen work trigger significantly more emit-ir calls than sessions with pure library work.

### Finding F9: Error Rate Is Declining

**Overall error rate: 13.2% (Mar 30) → 11.2% (Apr 4), -15% improvement**

Likely causes:
1. Better check-first filtering catches errors before expensive test runs
2. More proactive docs research reduces type errors at first attempt
3. Accumulated CLAUDE.md rules provide more guidance per session
4. LLM familiarity with the TML type system improves code quality

**Significance**: Error rate decline in a growing dataset (145% more calls) suggests genuine behavioral improvement, not regression to mean.

### Finding F10: Test Dominance Is Declining

**test: 60.3% (Mar 30) → 52.7% (overall Apr 4) → 44.0% (last 50 sessions)**

The LLM is diversifying its debugging strategy from "retry test" to "diagnose then fix." The decrease in test dominance mirrors the increase in check (25.3%) and emit-ir (9.2%) in recent sessions.

**Mechanism**: The CLAUDE.md rules explicitly state "use check BEFORE test" and "ALWAYS use debug_layers on first failure." Both rules redirect calls away from test and toward diagnostic tools.

**Projection**: If the trend continues, test may fall below 40% of calls within the next 50 sessions, with check potentially reaching 30%+.

### Finding F11: New Tool Adoption

**5 new tools now in use: build, debug, profile, inspect, docs_resolve**

The `build` tool (16 calls) reflects C++ compiler changes requiring rebuild. `debug` (2 calls) and `profile` (1 call) were added to CLAUDE.md rules in late March — very early adoption, but the rules are having effect. `docs_resolve` (3 calls) suggests the LLM is using qualified name resolution before docs lookup.

**Implication**: New tools added to CLAUDE.md rules begin appearing in call logs within 1-2 sessions. The LLM reads and applies tool documentation promptly.

### Finding F12: Session Complexity Growing

**Recent sessions show complex multi-tool workflows:**
- Session 1775315836689: 59 calls — check:17, emit-ir:16, test:11, docs:12
- Session 1775317820413: 63 calls — check:33, test:16, docs:10

These sessions show mature patterns: heavy check + docs research before testing. The check:test ratio in these sessions is 1:0.6 (more checks than tests), the inverse of early patterns.

**Finding**: As the LLM gains experience with the TML compiler's codegen behaviors, sessions become more diagnostic-heavy and less "try and see." This is the desired behavioral trajectory.

## 7. JIT Execution as Infrastructure Outcome

### Current Test Bottleneck

Test execution averages **37.2 seconds** per call, dominated by:
- In-process compilation: ~5 sec
- Object file I/O and linking (LLD): ~32 sec

The linking phase is I/O bound on modern NVMe drives.

### Projected JIT Impact (Phase 0, in development)

With in-process LLVM ORC JIT (no object files, no linking):

```
Current:  Parse → Typecheck → HIR → MIR → LLVM IR → Codegen → Linking → subprocess (37s)
JIT:      Parse → Typecheck → HIR → MIR → LLVM IR → ORC JIT (in-process) (2s)
```

**Expected speedup: 18.5x faster**

### Development Cycle Impact

**Current single iteration** (10 cycles):
```
edit (5s) → test (37s) → see result = 42s per iteration
10 iterations = 420 seconds = 7 minutes
```

**Projected with JIT** (10 cycles):
```
edit (5s) → test (2s) → see result = 7s per iteration
10 iterations = 70 seconds
```

**Net: 6x faster development**

Across 129 sessions analyzed here: **12.4 hours saved** (1/3 of total compute time)

### Phase 0 Implementation Status

The TML compiler is actively implementing ORC JIT with expected completion 2026-04-15:
- Phase 0a: LLVM ORC integration infrastructure
- Phase 0b: C runtime symbol binding
- Phase 0c: JIT execution wrapper
- Phase 0d: Cache invalidation and incremental JIT

**This is a direct outcome of the research finding that test dominates (60.3%) and is extremely slow.**

## 8. Limitations

1. **Single LLM model**: Claude Opus 4.6 only; may not generalize to Sonnet, Haiku, GPT-4o, or open-source models
2. **Single language**: TML has specialized stdlib; results may differ for general-purpose languages
3. **Single task domain**: Compiler development may not reflect web development, DevOps, or security research
4. **Observer effect**: Knowledge of measurement may influence behavior
5. **Confounding variables**: Cannot distinguish LLM preference from tool design prominence

## 9. Recommendations

1. **Default debug_layers on failure** — Change from opt-in to automatic when assertions fail. Expected adoption: 90%+ (matching structured output at 94.5%)

2. **Auto-suggest check before test** — When test is invoked on modified code without prior check, respond with suggestion. Expected adoption increase: 25-30%

3. **Implement JIT execution (Phase 0)** — Already in progress; projected 18.5x speedup on test execution

4. **Track time-to-resolution metrics** — Measure sessions to identify which patterns lead to faster fixes and guide LLM behavior

5. **Implement REPL mode for IR analysis** — Enable interactive JIT + IR introspection during exploratory programming

6. **Reduce emit-ir/emit-mir latency** — Cache LLVM IR in memory; enable streaming output for real-time inspection

7. **Add "Beginner/Advanced" tool modes** — Simplify onboarding without overwhelming with 17 tools at once

8. **Capitalize on new tool adoption momentum** — `debug` and `profile` tools were added to CLAUDE.md and are already appearing in logs. Add worked examples showing when to use them.

## 10. Longitudinal Trends

This section tracks behavioral metrics across four measurement points: Baseline (2026-03-25), Condition B (post INT-001, 2026-03-30), Condition C (overall Apr 4), and Recent (last 50 sessions Apr 4).

### 10.1 Check Adoption Curve

| Measurement Point | Date | check % | Notes |
|-------------------|------|---------|-------|
| Baseline | 2026-03-25 | 8.8% | Pre-CLAUDE.md rules, 238 calls |
| Condition B | 2026-03-30 | 12.0% | After INT-001 rules, 1321 calls |
| Condition C (overall) | 2026-04-04 | 17.5% | All 3241 calls |
| Recent (last 50 sessions) | 2026-04-04 | 25.3% | ~838 calls |

**Rate of change**: +36% (baseline → Mar 30), +46% (Mar 30 → Apr 4 overall), +45% (overall → recent). The adoption rate itself is increasing — a compounding curve, not a plateau. The CLAUDE.md rule is producing sustained behavioral change over a 10-day window.

**check/test ratio over time**:
- Baseline: 1:6.9 (8.8% / 60.3%)
- Mar 30: 1:5.0 (12.0% / 60.3%)
- Apr 4 overall: 1:3.0 (17.5% / 52.7%)
- Recent: 1:1.7 (25.3% / 44.0%)

The ratio is converging toward 1:1, indicating the LLM is approaching a check-before-every-test discipline.

### 10.2 emit-ir Growth Curve

| Measurement Point | emit-ir % | Calls | Notes |
|-------------------|-----------|-------|-------|
| Baseline | ~2% | ~5 | No explicit rule |
| Mar 30 | 3.9% | 52 | Post INT-001 |
| Apr 4 overall | 7.2% | ~233 | After IA library work |
| Recent (last 50 sessions) | 9.2% | ~77 | Post-IA work sessions |

**Interpretation**: emit-ir growth is task-driven, not rule-driven. The IA SIMD library work exposed codegen bugs that required IR analysis, driving adoption. This is a domain-learning effect: as the LLM encounters more codegen bugs, it develops a reflex to reach for IR tools.

### 10.3 Test Dominance Decline

| Measurement Point | test % | Trend |
|-------------------|--------|-------|
| Baseline | ~60% | — |
| Mar 30 | 60.3% | flat |
| Apr 4 overall | 52.7% | ↓ -7.6pp |
| Recent (last 50 sessions) | 44.0% | ↓ -8.7pp |

**Interpretation**: test dominance has declined 16 percentage points from peak to recent sessions. The freed capacity is being absorbed by check (+16.5pp) and emit-ir (+5.3pp). This represents a structural shift in debugging strategy from "run and see" to "analyze then verify."

**Projection**: At the current rate of decline (~8pp per 50 sessions), test could reach ~35% in the next measurement interval. At that point, check + emit-ir combined (~35%) would match test frequency — a historically significant behavioral inflection point.

### 10.4 Error Rate Trend

| Measurement Point | Error Rate | Notes |
|-------------------|------------|-------|
| Baseline | ~15% (est.) | Pre-rules, initial work |
| Mar 30 | 13.2% | After INT-001 |
| Apr 4 | 11.2% | After INT-002 + more sessions |

**Rate of improvement**: -1.8pp per 5-day period. If sustained, error rate could reach 9% within the next 10 days.

### 10.5 debug_layers Adoption Curve

| Measurement Point | debug_layers % | Notes |
|-------------------|----------------|-------|
| Baseline | 1.4% | Pre-rules |
| Mar 30 | 7.6% | After INT-001 rule |
| Apr 4 overall | 9.6% | Continued growth |
| Recent (last 50 sessions) | 11.1% | IA library work influence |

**Interpretation**: Growth is slower than check adoption (9.6% vs 17.5%). Unlike check (where the benefit is immediate: 10x faster), debug_layers requires a mental model shift — seeing a test failure and then switching to IR mode rather than editing code. The rule exists but the cognitive friction of the two-step workflow is higher.

## 11. Case Study: IA SIMD Library Session (2026-04-03)

### 11.1 Overview

The IA (Instruction Architecture) SIMD library work on 2026-04-04 provides a rich case study of mature LLM debugging behavior in a compiler-engineering context. Sessions involved implementing SIMD intrinsics (`F64x2`, `I32x4`, closure SIMD, const cross-module) with frequent codegen bugs.

**Session characteristics:**
- Highly complex: 59–63 tool calls per session
- Codegen-heavy: F64x2.get(), I32/I64 inference, closure codegen, extractelement type mismatch
- IR-diagnosis-driven: emit-ir used 16+ times in a single session

### 11.2 Bugs Discovered and Tool Patterns Used

**Bug: F64x2.get() codegen — extractelement i64 vs i32 mismatch**

The compiler emitted `extractelement <2 x double> %vec, i32 0` when the SIMD index type was `I64` (expected `i64`). The mismatch caused LLVM verification failure.

Tool pattern used:
1. `check` — caught the type error in the .tml source
2. `emit-ir` (function="test_f64x2_get") — revealed `extractelement ... i32 0` vs expected `i64`
3. `cache_invalidate` — after fixing compiler C++ code
4. `check` — verified fix compiled
5. `test` (path="...ia_f64x2.test.tml") — confirmed fix

This exact sequence (check → emit-ir → fix → cache_invalidate → check → test) appeared in 3 separate bugs in the same session, demonstrating pattern internalization.

**Bug: I32/I64 inference for scalar SIMD lane extraction**

When calling `vec.get(0)` on an `F32x4`, the scalar result type was incorrectly inferred as `I64` instead of `F32`. The LLM used:
1. `check` — exposed return type mismatch
2. `emit-ir` (function="test_lane_extract") — confirmed wrong type in LLVM IR
3. `docs_search` (query="SIMD F32x4 get extract") — validated expected API
4. Fix → `test`

**Bug: const cross-module — `base` reserved word collision**

`const base = ...` triggered a parse error because `base` is a reserved keyword in TML's expression grammar (for integer base literals). The LLM used:
1. `check` — exposed parse error immediately
2. `docs_search` (query="reserved keywords TML") — confirmed `base` is reserved
3. Renamed to `base_addr` → `check` → `test`

This required no emit-ir (parse errors are visible without IR).

### 11.3 debug_layers in the IA Session

The IA session provides a concrete case of debug_layers utility. When a closure codegen bug produced wrong output (not a type error, but incorrect runtime behavior), the LLM used:

```
test(path="ia_closure.test.tml", debug_layers=true)
```

The debug_layers output showed the MIR correctly capturing the closure variable, but the LLVM IR generated a stale stack copy instead of the live heap reference. This identified the bug as a MIR→LLVM codegen issue (not a type error or MIR construction error), directing the fix to `mir_codegen.cpp` instead of `thir_mir_builder.cpp`.

**Time saved**: Without debug_layers, the LLM would have investigated the wrong layer (type checker or HIR) before reaching the correct layer (codegen). Estimated 3-5 check/emit-ir calls avoided.

### 11.4 Session-Level Tool Distribution

| Session | Total | check | emit-ir | test | docs | Other | Pattern |
|---------|-------|-------|---------|------|------|-------|---------|
| 1775315836689 | 59 | 17 (29%) | 16 (27%) | 11 (19%) | 12 (20%) | 3 | check≈emit-ir |
| 1775317820413 | 63 | 33 (52%) | 5 (8%) | 16 (25%) | 10 (16%) | 0 | check-dominant |
| Typical (overall) | ~10.9 | ~1.9 | ~0.8 | ~5.7 | ~1.5 | ~1.0 | test-dominant |

The IA sessions are qualitatively different from typical sessions. The check-dominant session (1775317820413) shows the LLM pre-validating each incremental change before running tests — exactly the incremental implementation discipline described in CLAUDE.md.

### 11.5 Implications

1. **Domain expertise drives IR tool adoption**: Codegen work sessions show 5-10x more emit-ir usage than library work sessions. IR tool adoption may be permanently elevated as compiler work continues.

2. **Complex bugs require multi-tool workflows**: The typical 3-4 step pattern (check → emit-ir → fix → test) demonstrates that LLMs can execute structured debugging workflows when the tools provide clear evidence at each step.

3. **Session complexity is task-driven**: The 59-63 call sessions are not inefficiency — they reflect genuinely complex bugs requiring careful diagnosis. The per-bug tool cost (6-8 calls) is reasonable for IR-level debugging.

4. **The check-dominant pattern is achievable**: Session 1775317820413 shows 52% check usage in a complex session. This is the ceiling for what explicit rules + task context can produce without system-level changes (auto-check before test).
