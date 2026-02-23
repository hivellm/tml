TML_MODULE("mcp")

//! # MCP Plugin Entry Points
//!
//! This file implements the plugin ABI for the MCP (Model Context Protocol)
//! server module.

#include "plugin/abi.h"

// ============================================================================
// Plugin Metadata
// ============================================================================

static const char* capabilities[] = {CAP_MCP_SERVER, nullptr};

static const char* dependencies[] = {"compiler", nullptr};

static const PluginInfo mcp_plugin_info = {PLUGIN_ABI_VERSION, "mcp", "0.1.0", capabilities,
                                           dependencies};

// ============================================================================
// Plugin ABI Exports
// ============================================================================

extern "C" {

PLUGIN_API const PluginInfo* plugin_query(void) {
    return &mcp_plugin_info;
}

PLUGIN_API int plugin_init(void* /* host_ctx */) {
    return 0; // No special initialization needed
}

PLUGIN_API void plugin_shutdown(void) {
    // Nothing to clean up
}

} // extern "C"
