# Proposal: phase1j_diagnostics-to-stderr

## Why

Mensagens de erro do compilador (type errors, parse errors, codegen errors)
são escritas no buffer do console do Windows em vez de `stderr`. O exit code
fica correto (1), mas nenhum output é capturado via pipe. Consequências:

- AI agents não conseguem ler erros programaticamente
- MCP retorna `"Type check failed"` sem detalhes
- Loops automáticos de fix-error são impossíveis sem humano lendo o console
- `tml check invalid.tml 2>err.txt` produz err.txt vazio

Complementa a `phase0r` (pipe hang geral): enquanto aquela corrige o deadlock,
esta garante que diagnósticos fluam pelo canal correto (stderr) para
ferramentas e agentes conseguirem parsear.

Source: UzDB feedback letter, P0-2.

## What Changes

1. Auditar `compiler/src/diagnostics/` e identificar todos os emission paths
2. Centralizar emissão em `diagnostic_emitter.cpp` → sempre `stderr`
3. Nunca usar `fprintf(stdout, ...)` para diagnósticos (errors/warnings)
4. `println` do usuário continua em `stdout`, diagnósticos em `stderr`
5. Respeitar níveis `TML_LOG_TRACE/DEBUG/WARN/ERROR` via env var
6. JSON mode (`--format=json`) emite diagnósticos estruturados em `stderr` ainda
7. Teste: `tml check invalid.tml 2>err.txt >out.txt` escreve erros em err.txt, out.txt vazio

## Impact

- Affected specs: compiler/diagnostics
- Affected code: `compiler/src/diagnostics/*.cpp`, `compiler/src/cli/cmd_check.cpp`
- Breaking change: NO (exit code e formato de mensagem inalterados)
- User benefit: Permite fix-error loops automatizados e parsing de erro confiável por MCP/CI/AI agents. P0 CRITICAL.
