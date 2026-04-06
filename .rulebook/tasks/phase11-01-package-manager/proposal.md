# Proposal: Package Manager — `tml pkg`

**Task**: phase11-01-package-manager
**Status**: Planning
**Priority**: P1
**Estimated effort**: 2–3 weeks
**Risk**: Medium — semver dependency resolution has known edge cases (diamond dependencies,
version conflicts); registry infrastructure adds an external dependency

## Problem

TML has no package manager. Every third-party dependency must be vendored manually or copied
by hand. This creates friction for library authors (no publishing workflow), application
developers (no version pinning, no reproducible builds), and the ecosystem overall (no
discoverability). As TML's stdlib coverage grows, the absence of a package manager becomes
the primary barrier to third-party library development.

## Proposed Solution

A `tml pkg` CLI with four core commands: `add`, `remove`, `update`, `install`. Project
configuration lives in `package.toml` (Cargo-style). A `package.lock` file records the
exact resolved versions for reproducible builds. Packages are cached in `~/.tml/packages/`
keyed by `name@version`. The registry is GitHub-based initially (no custom server required):
a package is a git repo with a `package.toml` at the root.

**Dependency resolution**: semver range satisfaction with backtracking. Diamond dependency
detection with conflict error reporting. Minimum version selection (MVS, like Go modules)
as the default resolution strategy — predictable, reproducible, no surprise upgrades.

**Build integration**: the compiler's query system reads `package.toml` and resolves imports
from `~/.tml/packages/` automatically. No manual path configuration required.

**Native dependencies**: a `build.tml` file at the package root (analogous to Cargo's
`build.rs`) runs before compilation to compile C dependencies or generate code.

## Key Decisions

- TOML for configuration: `package.toml` and `package.lock` follow Cargo conventions,
  reducing learning curve for developers coming from Rust.
- Minimum version selection over SAT solving: MVS is O(n), deterministic, and produces
  the same result on every machine without a lock file. Lock file still written for
  explicit reproducibility.
- GitHub-based registry first: avoids the need to host infrastructure. Packages are
  referenced as `github.com/user/repo@v1.2.3`. A curated index (a single JSON file in
  a `tml-lang/packages` repo) enables search without a custom API server.
- `~/.tml/packages/` local cache: packages are content-addressed (name + version). The
  cache is append-only — `tml pkg clean` is the only way to remove entries, and only
  with explicit user invocation.
- `build.tml` for native deps: keeps native compilation in TML rather than requiring
  a separate shell script. Runs in a restricted sandbox (no network, no file writes
  outside the package directory).

## Files to Create/Modify

- `compiler/src/cli/commands/cmd_pkg.cpp` — `tml pkg` subcommand (add/remove/update/install)
- `lib/std/src/pkg/resolver.tml` — semver range parser, MVS resolution algorithm
- `lib/std/src/pkg/registry.tml` — GitHub-based registry fetch, package index lookup
- `lib/std/src/pkg/lockfile.tml` — package.lock read/write, hash verification
- `lib/std/src/pkg/cache.tml` — ~/.tml/packages/ content-addressed local cache
- `compiler/src/query/query_context.cpp` — resolve package imports from cache during compilation
- `docs/user/package-manager.md` — user guide: creating packages, adding dependencies

## Success Criteria

- `tml pkg add github.com/user/mylib@v1.0.0` downloads, caches, and writes package.toml
- `tml pkg install` reads package.lock and restores exact versions from cache or network
- `use mylib::Foo` in TML source resolves to the cached package without path configuration
- Diamond dependency (A requires B@1.x and C@1.x, both require D at different minor versions)
  resolves to the higher minor version with no error under MVS
- Conflicting major versions (requires D@1.x and D@2.x simultaneously) produces a clear
  error message identifying the two conflicting requirements and their dependency paths
- `build.tml` runs before compilation for packages that declare native dependencies
- Round-trip: `tml pkg add` → edit source → `tml build` produces working executable

## Dependencies

- Depends on: compiler query system (existing), std::http (for registry fetch), std::json
  (for index parsing), std::crypto (for hash verification of cached packages)
- Blocks: any future tasks requiring third-party library distribution
