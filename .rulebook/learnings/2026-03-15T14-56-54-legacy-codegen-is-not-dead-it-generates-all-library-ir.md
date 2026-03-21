# Legacy codegen is NOT dead — it generates all library IR
**Source**: manual
**Date**: 2026-03-15
**Related Task**: codegen-structural-fixes
**Tags**: codegen, architecture, mir, legacy
The "legacy" LLVMIRGen path (~49K lines) is misleadingly named. It generates ALL standard library function implementations in every build. The MIR path (~3K lines) handles only user code. True elimination of the legacy path requires MIR to also handle: library IR generation, debug info, coverage instrumentation, and async codegen. Any investment in "fixing legacy" should be weighed against closing MIR gaps instead.