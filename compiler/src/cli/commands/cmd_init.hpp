//! # Init Command Interface
//!
//! This header defines the project initialization command API.
//!
//! ## Usage
//!
//! - `tml init`: Create binary project
//! - `tml init --lib`: Create library project
//! - `tml init --name foo`: Custom project name

#ifndef TML_CLI_CMD_INIT_HPP
#define TML_CLI_CMD_INIT_HPP

namespace tml::cli {

/**
 * Initialize a new TML project
 * Creates tml.toml manifest file
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_init(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_INIT_HPP
