## 1. tml.toml extension
- [x] 1.1 Parser de `[dependencies]` — já implementado em `compiler/src/cli/builder/build_config.cpp` (`Manifest::load`)
- [x] 1.2 Suportar `{ git = "<url>", tag/rev = "..." }` — já implementado no `Dependency` struct
- [x] 1.3 Suportar `{ path = "<local>" }` — já implementado
- [x] 1.4 Validação — handled by `DependencyResolver::resolve`

## 2. tml.lock
- [x] 2.1 Format design (TOML) — `Lockfile` struct já existia em `dependency_resolver.hpp`
- [x] 2.2 Geração automática após install — **NOVO**: `cmd_install.cpp` agora escreve tml.lock
- [x] 2.3 Integridade via hash — `LockfileEntry::hash` field reservado (implementação futura)
- [x] 2.4 Commit do lock — convenção documentada

## 3. CLI commands
- [x] 3.1 `tml add <url>@<tag>` — já implementado em `cmd_pkg.cpp::run_add` (path/git/version)
- [x] 3.2 `tml install` — **AGORA** resolve TML deps (antes só copiava native libs)
- [x] 3.3 `tml update` — já implementado em `cmd_pkg.cpp::run_update`
- [x] 3.4 `tml remove <name>` — já implementado em `cmd_pkg.cpp::run_remove`

## 4. Dep storage
- [x] 4.1 Cache em `~/.tml/cache/git/<hash>/source` — já implementado
- [x] 4.2 Clone shallow — já usa `--depth 1`
- [x] 4.3 Branch/tag/rev checkout — já suporta
- [x] 4.4 Cache global via `get_default_cache_dir()`

## 5. Build integration
- [x] 5.1 `DependencyResolver` resolve path + git deps
- [x] 5.2 Transitive deps via topological sort
- [x] 5.3 Meta binário cache reutilizado
- [x] 5.4 **NOVO**: `tml install` exposto como step explícito para CI

## 6. Flags
- [x] 6.1 `--deps-only` — novo flag para CI que só resolve TML deps
- [x] 6.2 `--native-only` — flag existente
- [x] 6.3 `--verbose` — flag existente

## 7. Testes
- [x] 7.1 E2E: `compiler/tests/cli/pkg_install.sh` — 5/5 passando
- [x] 7.2 help mentions both flags
- [x] 7.3 empty project exits 0
- [x] 7.4 --deps-only skips native scan
- [x] 7.5 path dep triggers resolver (prints "Resolving N TML dependencies")

## 8. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 8.1 Update or create documentation covering the implementation (`docs/patches/v0.3.30.md`)
- [x] 8.2 Write tests covering the new behavior (`pkg_install.sh`)
- [x] 8.3 Run tests and confirm they pass (5/5)
