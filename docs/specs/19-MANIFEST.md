# 19. Package Manifest (tml.toml)

## Overview

The `tml.toml` file is the package manifest for TML projects. It defines:
- Package metadata (name, version, authors)
- Build configuration (library vs binary)
- Dependencies
- Build settings

Similar to `Cargo.toml` for Rust or `package.json` for Node.js, `tml.toml` provides a declarative way to configure TML projects without command-line flags.

## File Location

The `tml.toml` file must be in the project root:

```
my_project/
├── tml.toml          ← Package manifest
├── src/
│   └── main.tml
└── build/
    └── debug/
```

## Basic Structure

```toml
[package]
name = "my_project"
version = "0.1.0"
authors = ["Jane Doe <jane@example.com>"]
edition = "2024"

[lib]
path = "src/lib.tml"
crate-type = ["rlib"]

[[bin]]
name = "my_app"
path = "src/main.tml"

[dependencies]
math_lib = "1.0.0"
utils = { path = "../utils" }

[build]
optimization-level = 2
emit-ir = false
emit-header = false
```

## Section Reference

### `[package]` - Required

Defines package metadata.

**Fields:**

```toml
[package]
name = "my_package"           # Package name (required)
version = "1.0.0"              # Semver version (required)
authors = ["Author Name <email@example.com>"]  # Optional
edition = "2024"               # TML edition (optional, default: 2024)
description = "A sample TML package"  # Optional
license = "MIT"                # Optional
repository = "https://github.com/user/repo"  # Optional
```

**Constraints:**
- `name`: Must be lowercase, alphanumeric, hyphens/underscores allowed
- `version`: Must follow Semantic Versioning (MAJOR.MINOR.PATCH)
- `edition`: Currently only "2024" is supported

### `[lib]` - Optional

Configures library output.

**Fields:**

```toml
[lib]
path = "src/lib.tml"           # Library entry point (default: src/lib.tml)
crate-type = ["rlib", "lib"]   # Output formats (default: ["rlib"])
name = "my_lib"                # Library name (default: package name)
emit-header = true             # Generate C header (default: false)
```

**Crate Types:**
- `"rlib"`: TML native library format (.rlib)
- `"lib"`: Static library (.lib on Windows, .a on Linux)
- `"dylib"`: Dynamic library (.dll on Windows, .so on Linux)

**Example:**

```toml
[lib]
path = "src/lib.tml"
crate-type = ["rlib", "lib", "dylib"]  # Build all three formats
emit-header = true  # Also generate mylib.h for C FFI
```

### `[[bin]]` - Optional (Multiple Allowed)

Configures binary executables. You can have multiple `[[bin]]` sections for different executables.

**Fields:**

```toml
[[bin]]
name = "my_app"                # Binary name (required)
path = "src/main.tml"          # Entry point (required)

[[bin]]
name = "helper_tool"
path = "src/tools/helper.tml"
```

**Note:** Double brackets `[[bin]]` indicate an array - you can have multiple binaries.

### `[dependencies]` - Optional

Declares dependencies on other TML packages.

**Version dependency:**

```toml
[dependencies]
math_lib = "1.0.0"             # Exact version
utils = "^1.2.0"               # Compatible with 1.x.x (>= 1.2.0, < 2.0.0)
parser = "~1.2.3"              # Compatible with 1.2.x (>= 1.2.3, < 1.3.0)
```

**Path dependency:**

```toml
[dependencies]
local_lib = { path = "../local_lib" }
```

**Git dependency (future):**

```toml
[dependencies]
remote_lib = { git = "https://github.com/user/lib", tag = "v1.0.0" }
```

**Semver Constraints:**

- `"1.0.0"`: Exact version
- `"^1.2.0"`: Caret (>= 1.2.0, < 2.0.0)
- `"~1.2.3"`: Tilde (>= 1.2.3, < 1.3.0)
- `">=1.0.0"`: Greater than or equal to
- `"<2.0.0"`: Less than

### `[build]` - Optional

Build configuration settings.

