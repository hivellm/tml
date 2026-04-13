## 1. Implementation
- [ ] 1.1 Collection dispatch: emit runtime calls for List.push(val), List.get(i), List.len(), List.pop(), HashMap.set(k,v), HashMap.get(k), HashMap.has(k), HashMap.len() using their C ABI signatures
- [ ] 1.2 Array dispatch: emit GEP-based indexed get and set, emit load of length field, emit slice-construction (base ptr + len) from subrange
- [ ] 1.3 Slice dispatch: emit len load from slice header, emit GEP-based get, emit iter start/end pointer pair
- [ ] 1.4 Primitive extension dispatch: I64.to_string via sprintf into heap buffer, Str.len via strlen, Str.contains via strstr, Str.starts_with via strncmp, Str.slice via ptr+offset
- [ ] 1.5 Maybe dispatch: unwrap (extract payload or abort), unwrap_or (branch on tag), map (call closure fn_ptr with payload), is_just (compare tag to 1)
- [ ] 1.6 Outcome dispatch: unwrap (extract Ok payload or abort), map (call closure on Ok), map_err (call closure on Err), is_ok (compare tag), unwrap_err
- [ ] 1.7 Integration test: 10 distinct method call sites spanning all 6 categories compile and produce correct output natively

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
