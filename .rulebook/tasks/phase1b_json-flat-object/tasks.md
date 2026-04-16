## 1. Change JsonObject typedef
- [x] 1.1 Change `using JsonObject = std::map<std::string, JsonValue>` to `std::vector<std::pair<std::string, JsonValue>>` in `compiler/include/json/json_value.hpp:80`
- [x] 1.2 Update `JsonValue::get(key)` methods to use linear scan instead of `map::find()`
- [x] 1.3 Update `JsonValue::operator[]` for object key access
- [x] 1.4 Build compiler — fix all compilation errors from map→vector change

## 2. Update parser
- [x] 2.1 In `json_fast_parser.cpp parse_object()`: replace `obj.emplace(key, value)` with `obj.push_back({key, value})`
- [x] 2.2 Add `obj.reserve(8)` at start of `parse_object()` (like `parse_array()` already does)
- [x] 2.3 Build and verify JSON parse tests still pass

## 3. Update FFI
- [x] 3.1 Update `tml_json_object_get` in json_runtime.cpp — linear scan for key
- [x] 3.2 Update `tml_json_object_keys` — iterate vector instead of map
- [x] 3.3 Update `tml_json_object_has` — linear scan
- [x] 3.4 Update any other FFI functions that use map iterators

## 4. Benchmark gate
- [x] 4.1 Run json_bench.tml — Parse Small 11,175 → 9,494 ns/op (~15% faster; further gains belong to phase1c/d/e)
- [x] 4.2 Run json tests — zero regressions (22/22 suites pass)

## 5. Tail (mandatory)
- [x] 5.1 Update docs/analysis/json/ with new numbers
- [x] 5.2 Write test: parse object with 20+ fields, verify all accessible
- [x] 5.3 Run tests and confirm pass
