//! # Explain Command Interface
//!
//! Show detailed explanation for a compiler error code.
//!
//! ## Usage
//!
//! | Command                | Output                              |
//! |------------------------|-------------------------------------|
//! | `tml explain T001`     | Detailed type mismatch explanation  |
//! | `tml explain B001`     | Use-after-move explanation          |

#pragma once
#include <string>

namespace tml::cli {

/// Show detailed explanation for an error code.
/// Returns 0 on success, 1 if the code is not found.
int run_explain(const std::string& code, bool verbose);

} // namespace tml::cli
