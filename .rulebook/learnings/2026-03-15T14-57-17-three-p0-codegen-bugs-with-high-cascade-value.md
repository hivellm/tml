# Three P0 codegen bugs with high cascade value
**Source**: manual
**Date**: 2026-03-15
**Related Task**: fix-codegen-coverage-blockers
**Tags**: codegen, blockers, priority, cascade
Three specific bugs gate large amounts of downstream work: (1) Generic trait dispatch returning () — blocks Array.hash, Pool::acquire, Range::size_hint, ~30 functions (fix-codegen-coverage-blockers). (2) Lambda→func ptr conversion in call.cpp:124+ — blocks ALL of stdlib-essentials Phase 2 including Vec::retain, Vec::from_iter, HashSet::from_iter (~6 items). (3) Symbol collision in suite merging (repeat[T]/repeat_char) — forces individual test mode, degrading compilation throughput. Fixing these three in order maximizes cascade unblocking.