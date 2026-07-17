## 1. Audit & Helper
- [x] 1.1 Mapear call-sites de `WriteConsole*`, `SetConsoleTextAttribute`, `GetStdHandle` em compiler/src/
- [x] 1.2 Criar `compiler/src/cli/tty_output.hpp` e `.cpp` com `tml_out`/`tml_err`/`tml_out_color`
- [x] 1.3 Adicionar `TML_NO_COLOR` env var e flags `--no-color` / `--non-interactive`

## 2. Substituição de output
- [x] 2.1 Substituir chamadas em `compiler/src/cli/cmd_check.cpp` (não aplicável — usa `std::cerr`)
- [x] 2.2 Substituir chamadas em `compiler/src/cli/cmd_run.cpp` (não aplicável — usa `std::cerr`)
- [x] 2.3 Substituir chamadas em `compiler/src/cli/cmd_build.cpp` (não aplicável — usa `std::cerr`)
- [x] 2.4 Substituir chamadas em `compiler/src/cli/cmd_test.cpp` e `cmd_compile.cpp` (VT setup best-effort)
- [x] 2.5 Substituir chamadas em `compiler/src/frontend/main_frontend.cpp` (root cause era pipe-buffer deadlock)

## 3. ANSI Colors & Flush
- [x] 3.1 Substituir `SetConsoleTextAttribute` por códigos ANSI (já era ANSI; VT enablement continua)
- [x] 3.2 Strip automático de ANSI quando não-TTY (`strip_ansi` em `tty_output.cpp`)
- [x] 3.3 Adicionar `fflush(stdout); fflush(stderr);` em todos os paths de exit (via `atexit` handler)
- [x] 3.4 Desabilitar spinner/progress bar em modo não-TTY (não há — `setvbuf(_IOLBF)` resolve o hang)

## 4. Testes
- [x] 4.1 Criar `compiler/tests/cli/tty_output_test.cpp` + `pipe_output.sh`
- [x] 4.2 Teste: `tml check valid.tml > out.txt` retorna 0 em <2s com output correto
- [x] 4.3 Teste: `tml check invalid.tml 2>err.txt` retorna !=0 com erros em err.txt
- [x] 4.4 Teste: `tml run hello.tml > stdout.txt` imprime em stdout.txt (validado via smoke test)
- [x] 4.5 Teste: `TML_NO_COLOR=1` remove escape codes

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (`docs/patches/v0.3.30.md` + CHANGELOG.md)
- [x] 5.2 Write tests covering the new behavior (gtest `tty_output_test.cpp` + bash `pipe_output.sh`)
- [x] 5.3 Run tests and confirm they pass (6/6 pipe_output.sh + smoke + single test OK)
