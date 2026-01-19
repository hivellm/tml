//! # LLVM Backend Implementation
//!
//! Uses the LLVM C API for direct IR compilation to object files.

#include "backend/llvm_backend.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

// LLVM C API headers
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

namespace tml::backend {

// ============================================================================
// Helper Functions
// ============================================================================

/// Convert LLVM error message to string and dispose it.
static std::string consume_error_message(char* error) {
    if (error == nullptr) {
        return "";
    }
    std::string msg(error);
    LLVMDisposeMessage(error);
    return msg;
}

/// Get optimization level string for pass builder.
static const char* get_opt_level_string(int level) {
    switch (level) {
    case 0:
        return "default<O0>";
    case 1:
        return "default<O1>";
    case 2:
        return "default<O2>";
    case 3:
        return "default<O3>";
    default:
        return "default<O2>";
    }
}

// ============================================================================
// LLVMBackend Implementation
// ============================================================================

LLVMBackend::LLVMBackend() = default;

LLVMBackend::~LLVMBackend() {
    if (context_) {
        LLVMContextDispose(static_cast<LLVMContextRef>(context_));
        context_ = nullptr;
    }
}

auto LLVMBackend::initialize() -> bool {
    if (initialized_) {
        return true;
    }

    // Initialize all targets
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    // Create LLVM context
    context_ = LLVMContextCreate();
    if (!context_) {
        last_error_ = "Failed to create LLVM context";
        return false;
    }

    initialized_ = true;
    return true;
}

auto LLVMBackend::get_default_target_triple() const -> std::string {
    char* triple = LLVMGetDefaultTargetTriple();
    std::string result(triple);
    LLVMDisposeMessage(triple);
    return result;
}

auto LLVMBackend::compile_ir_to_object(const std::string& ir_content, const fs::path& output_path,
                                       const LLVMCompileOptions& options) -> LLVMCompileResult {
    LLVMCompileResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "LLVM backend not initialized";
        return result;
    }

    auto ctx = static_cast<LLVMContextRef>(context_);

    // Create a memory buffer from the IR content
    LLVMMemoryBufferRef buffer =
        LLVMCreateMemoryBufferWithMemoryRangeCopy(ir_content.c_str(), ir_content.size(), "ir");

    if (!buffer) {
        result.error_message = "Failed to create memory buffer for IR";
        return result;
    }

    // Parse the LLVM IR
    LLVMModuleRef module = nullptr;
    char* error = nullptr;

    if (LLVMParseIRInContext(ctx, buffer, &module, &error) != 0) {
        result.error_message = "Failed to parse LLVM IR: " + consume_error_message(error);
        return result;
    }

    // Determine target triple
    std::string target_triple = options.target_triple;
    if (target_triple.empty()) {
        target_triple = get_default_target_triple();
    }

    // Set the target triple on the module
    LLVMSetTarget(module, target_triple.c_str());

    // Look up the target
    LLVMTargetRef target = nullptr;
    error = nullptr;
    if (LLVMGetTargetFromTriple(target_triple.c_str(), &target, &error) != 0) {
        result.error_message = "Failed to get target: " + consume_error_message(error);
        LLVMDisposeModule(module);
        return result;
    }

    // Determine CPU and features
    std::string cpu = options.cpu;
    if (cpu.empty() || cpu == "native") {
        char* host_cpu = LLVMGetHostCPUName();
        cpu = host_cpu;
        LLVMDisposeMessage(host_cpu);
    }

    std::string features = options.features;
    if (features.empty()) {
        char* host_features = LLVMGetHostCPUFeatures();
        features = host_features;
        LLVMDisposeMessage(host_features);
    }

    // Determine optimization level
    LLVMCodeGenOptLevel opt_level;
    switch (options.optimization_level) {
    case 0:
        opt_level = LLVMCodeGenLevelNone;
        break;
    case 1:
        opt_level = LLVMCodeGenLevelLess;
        break;
    case 2:
        opt_level = LLVMCodeGenLevelDefault;
        break;
    case 3:
    default:
        opt_level = LLVMCodeGenLevelAggressive;
        break;
    }

