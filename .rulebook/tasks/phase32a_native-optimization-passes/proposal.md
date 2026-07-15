# Proposal: phase32a_native-optimization-passes (renumbered from phase33a, 2026-07-15 ERA 0 pivot)

## Why
The native backend currently emits unoptimised x86-64 assembly: every
value is spilled through the stack, every loop re-loads invariant
addresses, and every single-use helper function is called rather than
inlined. Benchmarks show the native backend running 4-8x slower than
LLVM -O2 on compute-intensive programs. The target before the default
backend switch (phase34a) is to be within 2x of LLVM -O2. Six passes
address the most impactful gaps: Loop-Invariant Code Motion eliminates
redundant loads inside loops, Global Value Numbering eliminates
redundant computations, inlining removes call overhead for small
helpers, dead-argument elimination shrinks call frames, tail-call
optimisation converts self-recursive calls to jumps, and loop
unrolling amortises branch cost for short known-trip-count loops.
Each pass operates on the MIR representation, making the
transformations backend-agnostic and reusable for future targets.

## What Changes
- New directory `compiler-tml/src/native/opt/` with six modules:
  - `licm.tml`: identifies loop-invariant instructions (operands all
    defined outside the loop) and hoists them to the loop pre-header.
  - `gvn.tml`: assigns value numbers to instructions; replaces
    duplicate computations with references to the first occurrence.
  - `inlining.tml`: inlines callee MIR bodies at call sites when the
    callee has fewer than 20 instructions and is not recursive.
  - `dead_arg.tml`: removes function parameters that are never used
    inside the body; updates all call sites to drop the corresponding
    argument.
  - `tailcall.tml`: converts a `return call <same_function>(args)` into
    a jump to the function entry with updated argument bindings.
  - `loop_unroll.tml`: unrolls loops with a statically-known trip count
    of 8 or fewer iterations by duplicating the body N times.
- `compiler-tml/src/native/pipeline.tml` runs these passes in order
  after MIR construction and before x86 emission.

## Impact
- Affected specs: native-backend/optimization
- Affected code: compiler-tml/src/native/opt/ (new), compiler-tml/src/native/pipeline.tml
- Breaking change: NO
- User benefit: Native backend performance reaches within 2x of LLVM -O2 on compute-intensive benchmarks, making it viable as the default backend.
