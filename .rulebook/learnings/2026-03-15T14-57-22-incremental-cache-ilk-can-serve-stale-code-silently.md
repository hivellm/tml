# Incremental cache (.ilk) can serve stale code silently
**Source**: manual
**Date**: 2026-03-15
**Tags**: build, msvc, incremental-link, gotcha
When MSVC incremental linker reuses old code despite new .obj files, strings from new code don't appear in the DLL. Diagnostic: Python binary search finds strings in .lib but not DLL = incremental link using old cache. Fix: delete .ilk file, touch tml_codegen.lib, then rebuild. This is a recurring issue when iterating on codegen — always suspect stale .ilk when new code seems to have no effect.