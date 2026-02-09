//! # Test V2 Command Interface (EXE-based)
//!
//! This header declares the `tml test-v2` command, which uses
//! EXE-based subprocess execution instead of DLL loading.
//!
//! Reuses the same TestOptions and TestResult types from cmd_test.hpp.

#pragma once

namespace tml::cli {

// Run test-v2 command (EXE-based subprocess execution)
int run_test_v2(int argc, char* argv[], bool verbose);

} // namespace tml::cli
