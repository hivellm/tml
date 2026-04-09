# TML plain enum pattern matching: bare variant names only
**Source**: manual
**Date**: 2026-04-09
**Related Task**: phase13c_tml-parser
**Tags**: tml, enum, pattern-matching, when, t069, syntax
In TML `when` arms, plain enum variants use BARE names without the enum type prefix:

```tml
// CORRECT
when tok.kind {
    Eof => return true,
    Newline => { p = p + 1 },
    Identifier => { ... },
    _ => return false
}

// WRONG — T069 "Pattern expects enum type, but got different type"
when tok.kind {
    TokenKind::Eof => ...,    // ERROR: double-colon prefix in pattern
}
```

This applies to ALL plain enums (TokenKind, Visibility, etc.) in pattern position. ADT sum-type variant construction uses the full name (`TypeExpr::Named(nt)`) but pattern matching uses bare names (`Named(nt) =>`).

For equality checks on plain enums (not in pattern context), use integer cast:
```tml
func tok_kind_eq(a: TokenKind, b: TokenKind) -> Bool {
    return a as I64 == b as I64
}
```