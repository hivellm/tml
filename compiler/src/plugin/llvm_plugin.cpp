TML_MODULE("codegen_x86")

//! # LLVM Codegen Plugin Entry Points
//!
//! This file implements the plugin ABI for the codegen_x86 module.
//! It exports the three mandatory plugin functions (query, init, shutdown)
//! plus the codegen-specific C API functions that the thin launcher or
//! monolithic build can call via dlsym/GetProcAddress.

#include "backend/lld_linker.hpp"
#include "backend/llvm_backend.hpp"
#include "plugin/abi.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Safe string duplication for error output (avoids MSVC strcpy deprecation).
static char* dup_string(const char* src) {
    if (!src)
        return nullptr;
    size_t len = strlen(src) + 1;
    char* dst = static_cast<char*>(malloc(len));
    if (dst)
        memcpy(dst, src, len);
    return dst;
}

// ============================================================================
// Plugin Metadata
// ============================================================================

static const char* capabilities[] = {
    CAP_CODEGEN_IR, CAP_TARGET_X86, CAP_TARGET_ARM64, CAP_EMIT_OBJ, CAP_LINK,
    nullptr // NULL-terminated
};

static const char* dependencies[] = {
    "compiler",
    nullptr // NULL-terminated
};

static const PluginInfo plugin_info = {PLUGIN_ABI_VERSION, "codegen_x86", "0.1.0", capabilities,
                                       dependencies};

// ============================================================================
// Plugin Globals
// ============================================================================

static std::unique_ptr<tml::backend::LLVMBackend> g_llvm_backend;
static std::unique_ptr<tml::backend::LLDLinker> g_lld_linker;
static std::mutex g_mutex;

// ============================================================================
// Plugin ABI Exports
// ============================================================================

extern "C" {

PLUGIN_API const PluginInfo* plugin_query(void) {
    return &plugin_info;
}

PLUGIN_API int plugin_init(void* /* host_ctx */) {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Initialize LLVM backend
    g_llvm_backend = std::make_unique<tml::backend::LLVMBackend>();
    if (!g_llvm_backend->initialize()) {
        return -1;
    }

    // Initialize LLD linker
    g_lld_linker = std::make_unique<tml::backend::LLDLinker>();
    (void)g_lld_linker->initialize(); // Not fatal if LLD not found

    return 0; // Success
}

PLUGIN_API void plugin_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lld_linker.reset();
    g_llvm_backend.reset();
}

// ============================================================================
// Codegen C API — called by the host (monolithic or thin launcher)
// ============================================================================

/// Compile LLVM IR text to a native object file.
///
/// @param ir_content   NULL-terminated LLVM IR text
/// @param output_path  NULL-terminated output file path
/// @param opt_level    Optimization level (0-3)
/// @param debug_info   Non-zero to include debug info
/// @param error_out    If non-null, receives an allocated error string on failure (caller must
/// free)
/// @return 0 on success, non-zero on failure
PLUGIN_API int codegen_compile_ir_to_object(const char* ir_content, const char* output_path,
                                            int opt_level, int debug_info, char** error_out) {
    if (!g_llvm_backend || !g_llvm_backend->is_initialized()) {
        if (error_out)
            *error_out = dup_string("LLVM backend not initialized");
        return -1;
    }

    tml::backend::LLVMCompileOptions opts;
    opts.optimization_level = opt_level;
    opts.debug_info = debug_info != 0;

    auto result = g_llvm_backend->compile_ir_to_object(ir_content, output_path, opts);

    if (!result.success) {
        if (error_out)
            *error_out = dup_string(result.error_message.c_str());
        return -1;
    }

    return 0;
}

/// Link object files into an executable or library.
///
/// @param object_paths   Array of NULL-terminated object file paths
/// @param num_objects    Number of object files
/// @param output_path    NULL-terminated output file path
/// @param output_type    0=executable, 1=shared lib, 2=static lib
/// @param error_out      If non-null, receives an allocated error string on failure (caller must
/// free)
/// @return 0 on success, non-zero on failure
PLUGIN_API int codegen_link_objects(const char* const* object_paths, int num_objects,
                                    const char* output_path, int output_type, char** error_out) {
    if (!g_lld_linker) {
        if (error_out)
            *error_out = dup_string("LLD linker not initialized");
        return -1;
    }

    std::vector<std::filesystem::path> objects;
    objects.reserve(num_objects);
    for (int i = 0; i < num_objects; ++i) {
        objects.emplace_back(object_paths[i]);
    }

    tml::backend::LLDLinkOptions opts;
    switch (output_type) {
    case 0:
        opts.output_type = tml::backend::LLDOutputType::Executable;
        break;
    case 1:
        opts.output_type = tml::backend::LLDOutputType::SharedLib;
        break;
    case 2:
        opts.output_type = tml::backend::LLDOutputType::StaticLib;
        break;
    default:
        opts.output_type = tml::backend::LLDOutputType::Executable;
        break;
    }

    auto result = g_lld_linker->link(objects, output_path, opts);

    if (!result.success) {
        if (error_out)
            *error_out = dup_string(result.error_message.c_str());
        return -1;
    }

    return 0;
}

/// Check if the LLVM backend is available and initialized.
/// @return Non-zero if available
PLUGIN_API int codegen_is_available(void) {
    return (g_llvm_backend && g_llvm_backend->is_initialized()) ? 1 : 0;
}

/// Check if the LLD linker is available and initialized.
/// @return Non-zero if available
PLUGIN_API int codegen_lld_is_available(void) {
    return (g_lld_linker && g_lld_linker->is_initialized()) ? 1 : 0;
}

/// Free an error string returned by codegen functions.
PLUGIN_API void codegen_free_error(char* error) {
    free(error);
}

} // extern "C"
