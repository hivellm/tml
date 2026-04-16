# Proposal: phase1c_json-string-zerocopy

## Why
`parse_string()` em json_fast_parser.cpp:657 faz `return std::move(string_buffer_)` — transfere o buffer interno para o caller. Na próxima chamada, `string_buffer_` está vazio e o primeiro `.append()` aloca novo buffer. 13 strings por parse = 13 alocações (~2,600 ns, 23% do custo). Para strings sem escape sequences, é possível retornar `string_view` no input original — zero cópia. Source: docs/analysis/json/03-bottleneck-analysis.md, Finding F-005.

## What Changes
1. Em `parse_string()`: detectar se a string tem escape sequences
2. Fast path (sem escapes): retornar `std::string_view` no input — zero alocação
3. Slow path (com escapes): copiar `string_buffer_` em vez de mover, preservando o buffer para reuso
4. Ajustar `JsonValue` para aceitar `string_view` (ou converter para `std::string` no ponto de uso)

## Impact
- Affected code: json_fast_parser.cpp, json_value.hpp
- Breaking change: NO
- User benefit: Parse Small ~2,600 ns saved. 13 allocs → 0-4 allocs para strings.
