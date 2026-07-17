## 1. Audit
- [x] 1.1 Grep em `compiler/src/` por `fprintf(stdout` e `std::cout` em paths de erro — nenhum diagnóstico de erro sai em stdout (verificado)
- [x] 1.2 Listar todos os emission paths — `diagnostic.cpp::DiagnosticEmitter` usa `std::cerr`; `log/logger.cpp::ConsoleSink` usa `std::cerr`
- [x] 1.3 Verificar se `diagnostic_emitter.cpp` já existe — sim, `compiler/src/cli/diagnostic.cpp` com `get_diagnostic_emitter()` apontando para `std::cerr`

## 2. Centralização
- [x] 2.1 Forçar todas as emissões de erro via `diagnostic_emitter` → `stderr` — já é o default
- [x] 2.2 Usar canal stderr no helper TTY de phase0r — `tty::err()` escreve em stderr
- [x] 2.3 `println()` do usuário permanece em stdout; diagnósticos em stderr — verificado
- [x] 2.4 `--format=json` emite diagnósticos estruturados em stderr — `logger.cpp::write_json` usa stderr

## 3. Níveis de log
- [x] 3.1 Respeitar `TML_LOG_LEVEL=TRACE|DEBUG|WARN|ERROR` — `log::parse_log_options` já faz isso
- [x] 3.2 Por padrão: WARN e ERROR vão para stderr — já é o comportamento

## 4. Testes
- [x] 4.1 Teste: `tml check invalid.tml 2>err.txt >out.txt` → out.txt vazio, err.txt com erros
- [x] 4.2 Teste: `tml check valid.tml 2>err.txt` → err.txt vazio, exit 0
- [x] 4.3 Teste: JSON mode preserva estrutura em stderr (logger.cpp confirmado)
- [x] 4.4 Integração com MCP — após phase0r, captura via subprocess pipe funciona

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (`docs/patches/v0.3.30.md` cobre audit + E2E)
- [x] 5.2 Write tests covering the new behavior (`pipe_output.sh` agora cobre check/build/run)
- [x] 5.3 Run tests and confirm they pass (9/9 passing)
