# Proposal: phase1d_json-ref-not-clone

## Why
`tml_json_object_get(handle, key)` faz `field->clone()` + `alloc_json_handle(clone)` — deep copy completa do valor (json_runtime.cpp:468-469). Cada acesso a campo aloca 2 vezes. serde_json retorna `&Value` (referência, zero alloc). Field Access = 15,320 ns no TML vs 7,100 ns no Rust. Source: docs/analysis/json/03-bottleneck-analysis.md, Finding F-004.

## What Changes
1. Adicionar conceito de "borrowed handle" — handle que referencia um valor dentro de um documento existente, sem clonar
2. `tml_json_object_get` retorna borrowed handle (ponteiro para o campo in-place)
3. `tml_json_array_get` retorna borrowed handle
4. Borrowed handles não são liberados por `tml_json_free` — o documento pai gerencia a memória

## Impact
- Affected code: json_runtime.cpp (accessor FFI functions)
- Breaking change: NO (handles são opacos I64, semântica de uso não muda)
- User benefit: Field Access 15,320 ns → ~1,000 ns. Zero allocs por acesso.
