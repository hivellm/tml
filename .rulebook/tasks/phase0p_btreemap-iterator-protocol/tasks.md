## 1. Iterator implementation
- [ ] 1.1 Adicionar `impl Iterator for BTreeMapIter[K, V]` em btreemap.tml
- [ ] 1.2 `fn next(&mut self) -> Maybe[(K, V)]` retorna Just((k,v)) ou Nothing
- [ ] 1.3 Manter API cursor legada funcionando (has_next/key/value/next void)

## 2. IntoIterator wiring
- [ ] 2.1 `impl IntoIterator for BTreeMap[K, V]` (by ref)
- [ ] 2.2 Verificar codegen de `for (k, v) in map.iter()` emite chamada ao next()
- [ ] 2.3 Destructuring de tuplas em for-in funciona corretamente

## 3. HashMap paridade (bônus)
- [ ] 3.1 Verificar se `HashMap.iter()` já suporta `for` — se não, replicar

## 4. Testes
- [ ] 4.1 Teste: for (k, v) in map.iter() itera em ordem sorted
- [ ] 4.2 Teste: early break dentro do for
- [ ] 4.3 Teste: iter vazio não entra no loop
- [ ] 4.4 Teste: iter consumido uma vez (não recursivo)
- [ ] 4.5 Teste: múltiplos iters independentes sobre mesmo map

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Atualizar `docs/stdlib/collections.md` (ou equivalente) com novo protocolo
- [ ] 5.2 CHANGELOG entry em "added/deprecated"
- [ ] 5.3 Rodar suíte completa, zero regressões
