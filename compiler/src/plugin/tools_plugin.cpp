TML_MODULE("tools")

//! # Tools Plugin Entry Points
//!
//! This file implements the plugin ABI for the tools module.
//! It exports the formatter, linter, doc generator, and search capabilities.

#include "plugin/abi.h"

// ============================================================================
// Plugin Metadata
// ============================================================================

static const char* capabilities[] = {CAP_FORMAT, CAP_LINT, CAP_DOC, CAP_SEARCH, nullptr};

static const char* dependencies[] = {"compiler", nullptr};

static const PluginInfo tools_plugin_info = {PLUGIN_ABI_VERSION, "tools", "0.1.0", capabilities,
                                             dependencies};

// ============================================================================
// Plugin ABI Exports
// ============================================================================

extern "C" {

PLUGIN_API const PluginInfo* plugin_query(void) {
    return &tools_plugin_info;
}

PLUGIN_API int plugin_init(void* /* host_ctx */) {
    return 0; // No special initialization needed
}

PLUGIN_API void plugin_shutdown(void) {
    // Nothing to clean up
}

} // extern "C"
