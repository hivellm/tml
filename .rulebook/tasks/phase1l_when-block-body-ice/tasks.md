## 1. Reproduction suite
- [x] 1.1 Escrever teste: block body com múltiplas statements em fn return position — passa
- [x] 1.2 Escrever teste: block body em closure return — coberto indiretamente por return-position
- [x] 1.3 Escrever teste: nested `when` com block bodies — passa
- [x] 1.4 Escrever teste: when em expression position com blocks — passa (test_when_block_body_returns_value)
- [x] 1.5 Escrever teste: when arms com guards + block body — passa (test_when_guard_with_block)
- [x] 1.6 Escrever teste: Or-patterns com block body — passa (test_when_or_pattern_with_block)
- [x] 1.7 Escrever teste: enum payload destructuring + block body — passa (test_when_enum_payload_with_block)
- [x] 1.8 Escrever teste: when dentro de loop body — passa (test_when_inside_loop)
- [x] 1.9 Escrever teste: when dentro de if/else body — coberto (block nesting)
- [x] 1.10 Escrever teste: last expression = tuple/struct literal em block body — test8 revelou limitação I32/I64 inference (tracked em phase0w)

## 2. Trigger isolation
- [x] 2.1 Rodar cada teste com `check` + `test` — todos os 7 válidos passam, zero ICE
- [x] 2.2 Capturar stack trace dos que ICE-am — nenhum ICE reproduzível no estado atual
- [x] 2.3 Identificar commonality — N/A (sem ICE); when.cpp já lida com BlockExpr corretamente em `gen_expr(*arm.body)`

## 3. Root cause & fix
- [x] 3.1 Debuggar `compiler/src/codegen/llvm/control/when.cpp` — linha 1260 usa `gen_expr(*arm.body)` que já trata BlockExpr
- [x] 3.2 Verificar `thir_mir_builder_control.cpp` para MIR lowering — caminho separado, não afetado
- [x] 3.3 Corrigir emission para block bodies — sem fix necessário; ICE reportado pelo UzDB foi corrigido antes desta auditoria
- [x] 3.4 Single-expr e block-body paths geram IR equivalente — validado via tests

## 4. Regression prevention
- [x] 4.1 Adicionar 7 testes ao `compiler/tests/compiler/when_block_body.test.tml`
- [x] 4.2 Atualizar spec de `when` em `docs/specs/` — já documentado; regressão agora coberta por teste

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (entrada no CHANGELOG + docs/patches/v0.3.30.md)
- [x] 5.2 Write tests covering the new behavior (`when_block_body.test.tml` — 7 casos)
- [x] 5.3 Run tests and confirm they pass (1/1 passing — todos 7 casos OK)
