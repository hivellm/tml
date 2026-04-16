## 1. Audit
- [ ] 1.1 Grep em `compiler/src/` por `fprintf(stdout` e `std::cout` em paths de erro
- [ ] 1.2 Listar todos os emission paths de diagnósticos (errors, warnings, info)
- [ ] 1.3 Verificar se `diagnostic_emitter.cpp` já existe ou precisa ser criado

## 2. Centralização
- [ ] 2.1 Forçar todas as emissões de erro via `diagnostic_emitter` → `stderr`
- [ ] 2.2 Usar `tml_err(...)` do helper TTY criado em phase0r
- [ ] 2.3 `println()` do usuário permanece em stdout; diagnósticos em stderr
- [ ] 2.4 `--format=json` emite diagnósticos estruturados em stderr

## 3. Níveis de log
- [ ] 3.1 Respeitar `TML_LOG_LEVEL=TRACE|DEBUG|WARN|ERROR`
- [ ] 3.2 Por padrão: WARN e ERROR vão para stderr, INFO apenas se verbose

## 4. Testes
- [ ] 4.1 Teste: `tml check invalid.tml 2>err.txt >out.txt` → out.txt vazio, err.txt com erros
- [ ] 4.2 Teste: `tml check valid.tml 2>err.txt` → err.txt vazio, exit 0
- [ ] 4.3 Teste: JSON mode preserva estrutura em stderr
- [ ] 4.4 Integração com MCP — verificar captura via subprocess pipe

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Atualizar `docs/patches/v0.3.30.md` (ou próxima) com mudança
- [ ] 5.2 Escrever testes cobrindo stderr/stdout separation
- [ ] 5.3 Rodar suíte completa, zero regressões