```toml
[build]
optimization-level = 2         # 0-3, default: 0 (debug)
emit-ir = false                # Emit LLVM IR (.ll files), default: false
emit-header = false            # Emit C headers, default: false
verbose = false                # Verbose build output, default: false
cache = true                   # Use build cache, default: true
parallel = true                # Parallel compilation, default: true
```

**Optimization Levels:**
- `0`: No optimization (debug builds)
- `1`: Basic optimizations (-O1)
- `2`: Moderate optimizations (-O2) **recommended for release**
- `3`: Aggressive optimizations (-O3)

### `[profile.release]` and `[profile.debug]` - Optional

Profile-specific build settings. These override `[build]` settings for specific profiles.

```toml
[profile.debug]
optimization-level = 0
emit-ir = true

[profile.release]
optimization-level = 2
emit-ir = false
```

Usage:
```bash
tml build              # Uses debug profile
tml build --release    # Uses release profile
```

## Complete Example

```toml
# tml.toml - Complete example

[package]
name = "web_server"
version = "2.1.0"
authors = ["Alice Smith <alice@example.com>", "Bob Jones <bob@example.com>"]
edition = "2024"
description = "A high-performance web server written in TML"
license = "MIT"
repository = "https://github.com/example/web_server"

[lib]
path = "src/lib.tml"
crate-type = ["rlib", "lib"]
emit-header = true

[[bin]]
name = "web_server"
path = "src/main.tml"

[[bin]]
name = "config_tool"
path = "src/tools/config.tml"

[dependencies]
http_parser = "^2.0.0"
json = "1.5.0"
logger = { path = "../logger" }

[build]
optimization-level = 0
emit-ir = false
verbose = false

[profile.release]
optimization-level = 2
```

## CLI Integration

### Reading Manifest

When running `tml build`, the compiler automatically reads `tml.toml` from the current directory.

**Behavior:**
1. Look for `tml.toml` in current directory
2. Parse manifest
3. Apply settings to build
4. Command-line flags override manifest settings

### Overriding Manifest

Command-line flags always override manifest settings:

```bash
# Manifest says optimization-level = 0
# This overrides to level 2:
tml build --release
```

### `tml init` Command

Generate a new `tml.toml` manifest:

```bash
tml init
```

Creates:
```toml
[package]
name = "my_project"  # Derived from directory name
version = "0.1.0"
authors = []

[[bin]]
name = "my_project"
path = "src/main.tml"
```

With options:
```bash
tml init --lib                 # Create library project
tml init --name my_lib         # Specify name
tml init --bin src/main.tml    # Create binary project
```

## Dependency Resolution

### Resolution Algorithm

1. **Read manifest**: Parse `tml.toml` and extract dependencies
2. **Locate dependencies**:
   - Path dependencies: Read from specified path
   - Version dependencies: Search in package registry (future)
3. **Resolve versions**: Find compatible versions satisfying all constraints
4. **Build order**: Topologically sort dependencies
5. **Build**: Compile dependencies first, then current package

### Example Dependency Tree

```
web_server (current package)
├── http_parser ^2.0.0
│   └── string_utils ^1.0.0
└── json 1.5.0
    └── string_utils ^1.0.0
```

**Resolution:**
- Both `http_parser` and `json` depend on `string_utils ^1.0.0`
- Resolve to single version: `string_utils 1.0.2` (latest compatible)
- Build order: `string_utils` → `http_parser`, `json` → `web_server`

### Lock File (Future)

For reproducible builds, TML will generate a `tml.lock` file:

```toml
# tml.lock - Auto-generated, do not edit

[[package]]
name = "http_parser"
version = "2.1.0"
source = "registry+https://packages.tmlang.org"
checksum = "abc123..."

[[package]]
name = "json"
version = "1.5.0"
source = "registry+https://packages.tmlang.org"
checksum = "def456..."

[[package]]
name = "string_utils"
version = "1.0.2"
source = "registry+https://packages.tmlang.org"
checksum = "ghi789..."
```

The lock file ensures that:
- All developers use the same dependency versions
- CI builds are reproducible
- Version upgrades are explicit (`tml update`)

## Workspace Support (Future)

For multi-package projects:

