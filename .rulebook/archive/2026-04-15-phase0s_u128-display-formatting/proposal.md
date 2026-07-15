# Proposal: phase0s_u128-display-formatting

## Why

`U128` and `I128` types lack `Display` and `to_string()` implementations.
Any code that needs to print a 128-bit value (connection IDs, UUIDs stored
as u128, large counters) must resort to workarounds like splitting into two
`I64` halves and formatting manually. Reported by the UzDB AI agent who needed
to log `ConnectionId: U128` values and had to write awkward split-format code.
Standard in every systems language: Rust `u128::to_string()`, Go `fmt.Sprintf
("%d", ...)`, C `__uint128_t` via custom printer.

## What Changes

- `lib/core/src/num.tml`: add `Display` impl for `U128` / `I128` — decimal
  formatting via repeated division by 10 (no hardware 128-bit divide on x86;
  must use 64-bit division chaining).
- Also add: `U128::from_str(s: Str) -> Maybe[U128]`, `I128::from_str`,
  `U128::to_hex() -> Str`, `I128::to_hex() -> Str`.
- Alternatively, implement via C runtime shim `tml_u128_to_str` if the pure
  TML approach is blocked by codegen limitations.

## Impact

- Affected specs: `lib/core/src/num.tml` (U128/I128 methods)
- Affected code: core numeric module
- Breaking change: NO (additive)
- User benefit: databases, networking code, UUID handling, and any code using
  128-bit values can log/display them without workarounds.
