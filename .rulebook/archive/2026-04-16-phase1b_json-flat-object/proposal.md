# Proposal: phase1b_json-flat-object

## Why
`JsonObject = std::map<std::string, JsonValue>` (json_value.hpp:80) usa red-black tree com alocação por nó. Para 9 campos: 9 nós RB = ~720 bytes + rebalanceamento. Representa 40% do custo de parse (4,500 ns dos 11,175 ns totais). serde_json usa flat vector — 1 alocação contígua. Source: docs/analysis/json/03-bottleneck-analysis.md, Finding F-001.

## What Changes
1. Mudar `using JsonObject = std::map<std::string, JsonValue>` para `std::vector<std::pair<std::string, JsonValue>>` em json_value.hpp:80
2. Atualizar `JsonValue::get(key)` para scan linear em vez de `map::find()`
3. Atualizar `parse_object()` em json_fast_parser.cpp para `push_back` + `reserve(8)`
4. Atualizar FFI em json_runtime.cpp (`tml_json_object_get`, `tml_json_object_keys`, etc.)

## Impact
- Affected code: json_value.hpp, json_fast_parser.cpp, json_runtime.cpp, json_builder.cpp
- Breaking change: NO (C++ internal, FFI API unchanged)
- User benefit: Parse Small 11,175 ns → ~6,500 ns (1.7x improvement). Elimina 9 allocs/parse.
