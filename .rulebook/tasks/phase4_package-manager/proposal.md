# Proposal: Package Manager — CLI + Registry Integration

## Why
The TML package manager CLI needs registry integration to enable developers to discover, install, publish, and audit packages. Currently only path dependencies work — registry, git, and version dependencies are stubs. The registry site (package.tml-lang.org) is being built in the tml-docs repo; this task covers the compiler-side CLI commands.

## What Changes
- Add HTTP client to compiler for registry API calls
- Implement `tml login` (GitHub OAuth via browser, token stored in ~/.tml/credentials.toml)
- Implement `tml publish` (index-only: sends repo URL + version + path to registry, no file upload)
- Implement `tml audit` (sends dependency list to registry, displays vulnerabilities)
- Implement `tml search` (query registry, display results)
- Complete `tml add` for registry packages (query registry -> sparse git checkout -> cache -> build rlib)
- Complete `resolve_version_dependency()` and `resolve_git_dependency()` in dependency_resolver.cpp
- Add semver resolution (^, ~, >=, =)
- Add workspace/monorepo support ([workspace] in tml.toml, `tml publish --workspace`)
- Add `tml update`, `tml lock`, lockfile verification

## Existing Infrastructure
- `compiler/src/cli/commands/cmd_pkg.cpp` — tml deps, tml remove, tml add (path only)
- `compiler/src/cli/builder/dependency_resolver.cpp` — full resolver with lockfile, cycle detection, topological sort
- `compiler/src/cli/builder/build_config.cpp` — manifest parsing (tml.toml)

## Impact
- Affected specs: docs/specs/19-MANIFEST.md (workspace section), docs/specs/17-FFI.md
- Affected code: compiler/src/cli/commands/cmd_pkg.cpp, dependency_resolver.cpp, new files for registry client, credentials, git ops
- Breaking change: NO (extends existing commands, adds new ones)
- User benefit: Full package ecosystem — install, publish, audit from CLI

## Architecture
- **Index-only registry** — package.tml-lang.org stores metadata only, not source code
- **Git-based packages** — source lives in Git repos, installed via sparse checkout
- **Monorepo support** — single repo can host multiple packages (e.g., TML's own lib/postgresql/, lib/ia/)
- **Token auth** — GitHub OAuth -> API token stored locally

## Phases
1. HTTP client infrastructure
2. Credentials and tml login
3. Git operations (clone, sparse checkout, cache)
4. tml add with registry integration
5. tml publish (index registration)
6. tml audit
7. tml search
8. Workspace/monorepo support
9. Update and lockfile management

## References
- Registry Design: tml-docs/docs/analyses/site/02-package-registry-design.md
- Site Strategic Plan: tml-docs/docs/analyses/site/00-strategic-plan.md
