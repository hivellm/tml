//! # Install Command Interface
//!
//! This header defines the `tml install` command API.
//!
//! ## Usage
//!
//! - `tml install`: Install native dependencies for all packages
//! - `tml install --native-only`: Only install native deps (skip TML deps)
//! - `tml install --verbose`: Show detailed output

#ifndef TML_CLI_CMD_INSTALL_HPP
#define TML_CLI_CMD_INSTALL_HPP

namespace tml::cli {

/**
 * Install dependencies for the current project.
 *
 * Reads tml.toml for dependencies, finds their package.toml files,
 * parses [native-deps] sections, and copies platform-appropriate
 * native files to the user cache (~/.tml/native-cache/).
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_install(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_INSTALL_HPP
