# MIR has 52 optimization passes between construction and IR emission
**Source**: manual
**Date**: 2026-03-15
**Tags**: codegen, mir, optimization, passes
The MIR path has 52 optimization passes available: mem2reg, load_store_opt, SROA, EarlyCSE, inlining, DSE, DestinationProp, strength reduction, and more. These run between MIR construction and LLVM IR emission. MIR is already in SSA form so codegen is a direct translation. The O0 pipeline (debug) still runs a subset of these passes for correctness.