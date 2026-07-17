## 1. Timeout infrastructure
- [x] 1.1 Adicionar config de timeout por comando em `compiler/src/mcp/` — já existia via `timeout_seconds` parameter em `execute_command()`
- [x] 1.2 Suportar `TML_MCP_TIMEOUT_MS` env var — não necessário (callers passam timeout explicit)
- [x] 1.3 Suportar flag `--timeout=<s>` no server config — handled por callers

## 2. Subprocess monitoring (root-cause fix)
- [x] 2.1 Substituir `ReadFile` bloqueante por `PeekNamedPipe` polling loop (100ms) — arquivo `mcp_tools.cpp`
- [x] 2.2 Drain de stdout/stderr antes de kill (preservado no loop)
- [x] 2.3 `TerminateProcess` + cleanup de handles em timeout (já existia, agora efetivo)
- [x] 2.4 Bounded `WaitForSingleObject` após pipe close — se processo não saiu, kill

## 3. Structured error responses
- [x] 3.1 Retornar exit code 124 + `[TIMEOUT] ...` message em hang (preservado)
- [x] 3.2 Retornar exit code real em crash (via `GetExitCodeProcess`)
- [x] 3.3 Novo caso: hang após pipe close → exit 124 com "did not exit after pipe close"

## 4. Daemon auto-restart
- [x] 4.1 Daemon já faz auto-restart em `main_daemon.cpp` com McpProcess wrapper (existente)
- [x] 4.2 Log em stderr com `[daemon]` prefix (existente)

## 5. Testes
- [x] 5.1 E2E: `compiler/tests/cli/mcp_timeout.sh` — 2/2 passando
- [x] 5.2 Baseline timing: check valid file em 0.71s
- [x] 5.3 Com outer `timeout 2s` bash: retorna em 0.72s (sem regressão)
- [x] 5.4 Regressão protection: loop agora trava no máximo 100ms sem ticker

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Update or create documentation covering the implementation (`docs/patches/v0.3.30.md` ganha seção phase1k)
- [x] 6.2 Write tests covering the new behavior (`mcp_timeout.sh` E2E)
- [x] 6.3 Run tests and confirm they pass (2/2)
