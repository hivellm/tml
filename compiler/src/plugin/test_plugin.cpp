TML_MODULE("test")

//! # Test Plugin Entry Points
//!
//! This file implements the plugin ABI for the test runner module.
//! It exports test execution, coverage, benchmark, and fuzz capabilities.

#include "plugin/abi.h"

// ============================================================================
// Plugin Metadata
// ============================================================================

static const char* capabilities[] = {CAP_TEST_RUN, CAP_COVERAGE, CAP_BENCHMARK, CAP_FUZZ, nullptr};

static const char* dependencies[] = {"compiler", nullptr};

static const PluginInfo test_plugin_info = {PLUGIN_ABI_VERSION, "test", "0.1.0", capabilities,
                                            dependencies};

// ============================================================================
// Plugin ABI Exports
// ============================================================================

extern "C" {

PLUGIN_API const PluginInfo* plugin_query(void) {
    return &test_plugin_info;
}

PLUGIN_API int plugin_init(void* /* host_ctx */) {
    return 0; // No special initialization needed
}

PLUGIN_API void plugin_shutdown(void) {
    // Nothing to clean up
}

} // extern "C"
