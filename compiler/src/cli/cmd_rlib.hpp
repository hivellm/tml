//! # RLIB Command Interface
//!
//! This header defines the RLIB inspection command API.
//!
//! ## Subcommands
//!
//! | Function             | Command             | Description              |
//! |----------------------|---------------------|--------------------------|
//! | `run_rlib_info()`    | `tml rlib info`     | Show library metadata    |
//! | `run_rlib_exports()` | `tml rlib exports`  | List public symbols      |
//! | `run_rlib_validate()`| `tml rlib validate` | Check format integrity   |

#ifndef TML_CLI_CMD_RLIB_HPP
#define TML_CLI_CMD_RLIB_HPP

namespace tml::cli {

/**
 * Display RLIB information
 * Shows: library name, version, modules, dependencies
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_rlib_info(int argc, char* argv[]);

/**
 * List exports from RLIB
 * Shows: all public functions, types, etc.
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_rlib_exports(int argc, char* argv[]);

/**
 * Validate RLIB file
 * Checks: format, metadata, object files
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_rlib_validate(int argc, char* argv[]);

/**
 * Main RLIB command dispatcher
 * Handles: tml rlib info, tml rlib exports, tml rlib validate
 *
 * @param argc Argument count
 * @param argv Argument values (starting from "rlib")
 * @return 0 on success, non-zero on error
 */
int run_rlib(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_RLIB_HPP
