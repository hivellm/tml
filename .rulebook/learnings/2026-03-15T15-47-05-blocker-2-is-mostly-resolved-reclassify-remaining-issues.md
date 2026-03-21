# BLOCKER #2 is MOSTLY RESOLVED — reclassify remaining issues
**Source**: manual
**Date**: 2026-03-15
**Related Task**: fix-codegen-coverage-blockers
**Tags**: blocker, codegen, resolved, reclassify
Deep analysis shows generic trait dispatch returning () is mostly fixed: 52/52 core/iter tests pass, 26/26 core/cell tests pass. The fix-codegen-coverage-blockers document is STALE. Remaining uncovered functions have DISTINCT root causes: (1) Pool::acquire — ACCESS_VIOLATION from lowlevel pointer dereference on cast-from-integer pointers, NOT generic dispatch. (2) F32/F64 sum/product — 'integer constant must have integer type' float literal codegen bug in generic context. These should be tracked as separate bugs, not conflated under 'generic trait dispatch'.