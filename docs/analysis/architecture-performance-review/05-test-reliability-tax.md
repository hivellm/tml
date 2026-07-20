# 05 — Test Reliability as a Hidden Speed Tax

## Overview

When test results themselves are unreliable, caching becomes useless and testing becomes a confidence-building ritual requiring reruns. Both are opposite of fast feedback.

---

### F-018 — The standalone dispatcher reported panicking `@test` bodies as `test_pass`

**Impact: High**  
**Status: RESOLVED (phase44a v0.3.83)**

**The bug:**

A discarded SEH catch-result in the test dispatcher meant a whole class of failures — panicking `@test` bodies — read as green.

**Evidence:**  
`docs/analysis/phase44a-fallout/01-revealed-failures.md`

**Speed connection:**

A test suite that can silently pass on failure forces:
- Defensive re-runs to gain confidence
- Manual verification of results
- Skepticism toward cached "green" results

The rule "run once, redirect to a log, never re-filter" (`.claude/rules/no-bash-filtering.md`) is itself a workaround for how expensive a rerun is.

**Current status:**

Fixed in phase44a. Test results are now trustworthy.

---

### F-019 — Composition-sensitive heap corruption: tests pass suite-packed, crash standalone

**Impact: High**  
**Status: OPEN (phase44b)**

**The symptom:**

Three `std/collections` files crash only standalone with `HEAP_CORRUPTION` / `TIMEOUT` / `STACK_BUFFER_OVERRUN`, with different signals across identical reruns.

**Evidence:**  
`docs/analysis/phase44a-fallout/01-revealed-failures.md:30-56`

**Why it matters for performance:**

Nondeterministic tests defeat caching:
- A "passed" result can't be trusted (it might crash in a different pack)
- Reruns are forced to gain confidence
- The cache is useless for nondeterministic tests

This is the exact opposite of the fast-feedback goal.

**Root causes:**

Trace to F-016 (clone-read/drop asymmetry) and F-017 (hand-rolled byte counts):
- `Deque::iter()` UAF or `List::iter()` refcount leak depending on heap layout
- `mem_alloc(8)` vs 12-byte struct corrupts neighboring allocations nondeterministically
- Which corruption manifests depends on test pack order and the order allocations reuse freed memory

**Current status:**

Phase44b is actively fixing the underlying F-016/F-017 bugs. Once those land, the nondeterminism should disappear, and cache results become trustworthy again.

---

### Summary: Reliability as a speed lever

A 10% test suite that is reliable beats a 100% suite that is flaky:
- Reliable suite runs once → cache hit → ~0 ms on reruns
- Flaky suite runs N times to build confidence → cache misses N times → N× the time

The compositional nondeterminism (F-019) is a structural consequence of F-016/F-017. Fixing those is essential not just for correctness, but for making testing fast.
