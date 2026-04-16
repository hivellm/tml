## 1. Timeout infrastructure
- [ ] 1.1 Adicionar config de timeout por comando em `compiler/src/mcp/`
- [ ] 1.2 Suportar `TML_MCP_TIMEOUT_MS` env var
- [ ] 1.3 Suportar flag `--timeout=<s>` no server config

## 2. Subprocess monitoring
- [ ] 2.1 Thread monitor que checa `exit_code` a cada 100ms
- [ ] 2.2 Drain de stdout/stderr em buffer antes de kill
- [ ] 2.3 `TerminateProcess()` em timeout com cleanup de handles

## 3. Structured error responses
- [ ] 3.1 Retornar `{"error":"compilation_timeout","pid":<n>,"partial_output":"..."}` em timeout
- [ ] 3.2 Retornar `{"error":"subprocess_crashed","exit_code":<n>}` em crash
- [ ] 3.3 Retornar `{"error":"subprocess_no_output","timeout_ms":<n>}` em hang sem output

## 4. Daemon auto-restart
- [ ] 4.1 Contar falhas consecutivas por comando
- [ ] 4.2 Após 3 falhas: reiniciar daemon + reabrir pipes
- [ ] 4.3 Log em `.tml/mcp.log` com timestamp e causa

## 5. Testes
- [ ] 5.1 Mock subprocess sleep 60s → timeout em 30s ± 1s
- [ ] 5.2 Mock subprocess crash → erro estruturado
- [ ] 5.3 Mock partial output + hang → captura parcial + timeout
- [ ] 5.4 Stress test: 100 calls consecutivos sem leak de handles

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Atualizar docs MCP (`docs/mcp-server.md` ou similar) com timeouts/errors
- [ ] 6.2 Escrever testes de integração cobrindo timeout/crash/restart
- [ ] 6.3 Rodar suíte completa, zero regressões