    // Relocation model
    LLVMRelocMode reloc_mode = options.position_independent ? LLVMRelocPIC : LLVMRelocDefault;

    // Create target machine
    LLVMTargetMachineRef target_machine =
        LLVMCreateTargetMachine(target, target_triple.c_str(), cpu.c_str(), features.c_str(),
                                opt_level, reloc_mode, LLVMCodeModelDefault);

    if (!target_machine) {
        result.error_message = "Failed to create target machine";
        LLVMDisposeModule(module);
        return result;
    }

    // Set data layout
    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    char* data_layout_str = LLVMCopyStringRepOfTargetData(data_layout);
    LLVMSetDataLayout(module, data_layout_str);
    LLVMDisposeMessage(data_layout_str);
    LLVMDisposeTargetData(data_layout);

    // Run optimization passes if optimization level > 0
    if (options.optimization_level > 0) {
        // Create pass builder options
        LLVMPassBuilderOptionsRef pass_opts = LLVMCreatePassBuilderOptions();

        // Configure pass builder options
        LLVMPassBuilderOptionsSetDebugLogging(pass_opts, options.verbose ? 1 : 0);

        // Run the optimization pipeline
        const char* passes = get_opt_level_string(options.optimization_level);
        error = nullptr;
        if (LLVMRunPasses(module, passes, target_machine, pass_opts) != LLVMErrorSuccess) {
            // Note: LLVMRunPasses returns LLVMErrorRef, handle accordingly
            result.warnings.push_back("Warning: optimization passes may have issues");
        }

        LLVMDisposePassBuilderOptions(pass_opts);
    }

    // Verify the module
    error = nullptr;
    if (LLVMVerifyModule(module, LLVMReturnStatusAction, &error) != 0) {
        result.warnings.push_back("Module verification warning: " + consume_error_message(error));
    } else if (error) {
        LLVMDisposeMessage(error);
    }

    // Emit object file
    std::string output_str = output_path.string();
    error = nullptr;
    if (LLVMTargetMachineEmitToFile(target_machine, module, output_str.c_str(), LLVMObjectFile,
                                    &error) != 0) {
        result.error_message = "Failed to emit object file: " + consume_error_message(error);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(module);
        return result;
    }

    if (options.verbose) {
        std::cout << "[llvm_backend] Compiled to: " << output_path << "\n";
    }

    // Cleanup
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeModule(module);

    result.success = true;
    result.object_file = output_path;
    return result;
}

auto LLVMBackend::compile_ir_file_to_object(const fs::path& ir_file,
                                            const std::optional<fs::path>& output_path,
                                            const LLVMCompileOptions& options)
    -> LLVMCompileResult {
    LLVMCompileResult result;
    result.success = false;

    // Read the IR file
    if (!fs::exists(ir_file)) {
        result.error_message = "IR file not found: " + ir_file.string();
        return result;
    }

    std::ifstream file(ir_file);
    if (!file) {
        result.error_message = "Failed to open IR file: " + ir_file.string();
        return result;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string ir_content = ss.str();
    file.close();

    // Determine output path
    fs::path out_path;
    if (output_path.has_value()) {
        out_path = output_path.value();
    } else {
        out_path = ir_file;
#ifdef _WIN32
        out_path.replace_extension(".obj");
#else
        out_path.replace_extension(".o");
#endif
    }

    return compile_ir_to_object(ir_content, out_path, options);
}

// ============================================================================
// Module-level Functions
// ============================================================================

auto is_llvm_backend_available() -> bool {
    // Try to initialize - if LLVM is properly linked, this will succeed
    LLVMBackend backend;
    return backend.initialize();
}

auto get_llvm_version() -> std::string {
    // LLVM version is available through the C API
    unsigned major = 0, minor = 0, patch = 0;

    // LLVMGetVersion is available in LLVM 16+
#if LLVM_VERSION_MAJOR >= 16
    LLVMGetVersion(&major, &minor, &patch);
#else
    // For older versions, we can use a compile-time constant
    major = LLVM_VERSION_MAJOR;
    minor = LLVM_VERSION_MINOR;
    patch = LLVM_VERSION_PATCH;
#endif

    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

} // namespace tml::backend
