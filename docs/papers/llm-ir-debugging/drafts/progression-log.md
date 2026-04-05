# Research Progression Log

Tracks interventions, measurements, and their effects over time.
Each entry records: what changed, when, baseline metric, and follow-up measurement.

## Format

Each entry:
- **Date**: When the intervention was applied
- **Intervention**: What was changed
- **Baseline**: The metric before the change
- **Measurement**: The metric after (with date of measurement)
- **Effect**: Quantified change
- **Status**: Active / Superseded / No effect

---

## Interventions

### INT-001: CLAUDE.md Anti-Pattern Rules (2026-03-25)

**Intervention:** Added explicit rules to CLAUDE.md documenting anti-patterns:
- "NEVER read source files to understand APIs — use docs tools"
- "Use check BEFORE test"
- "ALWAYS use debug_layers=true on first test failure"

**Baseline (2026-03-25, 238 calls, 60 sessions):**
| Metric | Value |
|--------|-------|
| check usage | 8.8% |
| docs/* usage | ~10% |
| debug_layers usage | 1.4% (3/216) |
| full suite runs | not tracked |

**Measurement (2026-03-30, 1321 calls, 129 sessions):**
| Metric | Value | Change |
|--------|-------|--------|
| check usage | 12.0% | +36% |
| docs/* usage | 12.7% | +27% |
| debug_layers usage | 7.6% (101/1321) | +443% |
| full suite runs | 1.4% | — |

**Effect:** Moderate positive. Rules changed behavior but docs adoption remains low (84.5% of sessions still zero docs calls).

**Status:** Active

---

### INT-002: Docs Hints in Error Output (2026-03-30)

**Intervention:** Modified MCP server (`compiler/src/mcp/mcp_tools.cpp`) to embed docs
tool suggestions directly in `check` and `test` error output:

1. **Error parsing** — `extract_hint_candidates()` finds single-quoted tokens and qualified
   module paths (containing `::`) in compiler error text, then emits
   `docs_search(query="TypeName")` suggestions (up to 5).

2. **Known-type inline hints** — `build_docs_hints()` detects `Maybe`, `Outcome`, and
   `Iterator` mentions and appends their variant signatures plus the exact
   `docs_get`/`docs_list` call to retrieve full API docs.

3. **Successful check imports** — When `check` succeeds, `extract_imports()` parses the
   source file for `use` statements and appends
   `module -> docs_list(module="module")` for every imported module, surfacing
   available APIs at the moment the LLM confirms the file compiles.

4. **Structured test JSON** — `handle_test()` structured path adds a `"docs_hints"` array
   field alongside `failures` and `diagnostics`, so callers using
   `structured=true` can programmatically surface hints.

5. **Non-structured test failure** — `handle_test()` plain-text failure path appends
   the same `--- Docs Hints ---` block as `handle_check()`.

**Baseline (2026-03-30, pre-intervention):**
| Metric | Value |
|--------|-------|
| docs/* usage | 12.7% (168/1321) |
| Sessions with docs | 15.5% (20/129) |
| docs/resolve usage | 0 calls |

**Measurement:** Pending — will measure after next 50+ sessions

**Expected effect:**
- docs/* usage: 12.7% → 20%+ (hypothesis)
- Sessions with docs: 15.5% → 40%+ (hypothesis)
- docs/resolve: 0 → >0 (if mentioned in hints)

**Status:** Active — awaiting measurement

---

### INT-003: JIT Execution Engine (2026-03-30)

**Intervention:** Implemented LLVM ORC JIT engine (Phases 0a-0d):
- `tml run --jit` and `tml script` commands
- Skips object files, linking, and subprocess execution
- In-process execution via LLJIT

**Baseline (2026-03-30):**
| Metric | Value |
|--------|-------|
| test avg duration | 37,181ms |
| run avg duration | 5,365ms |
| test calls (% of total) | 60.3% |

**Measurement:** Pending — JIT not yet used in MCP test tool

**Expected effect:**
- run duration: 5,365ms → <2,000ms (hypothesis)
- May shift test/run ratio if JIT is faster

**Status:** Active — infrastructure complete, not yet integrated into MCP test

---

## Measurement Schedule

| Date | Action | Notes |
|------|--------|-------|
| 2026-03-30 | INT-002 deployed | Docs hints in check/test |
| 2026-04-01 | Measure INT-002 | Need 50+ new sessions |
| 2026-04-05 | Full analysis | Compare all interventions |
| 2026-04-10 | INT-003 measurement | If JIT integrated into MCP |
