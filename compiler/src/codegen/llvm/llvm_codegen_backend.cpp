TML_MODULE("codegen_x86")

#include "codegen/llvm/llvm_codegen_backend.hpp"

#include "backend/llvm_backend.hpp"
#include "codegen/llvm/llvm_ir_gen.hpp"
#include "codegen/mir_codegen.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace tml::codegen {

auto LLVMCodegenBackend::capabilities() const -> BackendCapabilities {
    return BackendCapabilities{
        .supports_mir = true,
        .supports_ast = true,
        .supports_generics = true,
        .supports_debug_info = true,
        .supports_coverage = true,
        .supports_cgu = true,
        .max_optimization_level = 3,
    };
}

// --------------------------------------------------------------------------
// Helper: Convert CodegenOptions → MirCodegenOptions
// --------------------------------------------------------------------------

static MirCodegenOptions to_mir_opts(const CodegenOptions& opts) {
    MirCodegenOptions mir_opts;
    mir_opts.emit_comments = opts.emit_comments;
    mir_opts.dll_export = opts.dll_export;
    mir_opts.coverage_enabled = opts.coverage_enabled;
    if (!opts.target_triple.empty()) {
        mir_opts.target_triple = opts.target_triple;
    }
    return mir_opts;
}

// --------------------------------------------------------------------------
// Helper: Convert CodegenOptions → LLVMGenOptions
// --------------------------------------------------------------------------

static LLVMGenOptions to_llvm_gen_opts(const CodegenOptions& opts) {
    LLVMGenOptions llvm_opts;
    llvm_opts.emit_comments = opts.emit_comments;
    llvm_opts.dll_export = opts.dll_export;
    llvm_opts.coverage_enabled = opts.coverage_enabled;
    llvm_opts.emit_debug_info = opts.debug_info;
    if (!opts.target_triple.empty()) {
        llvm_opts.target_triple = opts.target_triple;
    }
    return llvm_opts;
}

// --------------------------------------------------------------------------
// Helper: Compile IR text to object via LLVMBackend
// --------------------------------------------------------------------------

static CodegenResult compile_ir_to_result(const std::string& ir, const CodegenOptions& opts) {
    CodegenResult result;
    result.llvm_ir = ir;

    // Create a temp object path
    auto temp_dir = fs::temp_directory_path() / "tml_codegen_backend";
    fs::create_directories(temp_dir);
    auto obj_path = temp_dir / "output.obj";

    backend::LLVMBackend backend;
    if (!backend.initialize()) {
        result.error_message = "Failed to initialize LLVM backend";
        return result;
    }

    backend::LLVMCompileOptions compile_opts;
    compile_opts.optimization_level = opts.optimization_level;
    compile_opts.debug_info = opts.debug_info;
    if (!opts.target_triple.empty()) {
        compile_opts.target_triple = opts.target_triple;
    }

    auto compile_result = backend.compile_ir_to_object(ir, obj_path, compile_opts);
    if (!compile_result.success) {
        result.error_message = compile_result.error_message;
        return result;
    }

    result.success = true;
    result.object_file = compile_result.object_file;
    return result;
}

// --------------------------------------------------------------------------
// compile_mir: Full MIR module → object
// --------------------------------------------------------------------------

auto LLVMCodegenBackend::compile_mir(const mir::Module& module, const CodegenOptions& opts)
    -> CodegenResult {
    MirCodegen codegen(to_mir_opts(opts));
    auto ir = codegen.generate(module);
    return compile_ir_to_result(ir, opts);
}

// --------------------------------------------------------------------------
// compile_mir_cgu: Subset of MIR functions → object
// --------------------------------------------------------------------------

auto LLVMCodegenBackend::compile_mir_cgu(const mir::Module& module,
                                         const std::vector<size_t>& func_indices,
                                         const CodegenOptions& opts) -> CodegenResult {
    MirCodegen codegen(to_mir_opts(opts));
    auto ir = codegen.generate_cgu(module, func_indices);
    return compile_ir_to_result(ir, opts);
}

// --------------------------------------------------------------------------
// compile_ast: AST + TypeEnv → object
// --------------------------------------------------------------------------

auto LLVMCodegenBackend::compile_ast(const parser::Module& module, const types::TypeEnv& env,
                                     const CodegenOptions& opts) -> CodegenResult {
    CodegenResult result;

    LLVMIRGen gen(env, to_llvm_gen_opts(opts));
    auto gen_result = gen.generate(module);

    if (std::holds_alternative<std::vector<LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<LLVMGenError>>(gen_result);
        result.error_message = errors.empty() ? "Code generation failed" : errors[0].message;
        return result;
    }

    auto ir = std::get<std::string>(gen_result);
    result.link_libs = gen.get_link_libs();
    auto compiled = compile_ir_to_result(ir, opts);
    compiled.link_libs = result.link_libs;
    return compiled;
}

// --------------------------------------------------------------------------
// generate_ir: MIR → IR text (no compilation)
// --------------------------------------------------------------------------

auto LLVMCodegenBackend::generate_ir(const mir::Module& module, const CodegenOptions& opts)
    -> std::string {
    MirCodegen codegen(to_mir_opts(opts));
    return codegen.generate(module);
}

} // namespace tml::codegen
