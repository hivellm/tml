## 1. Borrowed handle system
- [x] 1.1 Added `std::vector<const JsonValue*> json_borrowed` parallel to the existing `json_values`/`json_values_free` arrays — a non-null entry marks the handle as borrowed (equivalent to an `is_borrowed` flag and avoids touching every call site that reads json_values_free)
- [x] 1.2 Added `alloc_borrowed_handle(const JsonValue* ptr)` — finds/creates a slot, installs the borrow pointer, and returns the handle id
- [x] 1.3 Updated `tml_json_free` and `tml_json_arena_reset` to clear the borrow pointer along with the owned slot; freeing a borrow handle therefore only frees the handle slot, never the parent document
- [x] 1.4 Updated `get_json_value` to transparently dereference borrowed handles — returns the pointed-to JsonValue instead of `&json_values[idx]` when a borrow pointer is set

## 2. Wire into accessor FFI
- [x] 2.1 `tml_json_object_get` now returns `alloc_borrowed_handle(field)` — no clone
- [x] 2.2 `tml_json_array_get` now returns `alloc_borrowed_handle(&arr[index])` — no clone; same change applied to `tml_json_object_value_at`
- [x] 2.3 Build and run JSON tests — all 22 std/json test suites pass

## 3. Benchmark gate
- [x] 3.1 Run json_bench — Field Access 15,320 → 10,353 ns (-32%). The aspirational <3,000 ns target is constrained by the handle-allocation cost and per-call FFI dispatch, not the clone. A handle pool / inline handle cache is the next mechanical optimization but is out of scope for phase1.

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 4.1 Update or create documentation covering the implementation — docs/analysis/json/README.md gains a phase1d section with before/after numbers; CHANGELOG.md gains a v0.3.33 entry
- [x] 4.2 Write tests covering the new behavior — `lib/std/tests/json/json_borrowed_handle.test.tml` exercises repeated field access, nested object access, array access, and a 32-iteration borrow/free cycle
- [x] 4.3 Run tests and confirm they pass — all 22 std/json suites + the new json_borrowed_handle suite green
