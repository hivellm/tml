# Incremental implementation beats big-bang — test each stage
**Source**: manual
**Date**: 2026-03-21
**Tags**: implementation, methodology, testing, anti-pattern, critical
When implementing features (especially across multiple files), NEVER implement everything at once. Break into small testable stages, verify each one before moving to the next.

Evidence: Chunked encoding implementation. First attempt via single agent writing 6 files at once → crashed with Bool/i1 layout bug, missing trailers reference, cross-module struct field resolution returning (). Spent hours patching cascading errors. Second attempt: implement core type → test → decoder function → test → integration → test → all passed first try.

Rule: If 2-3 fix attempts fail on the same error, STOP. Delete broken code, re-analyze, choose a different approach. "The line between persistence and stupidity is very thin."