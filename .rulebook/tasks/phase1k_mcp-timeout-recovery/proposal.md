# Proposal: phase1k_mcp-timeout-recovery

## Why

O MCP server (`tml mcp`) spawna `tml.exe` como subprocesso e lê stdout via pipe.
Quando `tml.exe` trava (por pipe hang, ICE, ou deadlock), o MCP:

- Não tem timeout: trava indefinidamente
- Eventualmente crasha sem retornar resposta estruturada
- Precisa ser reiniciado externamente (VS Code ou shell)
- Chamadas em progresso são perdidas
- AI agents sem acesso a TTY não conseguem reiniciar o MCP

Mesmo após `phase0r` fixar o pipe hang, qualquer ICE ou deadlock futuro vai
gerar a mesma experiência ruim. O MCP precisa ser resiliente a qualquer
falha do subprocesso.

Source: UzDB feedback letter, P0-3.

## What Changes

1. **Timeout configurável** em `tml mcp`:
   - Default: 30s para `check`, 60s para `build`, 120s para `test`
   - Env var `TML_MCP_TIMEOUT_MS` para override
   - Flag `--timeout=<s>` no server config

2. **Kill + response estruturada**:
   - Em timeout: `TerminateProcess()` do subprocess
   - Drain de stdout/stderr antes do kill (o que já foi emitido)
   - Retornar `{"error":"compilation_timeout","pid":<n>,"partial_output":"..."}`

3. **Health check do subprocesso**:
   - Thread monitor checa `exit_code` a cada 100ms
   - Se processo morreu sem output: retornar `{"error":"subprocess_crashed"}`

4. **Restart automático** do daemon em caso de corrupção:
   - Se 3 calls consecutivos falham, reinicia o daemon
   - Loga para `.tml/mcp.log`

5. **Testes**:
   - Mock subprocess que dorme 60s → MCP retorna timeout em 30s
   - Mock subprocess que crasha → MCP retorna erro estruturado
   - Mock subprocess que emite output parcial e trava → MCP captura parcial + timeout

## Impact

- Affected specs: compiler/mcp
- Affected code: `compiler/src/mcp/*.cpp`, `compiler/src/daemon/*.cpp`
- Breaking change: NO (API JSON do MCP estendida, não quebrada)
- User benefit: MCP server resiliente. Nunca trava. Sempre retorna resposta estruturada. Desbloqueia uso confiável por AI agents. P0 CRITICAL.
