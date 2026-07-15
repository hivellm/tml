# Proposal: phase1g_text-as-ptr

## Why
`Text.as_str()` faz `mem_alloc(slen+1) + copy_nonoverlapping` em TODA chamada (text.tml:482-493). Para SSO inline strings de 5 bytes, aloca 6 bytes no heap e copia — derrotando completamente o propósito do SSO. `text.println()` chama `as_str()` internamente, causando alocação em cada print. Para heap mode, o buffer já está null-terminated — basta retornar o ponteiro. Source: docs/analysis/string/03-bottleneck-analysis.md, Finding F-002.

## What Changes
1. Adicionar `Text.as_ptr() -> Str` que retorna ponteiro direto sem copiar
2. Heap mode: retorna `_w0 as Str` (já null-terminated)
3. Inline mode (len < 23): escreve \0 no byte `len` do struct e retorna ponteiro ao struct
4. Inline mode (len == 23): fallback para as_str() (byte 23 é o tag, não pode ser sobrescrito)
5. Atualizar `Text.print()` e `Text.println()` para usar `as_ptr()` em vez de `as_str()`

## Impact
- Affected code: lib/std/src/text.tml (novo método + update print/println)
- Breaking change: NO (aditivo)
- User benefit: Elimina 1 alocação por print de Text. Impacto pervasivo em todo código que imprime strings.
