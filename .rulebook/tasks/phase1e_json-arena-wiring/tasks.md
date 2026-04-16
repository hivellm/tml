## 1. Thread arena into parser
- [ ] 1.1 Add `JsonArena* arena_` member to FastJsonParser
- [ ] 1.2 Update constructor to accept optional arena parameter
- [ ] 1.3 In parse_string(): use arena->alloc_string() for string allocation
- [ ] 1.4 In parse_object(): use arena for object node allocation
- [ ] 1.5 In parse_array(): use arena for vector buffer allocation

## 2. Wire into FFI
- [ ] 2.1 In tml_json_parse_fast(): create thread-local arena before parse
- [ ] 2.2 Keep arena alive until tml_json_free() is called
- [ ] 2.3 Build and verify all JSON tests pass

## 3. Benchmark gate
- [ ] 3.1 Run json_bench — Parse Small under 3,000 ns (from 11,175 ns)

## 4. Tail (mandatory)
- [ ] 4.1 Update docs with new allocation counts
- [ ] 4.2 Test: parse, access, free cycle with arena — zero leaks
- [ ] 4.3 Run tests and confirm pass
