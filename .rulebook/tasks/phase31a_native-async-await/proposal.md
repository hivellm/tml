# Proposal: phase31a_native-async-await (renumbered from phase32a, 2026-07-15 ERA 0 pivot)

## Why
TML supports `async`/`await` for concurrent programming and its runtime
executor is already implemented in C. However, the native backend has no
codegen for async functions: every `async func` and every `await` expression
currently falls through to the LLVM path. This means no async-heavy program
(HTTP servers, database clients, file-watchers) can be compiled natively.
The implementation follows the standard coroutine state-machine transformation:
an async function is split into a struct (holding locals and a resume point
discriminant) plus a `poll` function that switches on the discriminant and
jumps to the correct resume label. This transformation is well-understood,
produces minimal overhead, and is entirely expressible in the x86 assembly
the native backend already emits.

## What Changes
- `compiler-tml/src/native/mir_lower.tml` gains an async transformation pass
  that runs immediately after MIR is lowered for a function marked `is_async`:
  - A coroutine state struct is generated containing one field per local
    variable that is live across an `await` point, plus an `I64 resume_point`
    discriminant field.
  - The function body is split at each `await` site into numbered resume
    blocks (0 = initial entry, N = resume after await N-1).
  - Each resume block saves live locals into the state struct before
    yielding and reloads them on the next entry.
  - A `poll(state_ptr, cx_ptr) -> PollResult` wrapper is emitted that
    switches on `resume_point` and dispatches to the appropriate block.
  - The future being awaited is polled; if it returns `Pending` the wrapper
    stores the updated `resume_point` and returns `Pending` to the executor;
    if `Ready(value)` the wrapper continues into the next block.

## Impact
- Affected specs: native-backend/async
- Affected code: compiler-tml/src/native/mir_lower.tml
- Breaking change: NO
- User benefit: Async functions and await expressions compile and run natively through the TML executor without requiring the LLVM backend.
