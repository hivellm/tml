## 1. Audit & Helper
- [ ] 1.1 Mapear call-sites de `WriteConsole*`, `SetConsoleTextAttribute`, `GetStdHandle` em compiler/src/
- [ ] 1.2 Criar `compiler/src/cli/tty_output.hpp` e `.cpp` com `tml_out`/`tml_err`/`tml_out_color`
- [ ] 1.3 Adicionar `TML_NO_COLOR` env var e flags `--no-color` / `--non-interactive`

## 2. Substituição de output
- [ ] 2.1 Substituir chamadas em `compiler/src/cli/cmd_check.cpp`
- [ ] 2.2 Substituir chamadas em `compiler/src/cli/cmd_run.cpp`
- [ ] 2.3 Substituir chamadas em `compiler/src/cli/cmd_build.cpp`
- [ ] 2.4 Substituir chamadas em `compiler/src/cli/cmd_test.cpp` e `cmd_compile.cpp`
- [ ] 2.5 Substituir chamadas em `compiler/src/frontend/main_frontend.cpp`

## 3. ANSI Colors & Flush
- [ ] 3.1 Substituir `SetConsoleTextAttribute` por códigos ANSI
- [ ] 3.2 Strip automático de ANSI quando não-TTY
- [ ] 3.3 Adicionar `fflush(stdout); fflush(stderr);` em todos os paths de exit
- [ ] 3.4 Desabilitar spinner/progress bar em modo não-TTY

## 4. Testes
- [ ] 4.1 Criar `compiler/tests/cli/pipe_output.test.cpp`
- [ ] 4.2 Teste: `tml check valid.tml > out.txt` retorna 0 em <2s com output correto
- [ ] 4.3 Teste: `tml check invalid.tml 2>err.txt` retorna !=0 com erros em err.txt
- [ ] 4.4 Teste: `tml run hello.tml > stdout.txt` imprime em stdout.txt
- [ ] 4.5 Teste: `TML_NO_COLOR=1` remove escape codes

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Atualizar documentação (`docs/patches/v0.3.30.md` + CHANGELOG.md)
- [ ] 5.2 Escrever testes cobrindo o novo comportamento
- [ ] 5.3 Rodar suíte completa e confirmar zero regressões
