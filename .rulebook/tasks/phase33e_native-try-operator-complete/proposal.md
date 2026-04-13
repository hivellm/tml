# Proposal: phase33e_native-try-operator-complete

## Why
The `?` try operator is pervasive in idiomatic TML — virtually every function that calls fallible operations uses it. The native backend's current `emit_call.tml` handles the simple single-level case (`let x = foo()?`) but fails on chained try expressions (`foo()?.bar()?`), try inside loop bodies (`loop { let x = iter.next()? }`), and error wrapping with `From` conversion when the error types differ. Without these three scenarios, entire modules (I/O, parsing, networking) cannot compile on the native backend.

## What Changes
- `emit_call.tml`: fix chained try by tracking the unwrapped value through successive `?` applications — each `?` checks the `Outcome` discriminant, branches to an early-return block on `Err`, extracts the `Ok` payload, and passes it as the receiver of the next call.
- `emit_call.tml`: add error wrapping via `From::from` — when the called function's error type differs from the enclosing function's return error type, emit a call to the appropriate `From::from` implementation before constructing the `Err` return value.
- `emit_call.tml`: handle try inside loop bodies — ensure the early-return block for a `?` inside a loop exits the enclosing function (not just the loop), by threading the loop's landing pad correctly around the try branch.
- New test file `compiler-tml/tests/codegen/try_operator.test.tml` covering all three scenarios.

## Impact
- Affected specs: `compiler-tml/src/codegen/emit_call.tml`
- Affected code: `compiler-tml/src/codegen/emit_call.tml`
- Breaking change: NO
- User benefit: I/O, parsing, and networking code using chained `?` and `?` in loops compiles correctly on the native backend without falling back to the C++ codegen path.
