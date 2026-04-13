# Proposal: phase30f_native-let-patterns-complete

## Why
The native backend's `emit_let.tml` supports only simple name and tuple
destructuring in `let` bindings. Nested enum patterns (`let Just(Just(x)) = v`),
guard patterns (`let x if x > 0 = expr`), or-patterns (`let A | B = v`), and
nested struct patterns (`let Point { x, y } = p`) are silently mis-compiled or
crash the emitter. These patterns appear throughout the TML standard library and
any realistic codebase. Without complete `let`-pattern support the native backend
cannot reliably compile standard-library code such as parser combinators, iterator
adapters, or configuration parsers that rely on nested destructuring.

## What Changes
- `compiler-tml/src/native/x86/emit_let.tml` is extended with four new pattern
  branches: nested enum (recursive dispatch into variant payload offsets), guard
  (conditional branch after binding, jumping to the else block on failure), or-
  pattern (try each alternative in order, binding on the first match), and nested
  struct (field-by-field GEP load into named locals for each field pattern).
- A shared `emit_pattern_match(pat, src_ptr, mismatch_label)` helper is factored
  out so all four new branches reuse the same label-generation and register-
  allocation infrastructure.

## Impact
- Affected specs: native-backend/let-patterns
- Affected code: compiler-tml/src/native/x86/emit_let.tml
- Breaking change: NO
- User benefit: All `let`-binding pattern forms compile correctly natively, enabling reliable use of destructuring in standard-library and user code.
