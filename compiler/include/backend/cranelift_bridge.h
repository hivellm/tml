// Cranelift Backend FFI Bridge
//
// C API for the Rust-based Cranelift code generation library.
// This header is consumed by the C++ CraneliftCodegenBackend wrapper.

#ifndef TML_CRANELIFT_BRIDGE_H
#define TML_CRANELIFT_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result of a Cranelift compilation operation.
typedef struct CraneliftResult {
    int success;           // 0 = failure, 1 = success
    const uint8_t* data;   // Object file bytes (owned by bridge)
    size_t data_len;       // Length of object data
    const char* ir_text;   // Cranelift IR text (for generate_ir, null otherwise)
    size_t ir_text_len;    // Length of IR text
    const char* error_msg; // Error message (null if success)
} CraneliftResult;

// Options for Cranelift compilation.
typedef struct CraneliftOptions {
    int optimization_level;    // 0 = none, 1-3 = speed_and_size
    const char* target_triple; // e.g. "x86_64-pc-windows-msvc"
    int debug_info;            // 0 or 1
    int dll_export;            // 0 or 1 (export public functions as dllexport)
} CraneliftOptions;

// Compile a full MIR module to an object file.
CraneliftResult cranelift_compile_mir(const uint8_t* mir_data, size_t mir_len,
                                      const CraneliftOptions* options);

// Compile a subset of functions from a MIR module (CGU mode).
CraneliftResult cranelift_compile_mir_cgu(const uint8_t* mir_data, size_t mir_len,
                                          const size_t* func_indices, size_t num_indices,
                                          const CraneliftOptions* options);

// Generate Cranelift IR text from a MIR module (no compilation).
CraneliftResult cranelift_generate_ir(const uint8_t* mir_data, size_t mir_len,
                                      const CraneliftOptions* options);

// Free a CraneliftResult. Must be called for every result returned.
void cranelift_free_result(CraneliftResult* result);

// Get the Cranelift version string (statically allocated, do not free).
const char* cranelift_version(void);

#ifdef __cplusplus
}
#endif

#endif // TML_CRANELIFT_BRIDGE_H