```toml
# Workspace root tml.toml
[workspace]
members = [
    "server",
    "client",
    "shared"
]

[workspace.dependencies]
logger = "1.0.0"  # Shared dependency version
```

Each member has its own `tml.toml`:

```
project/
├── tml.toml           # Workspace root
├── server/
│   ├── tml.toml       # Member package
│   └── src/
├── client/
│   ├── tml.toml       # Member package
│   └── src/
└── shared/
    ├── tml.toml       # Member package
    └── src/
```

## Manifest Validation

The compiler validates `tml.toml` on every build:

**Checks:**
- Required fields present (`package.name`, `package.version`)
- Valid semver versions
- Valid dependency specifications
- File paths exist (`lib.path`, `bin.path`)
- No circular dependencies

**Example errors:**

```
error: missing field `package.name` in tml.toml
  ┌─ tml.toml:1:1
  │
1 │ [package]
  │ ^^^^^^^^^ add `name = "..."` here
```

```
error: invalid version `1.x.0` in tml.toml
  ┌─ tml.toml:3:11
  │
3 │ version = "1.x.0"
  │           ^^^^^^^ expected semver format (MAJOR.MINOR.PATCH)
```

## Implementation Files

### Core Implementation

- **`src/cli/build_config.hpp`**: Manifest data structures
- **`src/cli/build_config.cpp`**: TOML parsing and validation
- **`src/cli/cmd_init.cpp`**: `tml init` command
- **`src/cli/dependency_resolver.hpp`**: Dependency resolution (future)

### Data Structures

```cpp
namespace tml::cli {

struct PackageInfo {
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string edition;
    std::string description;
    std::string license;
    std::string repository;
};

struct LibConfig {
    std::string path;
    std::vector<std::string> crate_types;  // ["rlib", "lib", "dylib"]
    std::string name;  // Optional override
    bool emit_header = false;
};

struct BinConfig {
    std::string name;
    std::string path;
};

struct Dependency {
    std::string name;
    std::string version;  // Semver constraint
    std::string path;     // For path dependencies
    std::string git;      // For git dependencies (future)
    std::string tag;      // Git tag (future)
};

struct BuildConfig {
    int optimization_level = 0;
    bool emit_ir = false;
    bool emit_header = false;
    bool verbose = false;
    bool cache = true;
    bool parallel = true;
};

struct ProfileConfig {
    std::string name;  // "debug" or "release"
    BuildConfig build;
};

struct Manifest {
    PackageInfo package;
    std::optional<LibConfig> lib;
    std::vector<BinConfig> bins;
    std::vector<Dependency> dependencies;
    BuildConfig build;
    std::map<std::string, ProfileConfig> profiles;

    // Load from file
    static std::optional<Manifest> load(const fs::path& path);

    // Validate manifest
    bool validate() const;
};

} // namespace tml::cli
```

## Migration Guide

### From Command-Line Flags to Manifest

**Before (all command-line):**

```bash
tml build src/main.tml --crate-type=lib --emit-header --out-dir=build/debug
```

**After (with tml.toml):**

```toml
[package]
name = "mylib"
version = "1.0.0"

[lib]
path = "src/lib.tml"
crate-type = ["lib"]
emit-header = true
```

```bash
tml build  # Much simpler!
```

### Gradual Adoption

- `tml.toml` is **optional** - TML still works without it
- Command-line flags always override manifest
- Start with minimal manifest, add sections as needed

## Future Extensions

### Package Registry

Future support for a centralized package registry:

```toml
[dependencies]
http = "2.0.0"  # Fetched from registry.tmlang.org
```

Commands:
```bash
tml install        # Install dependencies from registry
tml publish        # Publish package to registry
tml search http    # Search for packages
```

### Build Scripts

Custom build steps:

```toml
[package.build]
script = "scripts/build.tml"
```

### Feature Flags

Conditional compilation:

```toml
[features]
default = ["std"]
std = []
no_std = []
networking = ["std"]
```

Usage:
```bash
tml build --features networking
```

## References

- TOML specification: https://toml.io
- Cargo manifest format: https://doc.rust-lang.org/cargo/reference/manifest.html
- Semantic Versioning: https://semver.org
