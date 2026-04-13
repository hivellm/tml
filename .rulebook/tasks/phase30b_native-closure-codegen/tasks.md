## 1. Implementation
- [ ] 1.1 Capture analysis: walk closure MIR body, collect every local variable referenced that is defined outside the closure; build CaptureEnv struct layout with one I64-aligned field per captured variable
- [ ] 1.2 Closure function emission: emit a concrete function named `__closure_N` with env: RawPtr as first parameter; generate field-offset loads to read each capture from the env pointer
- [ ] 1.3 Closure construction: at the closure expression site, alloc env struct on heap (malloc), store each captured local into the correct field offset, build {fn_ptr, env_ptr} fat pointer pair
- [ ] 1.4 Indirect call lowering: at CallIndirect MIR instructions, extract fn_ptr and env_ptr from fat pointer, prepend env_ptr to the argument list, emit CALL through fn_ptr
- [ ] 1.5 Test: closure that captures a local I64 variable and returns it multiplied by an argument; verify result equals expected value
- [ ] 1.6 Test: closure passed as callback to a higher-order function (map-style); verify each element is transformed correctly

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
