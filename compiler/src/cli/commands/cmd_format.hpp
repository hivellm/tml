//! # Format Command Interface
//!
//! This header defines the code formatting command API.
//!
//! ## Usage
//!
//! - `run_fmt(path, false, ...)`: Format file or directory in-place
//! - `run_fmt(path, true, ...)`: Check formatting (no changes)
//!
//! Supports both TML files (.tml) via AST-based formatter, and C/C++ files
//! (.c, .cpp, .h, .hpp) via clang-format delegation.

#pragma once
#include <string>

namespace tml::cli {

// Format command — handles files and directories, dispatches by extension
int run_fmt(const std::string& path, bool check_only, bool verbose);

} // namespace tml::cli
