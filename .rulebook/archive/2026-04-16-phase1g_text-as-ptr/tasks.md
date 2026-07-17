## 1. Fast-path print / println
- [x] 1.1 `print` / `println` now branch on `_w2 >= 0` (heap mode) and pass `this._w0 as Str` directly — no `as_str()` copy
- [x] 1.2 Heap path: `_w0` is already null-terminated (every Text constructor allocates `len + 1` and writes the trailing `\0`)
- [x] 1.3 Inline path (len ≤ 23): falls back to `as_str()` — the copy is ≤ 24 bytes so overhead is negligible and avoids the stack-data-escape hazard of returning a pointer into a by-value struct
- [x] 1.4 Not adding a standalone `as_ptr()` method — the heap/inline branch lives inside print/println where it's actually consumed. Callers that need a lifetime-bounded pointer can use `as_str()` (heap-owned copy)

## 2. Wire into print methods
- [x] 2.1 `lib/std/src/text.tml::print` — updated
- [x] 2.2 `lib/std/src/text.tml::println` — updated
- [x] 2.3 Build + text tests — green

## 3. Benchmark gate
- [x] 3.1 `string_bench` Log Building (Text) runs at 87 ns/op per append+println loop iteration — no allocation spike on each `println`
- [x] 3.2 5/5 correctness tests cover inline (short, 23-char boundary) and heap (24-char, 64-char long) plus empty

## 4. Tail (mandatory)
- [x] 4.1 Update or create documentation covering the implementation (`docs/patches/v0.3.26-0.3.36.md` v0.3.33 section + VERSION bump)
- [x] 4.2 Write tests covering the new behavior (`lib/std/tests/text/text_print_fastpath.test.tml` — 5 cases)
- [x] 4.3 Run tests and confirm they pass — 1 suite / 5 cases, all green
