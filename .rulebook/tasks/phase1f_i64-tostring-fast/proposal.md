# Proposal: phase1f_i64-tostring-fast

## Why
`I64.to_string()` usa `malloc(24) + snprintf("%ld", v)` (mir_codegen.cpp:1078-1083). Custo: 41 ns/op. Rust usa stack buffer + lookup table de 2 dígitos = 7 ns/op. Gap de 5.9x. snprintf é lento porque faz parse do format string, trata locale/padding/precision (nenhum necessário), e usa divisão por 10 em loop. TML já tem `text_i64_write_at` em text.tml que faz digit extraction manual sem snprintf. Source: docs/analysis/string/03-bottleneck-analysis.md, Finding F-001.

## What Changes
1. Substituir inline IR em mir_codegen.cpp:1078-1083 por implementação sem snprintf
2. Usar stack alloca + digit extraction loop + memcpy final para heap buffer
3. Aplicar mesma otimização para I32, I16, I8, F64.to_string()
4. Benchmark gate: 41 ns → under 15 ns

## Impact
- Affected code: mir_codegen.cpp (emit_preamble I64/I32/I16/I8 to_string)
- Breaking change: NO (mesma assinatura, mesma semântica)
- User benefit: 2-3x mais rápido para qualquer template literal ou interpolação com inteiros.
