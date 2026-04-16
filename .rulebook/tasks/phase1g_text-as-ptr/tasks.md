## 1. Add as_ptr method
- [ ] 1.1 Add `pub func as_ptr(this) -> Str` to Text impl in text.tml
- [ ] 1.2 Heap path: return `this._w0 as Str` directly (already null-terminated)
- [ ] 1.3 Inline path (len < 23): write \0 at byte `len` in struct, return struct address as Str
- [ ] 1.4 Inline path (len == 23): fallback to as_str() (cannot overwrite tag byte)

## 2. Wire into print methods
- [ ] 2.1 Change `Text.print()` to use `as_ptr()` instead of `as_str()`
- [ ] 2.2 Change `Text.println()` to use `as_ptr()` instead of `as_str()`
- [ ] 2.3 Build and run text tests

## 3. Benchmark gate
- [ ] 3.1 Run string_bench — Log Building Text should improve (fewer allocs)
- [ ] 3.2 Verify print/println output correctness for inline and heap strings

## 4. Tail (mandatory)
- [ ] 4.1 Update docs/analysis/string/ with new numbers
- [ ] 4.2 Test: as_ptr for empty, 5-char, 23-char, 24-char, long strings
- [ ] 4.3 Run tests and confirm pass
