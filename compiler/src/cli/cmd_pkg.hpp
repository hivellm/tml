//! # Package Commands Interface
//!
//! This header defines package management command APIs.
//!
//! ## Implemented Commands
//!
//! | Function       | Command       | Description                   |
//! |----------------|---------------|-------------------------------|
//! | `run_deps()`   | `tml deps`    | List project dependencies     |
//! | `run_remove()` | `tml remove`  | Remove dependency from tml.toml|
//!
//! ## Pending Commands (no registry yet)
//!
//! - `run_add()`: Add package from registry
//! - `run_update()`: Update dependencies
//! - `run_publish()`: Publish to registry

#ifndef TML_CLI_CMD_PKG_HPP
#define TML_CLI_CMD_PKG_HPP

namespace tml::cli {

// ============================================================================
// Package Management Commands
// ============================================================================
//
// NOTE: The following commands are NOT YET IMPLEMENTED because there is no
// TML package registry service (similar to crates.io for Rust):
//   - tml add      (requires registry to fetch packages)
//   - tml update   (requires registry to check for updates)
//   - tml publish  (requires registry to upload packages)
//
// For now, use path dependencies in tml.toml:
//   [dependencies]
//   mylib = { path = "../mylib" }
//
// Implemented commands:
//   - tml deps     (list dependencies from tml.toml)
//   - tml remove   (remove dependency from tml.toml)
// ============================================================================

/**
 * Run `tml add <package>` command (NOT YET IMPLEMENTED)
 *
 * Will add a dependency to tml.toml manifest when registry is available.
 * Currently returns an error with instructions for using path dependencies.
 */
int run_add(int argc, char* argv[]);

/**
 * Run `tml update` command (NOT YET IMPLEMENTED)
 *
 * Will update dependencies when registry is available.
 * Currently returns an error with instructions.
 */
int run_update(int argc, char* argv[]);

/**
 * Run `tml publish` command (NOT YET IMPLEMENTED)
 *
 * Will publish package to registry when available.
 * Currently returns an error with alternative suggestions.
 */
int run_publish(int argc, char* argv[]);

/**
 * Run `tml remove <package>` command
 *
 * Removes a dependency from tml.toml manifest.
 *
 * @param argc Argument count
 * @param argv Arguments
 * @return Exit code (0 on success)
 */
int run_remove(int argc, char* argv[]);

/**
 * Run `tml deps` command
 *
 * Lists all dependencies and their status.
 *
 * Usage:
 *   tml deps                       List dependencies
 *   tml deps --tree                Show dependency tree
 *
 * @param argc Argument count
 * @param argv Arguments
 * @return Exit code (0 on success)
 */
int run_deps(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_PKG_HPP
