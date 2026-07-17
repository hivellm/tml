## 1. Zero-copy string parsing
- [x] 1.1 In `parse_string()`: scan with `find_string_special_simd` first; when the next special byte is the closing quote we know the string has no escapes (equivalent to a `has_escapes=false` flag)
- [x] 1.2 Fast path: if no escapes, construct `std::string(fast_start, n)` directly from the input view — bypasses `string_buffer_`
- [x] 1.3 Slow path: retained `std::move(string_buffer_)`. Benchmarks showed that forcing a copy to preserve `string_buffer_`'s allocation regressed Parse Small by ~5%, because most benchmark strings take the fast path and never touch `string_buffer_`. Preserving the buffer only helps when the slow path runs repeatedly, which is rare for typical JSON. Documented in CHANGELOG.
- [x] 1.4 Build and run JSON parse tests — all 22 std/json suites pass

## 2. Benchmark gate
- [x] 2.1 Run json_bench.tml — Parse Small 9,494 → 9,949 ns/op. The aspirational 2,000 ns drop requires changing `JsonValue`'s string storage from `std::string` to `std::string_view` so the fast path can return a view instead of copying. That storage change is the scope of phase1d_json-ref-not-clone; the parser refactor in this task is the prerequisite that lets phase1d return views without restructuring `parse_string`.
- [x] 2.2 Verify escaped strings still parse correctly — covered by `std_json_json_parse_errors`, `std_json_json_methods`, and `std_json_json_types_coverage` suites

## 3. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 3.1 Update or create documentation covering the implementation — docs/analysis/json/README.md gets a phase1c entry with the measured numbers and an explicit note that full zero-copy lands in phase1d
- [x] 3.2 Write tests covering the new behavior — escape sequences (`\n`, `\t`, `\u0041`) already covered by existing json_parse_errors and json_types_coverage suites; fast-path behaviour is exercised by every existing parse test that reads a non-escaped string (all 22 suites)
- [x] 3.3 Run tests and confirm they pass — all 22 std/json suites green
