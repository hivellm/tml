# Proposal: phase1e_json-arena-wiring

## Why
JsonArena (json_allocator.hpp:194-306) tem bump allocation com 64KB blocks, string interning, e reset O(1) — mas parse_json_fast() não usa. O parser aloca com std::string/std::map/std::vector default allocators. A arena é dead code. Wiring a arena no parser eliminaria TODAS as alocações per-parse. Source: docs/analysis/json/03-bottleneck-analysis.md, Finding F-003.

## What Changes
1. Adicionar `JsonArena*` como membro de FastJsonParser
2. `parse_string()`: usar `arena->alloc_string()` para keys
3. `parse_object()`/`parse_array()`: usar pmr::vector/map com arena como backing allocator
4. `tml_json_parse_fast()`: criar arena thread-local, manter viva até free()

## Impact
- Affected code: json_fast_parser.hpp/cpp, json_runtime.cpp
- Breaking change: NO
- User benefit: Parse Small 11,175 ns target under 2,000 ns (5-6x improvement).
