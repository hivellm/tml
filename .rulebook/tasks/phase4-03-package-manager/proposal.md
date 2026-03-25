# Proposal: TML Package Manager

## Status: IN_PROGRESS (15% — Phase 1 partially implemented, Phases 2-5 blocked on registry)

## Summary

A complete package management system for TML covering dependency declaration (`tml.toml`), resolution, locking, fetching (git and registry), workspace support, and publishing. The CLI commands are `tml add`, `tml remove`, `tml update`, `tml deps`, `tml publish`, and `tml search`. The design is modeled on Cargo (Rust) with lessons from npm.

The foundation (manifest parsing, path dependencies, lockfile, `tml deps`, `tml remove`) is already implemented. Phase 1 (git dependencies) can proceed independently. Phases 2-5 are blocked on a TML package registry service that does not yet exist.

## Motivation

Without a package manager, TML projects can only use the standard library or copy source files manually. This is the single biggest barrier to ecosystem growth. No ecosystem means no community, no community means no adoption, no adoption means the language fails regardless of technical quality.

Git dependencies (Phase 1) unlock the immediate use case: sharing TML libraries between projects via GitHub before a formal registry exists. This is how Rust's early ecosystem worked (Cargo predated crates.io).

## Design

**Manifest format** (`tml.toml`): TOML-based, following Cargo conventions. Sections: `[package]` (name, version, authors), `[dependencies]` (path, git, or registry references), `[test]`, `[build]`, `[compiler]`.

**Dependency resolution**: Semver-compatible version constraint solving. Path deps are resolved relative to the manifest. Git deps clone to a local cache directory (`~/.tml/registry/git/`). Registry deps download from the TML registry API.

**Lockfile** (`tml.lock`): Records exact resolved versions and checksums for reproducible builds. Format mirrors Cargo.lock.

**Git dependencies** (Phase 1): Clone via `git clone --depth 1` (or libgit2 bindings). Support `branch`, `tag`, `rev` specifiers. Cache in `~/.tml/registry/git/{repo-hash}/`. Invalidate cache on `rev` change or explicit `tml update`.

**Registry** (Phase 2+): REST API with endpoints for upload, download, version resolution, and search. Authentication via token stored in `~/.tml/credentials`. Publishing validates the manifest, runs `tml build`, and uploads a `.rlib` archive.

**Workspace** (Phase 4): A root `tml.toml` with `[workspace]` section listing member paths. Shared dependency resolution across all members. `tml build -w` builds all workspace members.

## What Changes

Already implemented:
- `compiler/src/cli/dependency_resolver.hpp` and `.cpp` — DependencyResolver, Lockfile
- `compiler/src/cli/cmd_pkg.hpp` and `.cpp` — tml add/remove/update/deps/publish stubs
- `compiler/src/cli/build_config.hpp` and `.cpp` — manifest parser

Remaining:
- Phase 1: git clone logic in `dependency_resolver.cpp`
- Phase 2: registry service (separate deployment, not compiler code)
- Phase 3: registry client HTTP calls in `cmd_pkg.cpp`
- Phase 4: workspace discovery and build coordination in `builder/`
- Phase 5: audit, vendor, tree subcommands in `cmd_pkg.cpp`

## Dependencies

- Depends on: `tml.toml` manifest format (already implemented)
- Depends on: `compiler/src/cli/rlib.hpp` RLIB format for publishing
- Phase 2+ depends on: TML registry service (not planned near-term)
- Enables: TML ecosystem growth, library sharing, `tml doc` package documentation

## Risks

- The registry service is infrastructure that requires hosting, maintenance, and abuse prevention — it is a non-trivial operational commitment; Phases 2-5 should not be started until the registry hosting plan is decided
- Git dependency caching must handle concurrent `tml build` calls (from parallel CI) without corruption; a lock file per repo cache entry is needed
- Semver resolution with conflicts (two packages require incompatible versions of the same dependency) requires a backtracking solver; the initial implementation may use a greedy approach and error on conflicts
