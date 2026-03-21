# generate() is 8-phase monolithic function with ~1800 lines
**Source**: manual
**Date**: 2026-03-15
**Tags**: codegen, legacy, generate, architecture
The legacy codegen entry point generate() in generate.cpp is ~1800 lines orchestrating 8 phases: (1) library IR restoration/generation, (2) struct/enum declarations, (3) function signature declarations, (4) function body codegen, (5) generic monomorphization via pending queues, (6) vtable emission, (7) entry point generation (main), (8) IR finalization. It maintains ~50 mutable state maps. The monomorphization phase iterates pending queues until convergence — each instantiation may queue more work.