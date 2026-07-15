# Proposal: phase0v_package-registry-mvp

## Why

TML não tem package registry. Se algo não está na stdlib, o usuário precisa
reimplementar do zero. Comparação:

| Ecossistema | Registry |
|-------------|----------|
| Rust | crates.io |
| Go | go modules (git-based) |
| Node | npm |
| Python | PyPI |
| TML | **nada** |

UzDB reportou que teve que escrever ~400 linhas de MessagePack manualmente
porque `std::msgpack` não existia. Mesmo após adicionarmos `std::msgpack` e
`std::protobuf`, o problema estrutural permanece: qualquer lib nova força
reimplementação em casa.

Source: UzDB feedback letter, P2-6.

## What Changes

**Escopo MVP:** git-based deps. Sem web registry, sem semver resolver,
sem publishing. Só clonar + build.

1. **tml.toml** ganha seção `[dependencies]`:
   ```toml
   [dependencies]
   msgpack = { git = "https://github.com/user/tml-msgpack", tag = "v1.0.0" }
   proto = { git = "https://github.com/user/tml-proto", rev = "abc123" }
   local_lib = { path = "../local_lib" }
   ```

2. **CLI**:
   - `tml add <git-url>@<tag>` → adiciona ao tml.toml, clona em `.tml-deps/`
   - `tml update` → refresca deps (respeitando tml.lock)
   - `tml install` → clona deps que estão em tml.toml mas não em disco
   - `tml remove <name>` → remove do tml.toml e disco

3. **tml.lock** (gerado):
   - Resolved versions (git commit hashes)
   - Integrity hash (SHA-256 do tarball)
   - Ordering determinístico

4. **Build integration**:
   - Compiler lê `.tml-deps/<name>/lib/` como module path adicional
   - Imports `use msgpack::...` resolvem via deps path
   - Cache de meta binário por dep

5. **Limitações do MVP** (não-objetivos):
   - Sem web registry próprio
   - Sem resolução de versões semver (usa exact tag/rev)
   - Sem publishing
   - Sem features flags
   - Sem workspaces
   - (Tudo isso vira phase2+ depois)

## Impact

- Affected specs: package-management (novo)
- Affected code: `compiler/src/cli/cmd_add.cpp` (novo), `compiler/src/cli/cmd_install.cpp` (novo), `compiler/src/config/tml_toml.cpp` (extensão), `compiler/src/loader/` (module path)
- Breaking change: NO (tml.toml existente permanece válido)
- User benefit: Desbloqueia ecossistema. Usuários podem compartilhar libs sem esperar stdlib. P2.
