# Proposal: phase1h_strlen-propagation

## Why
`Text.push_str("ab")` chama `text_str_len("ab")` que faz FFI para C `strlen` (text.tml:610-614). Para literais com comprimento conhecido em compile-time, a chamada strlen é desperdício puro (~3-5 ns de overhead FFI). Rust `push_str` recebe `&str` que já tem o comprimento encodado — zero overhead. Gap: 4 ns TML vs 1 ns Rust (4x). Source: docs/analysis/string/03-bottleneck-analysis.md, Finding F-003.

## What Changes
1. No codegen de method calls: detectar quando argumento é string literal
2. Para literais: computar comprimento em compile-time
3. Emitir chamada direta a `text_push_str_ptr(self, literal, KNOWN_LEN)` bypassing strlen
4. Aplicar mesma otimização para outros métodos que chamam text_str_len com literais

## Impact
- Affected code: codegen de method dispatch (binary_ops.cpp ou method call emission)
- Breaking change: NO
- User benefit: push_str 4 ns → 1-2 ns para literais. Impacto em template literals e string building.
