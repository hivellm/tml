## 1. Type inference plumbing
- [ ] 1.1 Adicionar `expected_type` hint em `check_expr` para literais
- [ ] 1.2 Propagar hint a partir de struct field types
- [ ] 1.3 Propagar hint a partir de function param types
- [ ] 1.4 Propagar hint a partir de return type
- [ ] 1.5 Propagar hint em binary ops (um lado conhecido → outro infere)

## 2. Range validation
- [ ] 2.1 Checar range do literal vs tipo alvo (U8: 0-255, U32: 0-4B, etc.)
- [ ] 2.2 Erro de compilação com diagnóstico útil em overflow
- [ ] 2.3 Suportar hex/binary/octal literais (0xFF vira U8 em contexto)

## 3. Codegen adjust
- [ ] 3.1 Verificar que literal emite LLVM IR com tipo correto
- [ ] 3.2 Sem zext/trunc desnecessário

## 4. Testes
- [ ] 4.1 Struct literal: campos U8/U16/U32/U64/I8/I16/I32
- [ ] 4.2 Range error: `{ x: 300 }` em U8
- [ ] 4.3 Tuple com tipo anotado
- [ ] 4.4 Function call args
- [ ] 4.5 Return inference
- [ ] 4.6 Binary ops mistos (I64 default + struct field U32)
- [ ] 4.7 Array com tipo declarado: `[U8, 3] = [1, 2, 3]`

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Atualizar spec de type inference + CHANGELOG
- [ ] 5.2 Testes cobrindo todos os cenários
- [ ] 5.3 Rodar suíte completa, zero regressões
