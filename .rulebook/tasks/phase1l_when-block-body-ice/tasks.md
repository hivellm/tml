## 1. Reproduction suite
- [ ] 1.1 Escrever teste: block body com múltiplas statements em fn return position
- [ ] 1.2 Escrever teste: block body em closure return
- [ ] 1.3 Escrever teste: nested `when` com block bodies
- [ ] 1.4 Escrever teste: when em expression position com blocks
- [ ] 1.5 Escrever teste: when arms com guards + block body
- [ ] 1.6 Escrever teste: Or-patterns com block body
- [ ] 1.7 Escrever teste: enum payload destructuring + block body
- [ ] 1.8 Escrever teste: when dentro de loop body
- [ ] 1.9 Escrever teste: when dentro de if/else body
- [ ] 1.10 Escrever teste: last expression = tuple/struct literal em block body

## 2. Trigger isolation
- [ ] 2.1 Rodar cada teste com `--emit-ir` e `--debug-mir`
- [ ] 2.2 Capturar stack trace dos que ICE-am
- [ ] 2.3 Identificar commonality (phi? type inference? last expr?)

## 3. Root cause & fix
- [ ] 3.1 Debuggar `compiler/src/codegen/llvm/control/when.cpp`
- [ ] 3.2 Verificar `thir_mir_builder_control.cpp` para MIR lowering
- [ ] 3.3 Corrigir emission para block bodies
- [ ] 3.4 Verificar que single-expr e block-body paths geram IR equivalente

## 4. Regression prevention
- [ ] 4.1 Adicionar todos os 10+ testes ao `compiler/tests/compiler/`
- [ ] 4.2 Atualizar spec de `when` em `docs/specs/` com exemplos

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Atualizar spec + CHANGELOG
- [ ] 5.2 Testes cobrindo todos os padrões
- [ ] 5.3 Rodar suíte completa, zero regressões
