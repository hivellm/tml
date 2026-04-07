# Tasks: Package Manager — CLI + Registry Integration

**Status**: In Progress. 15% (deps + remove done).
**Reference**: tml-docs/docs/analyses/site/02-package-registry-design.md
**Existing**: cmd_pkg.cpp (deps, remove, add path), dependency_resolver.cpp (lockfile, cycle detection)

## Phase 1: HTTP Client Infrastructure

- [ ] 1.1 Add HTTP client to compiler (libcurl or WinHTTP)
- [ ] 1.2 RegistryClient class (fetch_package, publish, search, audit)
- [ ] 1.3 JSON parsing for API responses
- [ ] 1.4 Registry URL configuration (default: package.tml-lang.org/api/v1)
- [ ] 1.5 Error handling: network, 401/403/404/429, timeouts

## Phase 2: Credentials & Authentication

- [ ] 2.1 Credentials manager (~/.tml/credentials.toml)
- [ ] 2.2 `tml login` — browser OAuth + local callback server
- [ ] 2.3 `tml logout`
- [ ] 2.4 Token validation

## Phase 3: Git Operations

- [ ] 3.1 Git ops: clone_tag, sparse_checkout, fetch_file
- [ ] 3.2 Cache management (~/.tml/cache/<package>/<version>/)
- [ ] 3.3 Git executable detection
- [ ] 3.4 Fallback: GitHub API tarball download

## Phase 4: tml add — Registry Integration

- [ ] 4.1 Update run_add() for registry packages (not just path)
- [ ] 4.2 resolve_version_dependency() — query registry, cache, build rlib
- [ ] 4.3 resolve_git_dependency() — clone at tag, cache, build
- [ ] 4.4 tml.toml format: "0.1.0", { version }, { git }, { path }
- [ ] 4.5 Semver resolution: ^, ~, >=, =
- [ ] 4.6 Lockfile update with registry + git sources
- [ ] 4.7 tml remove — clean cache

## Phase 5: tml publish — Index Registration

- [ ] 5.1 run_publish() — validate tml.toml, send repo URL + version to registry
- [ ] 5.2 --workspace flag for monorepo batch publish
- [ ] 5.3 Pre-publish validation (tml check, version, README, LICENSE)
- [ ] 5.4 --dry-run flag
- [ ] 5.5 tml yank / tml unyank

## Phase 6: tml audit

- [ ] 6.1 run_audit() — send deps to registry, display vulns
- [ ] 6.2 --fix flag (auto-update vulnerable deps)
- [ ] 6.3 --ignore flag

## Phase 7: tml search

- [ ] 7.1 run_search() — query registry, display results
- [ ] 7.2 tml info <package>

## Phase 8: Workspace/Monorepo

- [ ] 8.1 Parse [workspace] in root tml.toml (members + glob)
- [ ] 8.2 tml workspace list / tml workspace build
- [ ] 8.3 Workspace-level shared deps

## Phase 9: Update & Lockfile

- [ ] 9.1 tml update / tml update <package>
- [ ] 9.2 Lockfile generation on first build
- [ ] 9.3 Lockfile verification on build
- [ ] 9.4 tml lock — regenerate lockfile

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
