# 06 — Frozen Self-Hosting Compiler

## Overview

A 45K-LOC self-hosting compiler written in TML exists, but is frozen until phases 30–33 are unfrozen. It currently appears in the build, test discovery, and documentation, despite being neither shipped nor actively maintained.

---

### F-020 — 45K LOC of frozen self-hosting compiler carried as dead weight

**Impact: Medium**  
**Confidence: High**

**Measured code:**  
`compiler-tml/src/` = 182 files / 45,008 LOC

**What is it?**

A complete reimplementation of the TML compiler written in TML itself. This was the goal of the self-hosting project (phases 30–33 unfreezing is on the roadmap) but is currently frozen.

**Current status:**

- **Not shipped** — the active compiler is the C++ one (`compiler/src/`)
- **Not maintained** — project memory records "compiler-tml is frozen; pure-TML repros only, never anchor gates on cc_driver"
- **Appears in visible surfaces:**
  - `docs.json` (API documentation discovery)
  - Test discovery (test runner finds tests in `compiler-tml/tests/`)
  - ARCHITECTURE-MAP (appears as a documented component)
  - Default build surface

**Historical context:**

Its C frontend (`cc/` in the compiler-tml tree) generated most of the phase24 heap-corruption grind.

**Why this matters:**

45K LOC of dead code clutters:
- Documentation and API search results
- Test discovery (slows down test enumeration)
- Mental model clarity (readers see "two compilers" when only one is active)
- Maintenance assumptions (someone might fix a bug in compiler-tml expecting it to matter)

---

### Proposed action (Phase D)

**Quarantine `compiler-tml/` from default build/test/docs discovery** until phases 30–33 unfreeze it.

**Concrete steps:**
1. Move `compiler-tml/` to a separate top-level directory or mark it as archived
2. Exclude it from `.mcp.json` test discovery
3. Exclude it from `docs.json` generation (or mark entries as `@deprecated`)
4. Document the boundary in ARCHITECTURE-MAP with a clear "frozen until phase 30" callout

**Benefit:**

Default tooling becomes cleaner, test discovery faster, and documentation clearer. The self-hosting compiler is still available for whoever unfreezes it, but doesn't clutter the active developer experience.
