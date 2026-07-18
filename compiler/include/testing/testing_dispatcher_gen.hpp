//! # Dispatcher IR Generator (NDJSON)
//!
//! Generates LLVM IR for a test dispatcher that emits NDJSON events.
//! The dispatcher is compiled into an EXE that runs test functions and
//! outputs structured JSON to stdout.
//!
//! ## Modes
//!
//! - `--run-all`       — Run all tests, emit NDJSON events per test
//! - `--test-index=N`  — Run single test, emit NDJSON for just that test
//! - `--list`          — Emit test metadata as JSON array, exit(0)
//!
//! ## NDJSON Events Emitted
//!
//! suite_start → (test_start → test_pass|test_fail)* → suite_end
//!
//! Crash isolation (SEH/signals) and per-test timeout are handled
//! by the generated dispatcher code, not by the coordinator.

#pragma once

#include <string>
#include <vector>

namespace tml::testing {

/// Information about a single test for dispatcher generation.
struct DispatcherTestInfo {
    std::string name; // e.g. "test_concat"
    std::string file; // e.g. "basic.test.tml"
    int index = 0;    // 0-based suite position — user-facing selection/event index.
    // F-023: the compiled test object exports its entry as `tml_test_<symbol_id>`
    // where symbol_id is a file-STABLE id (query::stable_test_symbol_id), NOT the
    // suite position. The dispatcher declares/calls that stable symbol, but keeps
    // `index` for --test-index selection, --list, and NDJSON event indices (which
    // the coordinator maps to suite slots). Defaults to `index` when unset.
    uint32_t symbol_id = 0;
};

/// Generate LLVM IR for a test dispatcher with NDJSON output.
///
/// The generated IR supports three modes:
/// - `--run-all`: runs all tests, emits NDJSON events
/// - `--test-index=N`: runs single test with NDJSON
/// - `--list`: emits test metadata as JSON array
///
/// @param tests     Test metadata (name, file, index)
/// @param suite_name  Name of the suite (e.g. "core_str")
/// @return LLVM IR string ready for compilation
std::string generate_ndjson_dispatcher_ir(const std::vector<DispatcherTestInfo>& tests,
                                          const std::string& suite_name);

} // namespace tml::testing
