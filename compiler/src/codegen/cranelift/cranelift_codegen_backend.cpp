TML_MODULE("codegen_cranelift")

#include "codegen/cranelift/cranelift_codegen_backend.hpp"

#include "backend/cranelift_bridge.h"
#include "mir/mir_serialize.hpp"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;

namespace tml::codegen {

// --------------------------------------------------------------------------
// capabilities
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::capabilities() const -> BackendCapabilities {
    return BackendCapabilities{
        .supports_mir = true,
        .supports_ast = false,
        .supports_generics = false,
        .supports_debug_info = false,
        .supports_coverage = false,
        .supports_cgu = true,
        .max_optimization_level = 2,
    };
}

// --------------------------------------------------------------------------
// Helper: CodegenOptions → CraneliftOptions
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::to_cranelift_opts(const CodegenOptions& opts) -> ::CraneliftOptions {
    ::CraneliftOptions c_opts{};
    c_opts.optimization_level = opts.optimization_level;
    c_opts.target_triple = opts.target_triple.empty() ? nullptr : opts.target_triple.c_str();
    c_opts.debug_info = opts.debug_info ? 1 : 0;
    c_opts.dll_export = opts.dll_export ? 1 : 0;
    return c_opts;
}

// --------------------------------------------------------------------------
// Helper: Write object bytes to temp file
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::write_object_file(const uint8_t* data, size_t len) -> fs::path {
    auto temp_dir = fs::temp_directory_path() / "tml_cranelift";
    fs::create_directories(temp_dir);

    // Use unique filename per thread to avoid races in parallel compilation
    static std::atomic<uint64_t> counter{0};
    auto id = counter.fetch_add(1, std::memory_order_relaxed);
    auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    auto obj_name = "output_" + std::to_string(tid) + "_" + std::to_string(id) + ".obj";
    auto obj_path = temp_dir / obj_name;

    std::ofstream out(obj_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open temp file: " + obj_path.string());
    }
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    if (!out) {
        throw std::runtime_error("Failed to write object file: " + obj_path.string());
    }
    return obj_path;
}

// --------------------------------------------------------------------------
// compile_mir: Full MIR module → object
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::compile_mir(const mir::Module& module, const CodegenOptions& opts)
    -> CodegenResult {
    CodegenResult result;

    // Serialize MIR to binary
    auto mir_bytes = mir::serialize_binary(module);

    // Call Cranelift bridge
    auto c_opts = to_cranelift_opts(opts);
    auto c_result = cranelift_compile_mir(mir_bytes.data(), mir_bytes.size(), &c_opts);

    if (!c_result.success) {
        result.error_message =
            c_result.error_msg ? std::string(c_result.error_msg) : "Cranelift compilation failed";
        cranelift_free_result(&c_result);
        return result;
    }

    // Write object bytes to temp file
    try {
        result.object_file = write_object_file(c_result.data, c_result.data_len);
        result.success = true;
    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    cranelift_free_result(&c_result);
    return result;
}

// --------------------------------------------------------------------------
// compile_mir_cgu: Subset of MIR functions → object
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::compile_mir_cgu(const mir::Module& module,
                                              const std::vector<size_t>& func_indices,
                                              const CodegenOptions& opts) -> CodegenResult {
    CodegenResult result;

    auto mir_bytes = mir::serialize_binary(module);

    auto c_opts = to_cranelift_opts(opts);
    auto c_result = cranelift_compile_mir_cgu(mir_bytes.data(), mir_bytes.size(),
                                              func_indices.data(), func_indices.size(), &c_opts);

    if (!c_result.success) {
        result.error_message = c_result.error_msg ? std::string(c_result.error_msg)
                                                  : "Cranelift CGU compilation failed";
        cranelift_free_result(&c_result);
        return result;
    }

    try {
        result.object_file = write_object_file(c_result.data, c_result.data_len);
        result.success = true;
    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    cranelift_free_result(&c_result);
    return result;
}

// --------------------------------------------------------------------------
// compile_ast: Not supported by Cranelift backend
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::compile_ast(const parser::Module& /*module*/,
                                          const types::TypeEnv& /*env*/,
                                          const CodegenOptions& /*opts*/) -> CodegenResult {
    CodegenResult result;
    result.error_message = "Cranelift backend does not support AST compilation. Use MIR path.";
    return result;
}

// --------------------------------------------------------------------------
// generate_ir: MIR → Cranelift IR text (no compilation)
// --------------------------------------------------------------------------

auto CraneliftCodegenBackend::generate_ir(const mir::Module& module, const CodegenOptions& opts)
    -> std::string {
    auto mir_bytes = mir::serialize_binary(module);

    auto c_opts = to_cranelift_opts(opts);
    auto c_result = cranelift_generate_ir(mir_bytes.data(), mir_bytes.size(), &c_opts);

    std::string ir_text;
    if (c_result.success && c_result.ir_text) {
        ir_text.assign(c_result.ir_text, c_result.ir_text_len);
    } else if (c_result.error_msg) {
        ir_text = std::string("; ERROR: ") + c_result.error_msg;
    } else {
        ir_text = "; ERROR: Failed to generate Cranelift IR";
    }

    cranelift_free_result(&c_result);
    return ir_text;
}

} // namespace tml::codegen
