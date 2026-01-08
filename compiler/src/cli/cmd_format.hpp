//! # Format Command Interface
//!
//! This header defines the code formatting command API.
//!
//! ## Usage
//!
//! - `run_fmt(path, false, ...)`: Format file in-place
//! - `run_fmt(path, true, ...)`: Check formatting (no changes)

#pragma once
#include <string>

namespace tml::cli {

// Format command
int run_fmt(const std::string& path, bool check_only, bool verbose);

} // namespace tml::cli
