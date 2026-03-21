# Revised blocker priority after deep analysis — only 2 active blockers
**Source**: manual
**Date**: 2026-03-15
**Tags**: blockers, priority, revised, planning
After deep root-cause analysis of all 5 claimed blockers: ACTIVE: (1) Lambda→Func ptr / MIR closure stub — CRITICAL, blocks all higher-order stdlib APIs. (3) Suite merging symbol collision — MEDIUM, workaround in place. RESOLVED: (2) Generic trait dispatch — 52/52 iter + 26/26 cell pass, remaining issues are distinct bugs. (4) Closure capture — subsumed by #1. (5) cell/lazy — fully fixed 26/26. Recommended fix order: #1 first (highest cascade), #3 in parallel (independent). The fix-codegen-coverage-blockers task document is STALE and should be updated.