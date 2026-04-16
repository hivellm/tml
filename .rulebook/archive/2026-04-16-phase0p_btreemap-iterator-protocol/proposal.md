# Proposal: phase0p_btreemap-iterator-protocol

## Why

`BTreeMapIter` atualmente expõe uma API de cursor (has_next/key/value/next() void)
em vez do protocolo padrão `Iterator`:

```tml
// Atual (cursor)
loop (iter.has_next()) {
    let k = iter.key()
    let v = iter.value()
    iter.next()
}

// Esperado (idiomático Rust/Python)
for (k, v) in map.iter() { ... }
```

Problemas:
- Não é descobrível — usuários esperam `for` funcionar
- `for-in` não compila em `BTreeMapIter`
- Comportamento `Iterator` em `core::iter::traits` existe mas não está implementado
- Inconsistente com `List.iter()` que suporta `for`

Source: UzDB feedback letter, P3-9. Listado em PLANS.md como pending.

## What Changes

1. Em `lib/std/src/collections/btreemap.tml`:
   - `impl Iterator for BTreeMapIter[K, V]` com `next() -> Maybe[(K, V)]`
   - Manter API de cursor (`has_next`, `key`, `value`, `next()` void) como deprecated

2. Em `lib/std/src/collections/btreemap.tml`:
   - `impl IntoIterator for BTreeMap[K, V]` (by ref)
   - `fn iter(&self) -> BTreeMapIter[K, V]` usa o protocolo novo

3. Wire com `for-in`:
   - Confirmar que `for (k, v) in map.iter()` gera codegen correto
   - Confirmar destructuring de tuplas funciona em for-in

4. Testes:
   - `for (k, v) in map.iter()` itera todas as entries em ordem sorted
   - Early break funciona
   - Iterator é consumido (não é recursivo)
   - Teste também para `HashMap.iter()` para paridade

## Impact

- Affected specs: std/collections/btreemap
- Affected code: `lib/std/src/collections/btreemap.tml`, `lib/std/tests/collections/btreemap_iter.test.tml`
- Breaking change: NO (API antiga permanece; API nova adicionada)
- User benefit: Sintaxe idiomática, melhor discoverability, paridade com List/HashMap. P3.
