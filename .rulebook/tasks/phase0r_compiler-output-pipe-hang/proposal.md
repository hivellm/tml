# Proposal: phase0r_compiler-output-pipe-hang

## Why

`tml check`, `tml run`, `tml build` travam indefinidamente quando stdout/stderr
são redirecionados para um pipe (não-TTY). Causa: uso de `WriteConsoleW` /
handles Win32 em vez de `WriteFile`/`fprintf`. Resultado:

- `tml check file.tml > output.txt` trava para sempre
- MCP server (que spawna `tml.exe` e lê via pipe) não recebe output e crasha
- CI/CD, scripts e workflows de AI agents completamente bloqueados
- `tml lint` funciona (usa printf/WriteFile) — confirma que o problema é
  localizado nos paths que usam console API

Este é o bloqueador #1 reportado pelo UzDB. Sem esse fix, TML é inutilizável
em qualquer contexto não-interativo, incluindo o próprio MCP server que TML
envia.

Source: UzDB feedback letter, P0-1.

## What Changes

1. Criar helper `compiler/src/cli/tty_output.{hpp,cpp}` com detecção via `_isatty(_fileno(stdout/stderr))`, API `tml_out`/`tml_err`, fallback para `fprintf` quando não-TTY, respeita `TML_NO_COLOR=1`
2. Auditar e substituir todos os `WriteConsole*` / `SetConsoleTextAttribute` em `compiler/src/cli/`, `compiler/src/diagnostics/`, `compiler/src/frontend/main*.cpp`
3. Substituir cores diretas por códigos ANSI (`\x1b[31m` etc.) — Win10+ cmd.exe interpreta
4. Adicionar flags `--no-color` e `--non-interactive`
5. Adicionar `fflush(stdout); fflush(stderr);` em todos os exit paths
6. Teste de regressão: spawna `tml check file.tml > out.txt 2>err.txt`, verifica exit em <2s com conteúdo correto

## Impact

- Affected specs: compiler/cli
- Affected code: `compiler/src/cli/*.cpp`, `compiler/src/diagnostics/*.cpp`, `compiler/src/frontend/main*.cpp`
- Breaking change: NO (comportamento em TTY inalterado)
- User benefit: **Desbloqueia MCP server, CI/CD e AI agents completamente**. Prioridade P0 CRITICAL reportada pelo UzDB.
