# Tasks: TML Package Manager

## Overview

This task covers the implementation of a complete package management system for TML,
similar to Cargo (Rust), npm (Node.js), or pip (Python).

**Status**: Partial implementation - waiting for registry service

**Blocking Dependency**: Requires a TML package registry service (similar to crates.io)

## Current State

### Already Implemented ✅
- [x] `tml.toml` manifest format (package/test/build/compiler sections)
- [x] Manifest parser (SimpleTomlParser in build_config.cpp)
- [x] Dependency resolution for path dependencies (DependencyResolver class)
- [x] Lockfile support (Lockfile class in dependency_resolver.cpp)
- [x] `tml deps` command - lists project dependencies
- [x] `tml remove` command - removes dependency from tml.toml
- [x] Stubs for `tml add`, `tml update`, `tml publish` with helpful error messages

### Files Created
- `compiler/src/cli/dependency_resolver.hpp` - DependencyResolver, Lockfile classes
- `compiler/src/cli/dependency_resolver.cpp` - Implementation
- `compiler/src/cli/cmd_pkg.hpp` - Package command declarations
- `compiler/src/cli/cmd_pkg.cpp` - Package command implementations

## Phase 1: Git Dependencies
Enable fetching dependencies from Git repositories without a registry.

- [ ] 1.1 Implement git clone for dependencies
- [ ] 1.2 Support git URL in tml.toml: `mylib = { git = "https://..." }`
- [ ] 1.3 Support branch/tag/rev specification: `{ git = "...", branch = "main" }`
- [ ] 1.4 Implement git dependency caching
- [ ] 1.5 Update lockfile format for git dependencies
- [ ] 1.6 Handle git authentication (SSH keys, tokens)

## Phase 2: Package Registry Service
Create a centralized package registry for TML packages.

- [ ] 2.1 Design registry API (REST/GraphQL)
- [ ] 2.2 Implement package upload endpoint
- [ ] 2.3 Implement package download endpoint
- [ ] 2.4 Implement version resolution endpoint
- [ ] 2.5 Implement search functionality
- [ ] 2.6 Add authentication/authorization
- [ ] 2.7 Implement rate limiting
- [ ] 2.8 Add package validation (manifest check, build test)
- [ ] 2.9 Deploy registry service (hosting TBD)

## Phase 3: Registry Client
Implement CLI commands that interact with the registry.

- [ ] 3.1 Implement registry client library
- [ ] 3.2 Implement `tml add <package>` - fetch from registry
- [ ] 3.3 Implement `tml add <package>@<version>` - specific version
- [ ] 3.4 Implement `tml update` - update to latest compatible versions
- [ ] 3.5 Implement `tml update <package>` - update specific package
- [ ] 3.6 Implement `tml publish` - upload to registry
- [ ] 3.7 Implement `tml search <query>` - search packages
- [ ] 3.8 Implement `tml info <package>` - show package info
- [ ] 3.9 Add `tml login` / `tml logout` for authentication

## Phase 4: Workspace Support
Support multi-package projects (monorepos).

- [ ] 4.1 Design workspace manifest format
- [ ] 4.2 Implement workspace discovery
- [ ] 4.3 Implement shared dependency resolution
- [ ] 4.4 Implement `tml build --workspace` / `-w`
- [ ] 4.5 Implement `tml test --workspace`
- [ ] 4.6 Support inter-workspace dependencies
- [ ] 4.7 Add workspace-level scripts

## Phase 5: Advanced Features
Additional features for mature package management.

- [ ] 5.1 Implement `tml audit` - security vulnerability check
- [ ] 5.2 Implement `tml outdated` - show outdated dependencies
- [ ] 5.3 Implement `tml tree` - full dependency tree visualization
- [ ] 5.4 Implement `tml vendor` - vendor dependencies locally
- [ ] 5.5 Add private registry support
- [ ] 5.6 Implement dependency overrides/patches
- [ ] 5.7 Add pre/post install hooks

## Current Workarounds

Until the registry is available, users can:

1. **Path Dependencies** (recommended for local development):
   ```toml
   [dependencies]
   mylib = { path = "../mylib" }
   ```

2. **Git Dependencies** (once Phase 1 is complete):
   ```toml
   [dependencies]
   mylib = { git = "https://github.com/user/mylib" }
   ```

## Related Files

- `compiler/src/cli/build_config.hpp` - Manifest, Dependency structs
- `compiler/src/cli/build_config.cpp` - SimpleTomlParser
- `compiler/src/cli/dependency_resolver.hpp` - DependencyResolver
- `compiler/src/cli/dependency_resolver.cpp` - Resolution logic
- `compiler/src/cli/cmd_pkg.hpp` - Command declarations
- `compiler/src/cli/cmd_pkg.cpp` - Command implementations
- `compiler/src/cli/rlib.hpp` - RLIB format for compiled libraries
