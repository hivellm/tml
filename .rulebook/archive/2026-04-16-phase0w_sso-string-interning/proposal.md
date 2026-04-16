# Proposal: phase0w_sso-string-interning

## Why
TML's `Text` type always heap-allocates its buffer, even for 1-15 character strings. Rust's `String` (and `smol_str` / `compact_str`) uses Small String Optimization (SSO): strings ≤15 bytes are stored inline in the struct (stack), avoiding malloc entirely. The TML compiler itself creates millions of short strings (identifiers, keywords, type names) during compilation — each one currently calls `malloc`. String-heavy workloads benchmark at 3-5x Rust overhead partly due to this allocation pattern. Additionally, identifier strings in the compiler are repeatedly compared for equality — interning them (storing each unique string once and comparing by pointer) would reduce O(n) `strcmp` to O(1) pointer equality. See `docs/analysis/benchmark/14-text-stringbuilder.md` and `docs/analysis/benchmark/08-compilation.md`.

## What Changes
1. **SSO for `Text`**: change the `Text` struct layout to use a tagged union:
   - Inline variant (len ≤ 15): `{ len: I8, data: [I8; 15] }` — 16 bytes, no heap allocation
   - Heap variant (len > 15): `{ len: I64, ptr: RawPtr, cap: I64 }` — 24 bytes, heap allocated
   - Tag in the high bit of `len` field distinguishes variants
2. **String interning table**: a global `HashMap[Text, I64]` maps each unique identifier seen by the compiler to an integer ID. `intern(str)` returns the ID; `Text` equality for interned strings becomes integer comparison. The intern table is thread-local for parallel codegen compatibility.
3. **`Str` (string slice)**: ensure `Str` literals reference static memory — no allocation for string literals.

## Impact
- Affected specs: core/text, compiler/intern
- Affected code: `lib/core/src/text.tml` (SSO layout), new `compiler-tml/src/intern.tml` (intern table), `lib/core/src/str.tml` (literal refs)
- Breaking change: POTENTIALLY (Text struct layout changes — any lowlevel code reading Text fields directly must be updated)
- User benefit: Eliminate malloc for all short strings (identifiers, keywords). Compiler throughput improvement of 2-3x for parse/type-check phases. Production applications with many short strings benefit similarly.
