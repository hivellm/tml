## 1. Implementation
- [ ] 1.1 Define coroutine state struct: for each async function emit a struct type whose fields are the function's locals that are live across any await point plus an I64 resume_point discriminant; register the struct in the type registry
- [ ] 1.2 Transform async function body into state machine: split the MIR instruction list at each Await instruction into numbered resume blocks (block 0 = entry before first await, block N = continuation after await N-1)
- [ ] 1.3 Emit yield path: at each await site, store all live locals back into the state struct, increment resume_point, and emit a return of PollResult::Pending to the executor
- [ ] 1.4 Emit resume path: at the top of each resume block (after the switch dispatch), reload live locals from the state struct fields so execution continues with the correct values
- [ ] 1.5 Emit poll wrapper: generate a `poll(state_ptr: RawPtr, cx_ptr: RawPtr) -> I64` function that loads resume_point, emits a switch/jump-table dispatching to each resume block label, and propagates PollResult from inner awaited futures
- [ ] 1.6 Test: compile an async function that awaits a future already resolved to a known value; assert the poll wrapper returns Ready(value) on the second call (first call returns Pending, second resumes and completes)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
