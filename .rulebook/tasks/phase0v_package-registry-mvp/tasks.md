## 1. tml.toml extension
- [ ] 1.1 Parser de `[dependencies]` em `compiler/src/config/tml_toml.cpp`
- [ ] 1.2 Suportar `{ git = "<url>", tag/rev = "..." }`
- [ ] 1.3 Suportar `{ path = "<local>" }`
- [ ] 1.4 Validação: url válida, tag ou rev obrigatório para git deps

## 2. tml.lock
- [ ] 2.1 Format design (TOML) com resolved commit hashes
- [ ] 2.2 Geração automática após `install`/`update`
- [ ] 2.3 Integridade via SHA-256 hash
- [ ] 2.4 Commit do lock recomendado (gitignore: não)

## 3. CLI commands
- [ ] 3.1 `tml add <url>@<tag>` — edita tml.toml, clona, atualiza lock
- [ ] 3.2 `tml install` — clona deps faltantes conforme tml.lock
- [ ] 3.3 `tml update` — re-resolve + atualiza lock
- [ ] 3.4 `tml remove <name>` — edita tml.toml, deleta .tml-deps/<name>

## 4. Dep storage
- [ ] 4.1 Criar `.tml-deps/` na raiz do projeto
- [ ] 4.2 Clone shallow (`--depth 1 --branch <tag>`) por performance
- [ ] 4.3 Gitignore automático de `.tml-deps/`
- [ ] 4.4 Cache global opcional (env var `TML_DEPS_CACHE`)

## 5. Build integration
- [ ] 5.1 Compiler lê `.tml-deps/<name>/lib/` como module path adicional
- [ ] 5.2 Imports `use <dep>::...` resolvem corretamente
- [ ] 5.3 Meta binário cache por dep (reutiliza mecanismo existente)
- [ ] 5.4 Dep transitiva: dep A depende de dep B → carrega B também

## 6. Testes
- [ ] 6.1 E2E: criar projeto, `tml add` uma repo real, compilar
- [ ] 6.2 Lockfile reproduzível: 2 máquinas diferentes → mesmo clone
- [ ] 6.3 Path deps funcionam para lib local em monorepo
- [ ] 6.4 Erro de dep faltante tem mensagem útil

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 7.1 Criar `docs/package-management.md` com guia completo
- [ ] 7.2 Testes cobrindo add/install/update/remove
- [ ] 7.3 Rodar suíte completa, zero regressões
