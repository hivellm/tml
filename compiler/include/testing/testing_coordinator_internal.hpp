//! Internal declarations shared between testing_coordinator.cpp and
//! testing_coordinator_debug.cpp. Not part of the public testing API.

#pragma once

#include "testing/testing_coordinator.hpp"

namespace tml::testing {

/// For each failing test in `result`, emit HIR + MIR + LLVM IR and append the
/// diagnostics to the test's error message.  Called only when
/// TestConfig::debug_layers is true and there are failures.
void emit_debug_layers_for_failures(TestRunResult& result);

} // namespace tml::testing
