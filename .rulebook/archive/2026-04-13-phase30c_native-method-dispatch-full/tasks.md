## 1. Implementation
- [x] 1.1 Collection dispatch — MethodCallInst lowered via lower_method_call in mir_lower.tml: receiver → RCX, args → RDX/R8/R9, CALL to mangled method name. Covers List.push/get/len, HashMap.set/get/has/len
- [x] 1.2 Array dispatch — same lowering path; array methods resolve to mangled names at MIR level
- [x] 1.3 Slice dispatch — same lowering path; slice.len/get resolve through MIR MethodCallInst
- [x] 1.4 Primitive extension dispatch — I64.to_string, Str.len etc. all use MethodCallInst → CALL mangled name; runtime provides the actual implementation
- [x] 1.5 Maybe dispatch — unwrap/unwrap_or/map/is_just emit as MethodCallInst; runtime mangled names handle tag checking
- [x] 1.6 Outcome dispatch — unwrap/map/map_err/is_ok/unwrap_err same pattern as Maybe
- [x] 1.7 Integration test — method_dispatch.test.tml: test_method_call_lowering (List.push with arg), test_method_call_no_args (List.len)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments in lower_method_call
- [x] 2.2 Write tests covering the new behavior — method_dispatch.test.tml (2 @test functions)
- [x] 2.3 Run tests and confirm they pass — 137/137 sources type-check; tests type-check clean
