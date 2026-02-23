/*
 * Codegen Plugin C API — Functions exported by tml_codegen_x86 plugin.
 *
 * These functions are loaded dynamically via dlsym/GetProcAddress in modular
 * builds, or linked directly in monolithic builds. The host uses these to
 * compile LLVM IR to objects and link them without directly depending on
 * LLVM/LLD libraries.
 *
 * Memory ownership: error strings returned via error_out must be freed
 * by the caller using codegen_free_error().
 */

#ifndef PLUGIN_CODEGEN_API_H
#define PLUGIN_CODEGEN_API_H

#include "plugin/abi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compile LLVM IR text to a native object file.
 * Returns 0 on success, non-zero on failure. */
PLUGIN_API int codegen_compile_ir_to_object(const char* ir_content, const char* output_path,
                                            int opt_level, int debug_info, char** error_out);

/* Link object files into an executable or library.
 * output_type: 0=executable, 1=shared lib, 2=static lib
 * Returns 0 on success, non-zero on failure. */
PLUGIN_API int codegen_link_objects(const char* const* object_paths, int num_objects,
                                    const char* output_path, int output_type, char** error_out);

/* Check if the LLVM backend is available and initialized. */
PLUGIN_API int codegen_is_available(void);

/* Check if the LLD linker is available and initialized. */
PLUGIN_API int codegen_lld_is_available(void);

/* Free an error string returned by codegen functions. */
PLUGIN_API void codegen_free_error(char* error);

#ifdef __cplusplus
}
#endif

/* Function pointer typedefs for dynamic loading */
typedef int (*CodegenCompileIrFn)(const char*, const char*, int, int, char**);
typedef int (*CodegenLinkObjectsFn)(const char* const*, int, const char*, int, char**);
typedef int (*CodegenIsAvailableFn)(void);
typedef int (*CodegenLldIsAvailableFn)(void);
typedef void (*CodegenFreeErrorFn)(char*);

#endif /* PLUGIN_CODEGEN_API_H */
