# Proposal: phase1i_str-free-fast

## Why
`tml_str_free` no Windows faz `HeapValidate(heap, 0, ptr)` (~100 ns) depois do PE image range check (str_free.c:146-148). HeapValidate é uma chamada de kernel necessária porque Str não tem modelo de ownership — um ptr pode ser literal .rdata, heap, ou stack. Mas o codegen já trackeia `is_heap_str_producer` e `holds_heap_str`, sabendo em compile-time quais ponteiros são heap. HeapValidate é redundante nesses casos. Source: docs/analysis/string/03-bottleneck-analysis.md, Finding F-005.

## What Changes
1. Remover HeapValidate de tml_str_free — confiar no image range check + codegen tracking
2. Se ptr não está em nenhum PE image range: free direto via mem_free
3. Isso é seguro porque o codegen só emite tml_str_free para ponteiros que is_heap_str_producer retorna true

## Impact
- Affected code: compiler/runtime/memory/str_free.c
- Breaking change: NO
- User benefit: String deallocation 125 ns → 25 ns. Impacto em toda operação que libera strings temporárias.
