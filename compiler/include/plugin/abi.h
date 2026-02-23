/*
 * Plugin ABI — Stable C interface for plugin modules.
 *
 * Every plugin DLL exports exactly three functions:
 *   - plugin_query()    → returns plugin metadata
 *   - plugin_init()     → called once after loading
 *   - plugin_shutdown() → called before unloading
 *
 * The ABI uses only C types to avoid C++ ABI issues across DLL boundaries.
 * Memory ownership rule: the plugin owns all pointers it returns.
 */

#ifndef PLUGIN_ABI_H
#define PLUGIN_ABI_H

#include <stdint.h>

/* ===== Export/Import macros ===== */

#ifdef _WIN32
#ifdef PLUGIN_BUILDING
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API __declspec(dllimport)
#endif
#else
#define PLUGIN_API __attribute__((visibility("default")))
#endif

/* ===== ABI version ===== */

#define PLUGIN_ABI_VERSION 1

/* ===== Plugin metadata ===== */

typedef struct PluginInfo {
    uint32_t abi_version;            /* Must equal PLUGIN_ABI_VERSION    */
    const char* name;                /* e.g. "codegen_x86"              */
    const char* version;             /* e.g. "0.1.0"                    */
    const char* const* capabilities; /* NULL-terminated string array    */
    const char* const* dependencies; /* NULL-terminated string array    */
} PluginInfo;

/* ===== Plugin entry points =====
 *
 * Each plugin DLL must export these with extern "C" linkage:
 *
 *   PLUGIN_API const PluginInfo* plugin_query(void);
 *   PLUGIN_API int  plugin_init(void* host_ctx);
 *   PLUGIN_API void plugin_shutdown(void);
 */

/* Function pointer typedefs for dynamic loading */
typedef const PluginInfo* (*PluginQueryFn)(void);
typedef int (*PluginInitFn)(void* host_ctx);
typedef void (*PluginShutdownFn)(void);

/* ===== Capability constants ===== */

/* Compiler */
#define CAP_PARSE "parse"
#define CAP_TYPECHECK "typecheck"
#define CAP_MIR "mir"
#define CAP_CODEGEN_IR "codegen_ir"

/* Backends */
#define CAP_TARGET_X86 "target_x86_64"
#define CAP_TARGET_ARM64 "target_aarch64"
#define CAP_TARGET_CUDA "target_cuda"
#define CAP_EMIT_OBJ "emit_obj"
#define CAP_EMIT_ASM "emit_asm"
#define CAP_LINK "link"

/* Tools */
#define CAP_FORMAT "format"
#define CAP_LINT "lint"
#define CAP_DOC "doc"
#define CAP_SEARCH "search"

/* Test */
#define CAP_TEST_RUN "test_run"
#define CAP_COVERAGE "coverage"
#define CAP_BENCHMARK "benchmark"
#define CAP_FUZZ "fuzz"

/* MCP */
#define CAP_MCP_SERVER "mcp_server"

#endif /* PLUGIN_ABI_H */
