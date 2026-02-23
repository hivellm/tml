TML_MODULE("compiler")

//! # Compiler Plugin Entry Points
//!
//! This file implements the plugin ABI for the compiler core module.
//! It exports the three mandatory plugin functions (query, init, shutdown)
//! plus `compiler_main(argc, argv)` which is the main dispatch function
//! called by the thin launcher.

#include "plugin/abi.h"

#include <cstring>

// Forward declaration of tml_main from the dispatcher
int tml_main(int argc, char* argv[]);

// ============================================================================
// Plugin Metadata
// ============================================================================

static const char* capabilities[] = {
    CAP_PARSE, CAP_TYPECHECK, CAP_MIR, CAP_CODEGEN_IR,
    nullptr // NULL-terminated
};

static const char* no_dependencies[] = {
    nullptr // compiler has no plugin dependencies
};

static const PluginInfo compiler_plugin_info = {PLUGIN_ABI_VERSION, "compiler", "0.1.0",
                                                capabilities, no_dependencies};

// ============================================================================
// Plugin ABI Exports
// ============================================================================

extern "C" {

PLUGIN_API const PluginInfo* plugin_query(void) {
    return &compiler_plugin_info;
}

PLUGIN_API int plugin_init(void* /* host_ctx */) {
    // No special initialization needed — the dispatcher initializes
    // logging and options on each call to compiler_main().
    return 0;
}

PLUGIN_API void plugin_shutdown(void) {
    // Nothing to clean up
}

// ============================================================================
// Compiler Main Entry Point
// ============================================================================

/// Main compiler entry point, called by the thin launcher.
/// This is the modular equivalent of the monolithic main() → tml_main().
PLUGIN_API int compiler_main(int argc, char* argv[]) {
    return tml_main(argc, argv);
}

} // extern "C"
