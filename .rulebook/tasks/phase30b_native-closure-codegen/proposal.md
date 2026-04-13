# Proposal: phase30b_native-closure-codegen

## Why
Closures are fundamental to TML: iterators, callbacks, map/filter/reduce chains,
and event handlers all use first-class functions that capture their environment.
Without native closure codegen, any program using higher-order functions falls
back to the LLVM backend. This blocks the native backend from compiling most
real-world TML code including the standard library itself. Closure support is a
prerequisite for iterator pipelines, async state machines, and behavior objects.

## What Changes
- `compiler-tml/src/native/mir_lower.tml` gains a closure lowering pass that:
  1. Collects captured variables from each closure expression and builds a heap-
     allocated capture struct (one field per captured local, in capture order).
  2. Emits a named function that takes the capture struct pointer as a hidden
     first parameter (`env: RawPtr`) and accesses captures via field offsets.
  3. Builds a fat pointer pair (fn_ptr, env_ptr) at the closure construction site.
  4. At indirect call sites, extracts fn_ptr and env_ptr from the fat pointer and
     emits a CALL through the function pointer with env_ptr prepended to args.

## Impact
- Affected specs: native-backend/closures
- Affected code: compiler-tml/src/native/mir_lower.tml
- Breaking change: NO
- User benefit: Closures, callbacks, and higher-order functions compile and execute correctly through the native backend without requiring LLVM.
