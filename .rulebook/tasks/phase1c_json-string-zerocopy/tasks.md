## 1. Zero-copy string parsing
- [ ] 1.1 In `parse_string()`: track `has_escapes` flag during scan
- [ ] 1.2 Fast path: if no escapes, construct `std::string` from input view — avoids `string_buffer_`
- [ ] 1.3 Slow path: copy `string_buffer_` instead of move — preserves buffer for reuse
- [ ] 1.4 Build and run JSON parse tests

## 2. Benchmark gate
- [ ] 2.1 Run json_bench.tml — Parse Small should drop by ~2,000 ns
- [ ] 2.2 Verify escaped strings still parse correctly

## 3. Tail (mandatory)
- [ ] 3.1 Update docs/analysis/json/ with new allocation count
- [ ] 3.2 Test: parse JSON with escaped strings (\n, \t, \u0041)
- [ ] 3.3 Run tests and confirm pass
