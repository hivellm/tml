TML_MODULE("codegen_x86")

//! # LLVM Backend Implementation
//!
//! Uses the LLVM C API for direct IR compilation to object files.

#include "backend/llvm_backend.hpp"

#include "log/log.hpp"
#include "profiler.hpp"
#include "version_generated.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

// LLVM C API headers
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

// Per-target initialization functions (declared here to avoid InitializeAllTargets overhead)
extern "C" {
void LLVMInitializeX86TargetInfo(void);
void LLVMInitializeX86Target(void);
void LLVMInitializeX86TargetMC(void);
void LLVMInitializeX86AsmParser(void);
void LLVMInitializeX86AsmPrinter(void);

#ifdef TML_LLVM_HAS_AARCH64
void LLVMInitializeAArch64TargetInfo(void);
void LLVMInitializeAArch64Target(void);
void LLVMInitializeAArch64TargetMC(void);
void LLVMInitializeAArch64AsmParser(void);
void LLVMInitializeAArch64AsmPrinter(void);
#endif
}

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

    // Initialize LLVM targets exactly once across all threads.
    // LLVMInitialize* functions are NOT safe for concurrent first calls,
    // but LLVMContextCreate IS thread-safe.
    static std::once_flag s_target_init_flag;
    std::call_once(s_target_init_flag, []() {
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86AsmPrinter();

#ifdef TML_LLVM_HAS_AARCH64
        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64Target();
        LLVMInitializeAArch64TargetMC();
        LLVMInitializeAArch64AsmParser();
        LLVMInitializeAArch64AsmPrinter();
#endif
    });

    // Create per-instance LLVM context (thread-safe)
    context_ = LLVMContextCreate();
    if (!context_) {
        last_error_ = "[K010] Failed to create LLVM context";
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

// ============================================================================
// Large-stack thread helper
// ============================================================================
//
// LLVM's code generator uses deep recursion (e.g., dominator tree DFS, register
// allocator, liveness analysis). For large IR files (35k+ lines) with many
// functions the default 1–16 MB thread stack can overflow.  We run the emit
// operation on a new OS thread whose reserved stack is 128 MB so that even the
// biggest TML parser/serializer modules compile without crashing.

namespace {

#ifdef _WIN32
struct LargeStackTask {
    std::function<void()> fn;
    std::exception_ptr exc{nullptr};
};

DWORD WINAPI large_stack_thread_proc(LPVOID param) {
    auto* task = static_cast<LargeStackTask*>(param);
    try {
        task->fn();
    } catch (...) {
        task->exc = std::current_exception();
    }
    return 0;
}

/// Run `fn` on a thread whose stack is reserved at `stack_bytes`.
/// Returns without throwing; any exception from `fn` is re-thrown here.
void run_on_large_stack(std::function<void()> fn, SIZE_T stack_bytes = 128ULL * 1024 * 1024) {
    LargeStackTask task{std::move(fn)};
    HANDLE h = CreateThread(nullptr, stack_bytes, large_stack_thread_proc, &task,
                            STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!h) {
        // CreateThread failed — fall back to running on current thread
        task.fn();
    } else {
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
        if (task.exc) {
            std::rethrow_exception(task.exc);
        }
    }
}
#else
static void run_on_large_stack(std::function<void()> fn, std::size_t /*stack_bytes*/ = 0) {
    fn(); // On Linux/macOS the default stack is usually 8 MB and ulimit can raise it
}
#endif

// ============================================================================
// IR verification policy (phase25b)
// ============================================================================
//
// The LLVM parser only checks syntax/type well-formedness; the verifier
// enforces the structural rules (dominance, terminators, PHI shape, attribute
// combinations) that, when violated, make the optimizer and codegen produce
// undefined behavior. Invalid IR is ALWAYS a TML codegen bug, so verification
// failure is a hard error. TML_NO_VERIFY_IR=1 demotes it to a warning — an
// escape hatch for bisecting only, never for shipping.

/// True when TML_NO_VERIFY_IR=1 demotes verifier failures to warnings.
static bool verify_ir_demoted() {
    static const bool demoted = [] {
        const char* v = std::getenv("TML_NO_VERIFY_IR");
        return v != nullptr && v[0] == '1' && v[1] == '\0';
    }();
    return demoted;
}

/// Runs the LLVM verifier on `module`. Returns true when compilation may
/// proceed. On failure under the hard-error policy, fills
/// `result.error_message` (K002) and returns false; the caller must dispose
/// its LLVM resources and return `result`.
static bool verify_module_or_fail(LLVMModuleRef module, const char* phase,
                                  LLVMCompileResult& result) {
    char* error = nullptr;
    if (LLVMVerifyModule(module, LLVMReturnStatusAction, &error) != 0) {
        std::string msg = consume_error_message(error);
        if (verify_ir_demoted()) {
            result.warnings.push_back(std::string("[K002] Module verification failed (") + phase +
                                      ", demoted to warning by TML_NO_VERIFY_IR=1): " + msg);
            return true;
        }
        result.error_message =
            std::string("[K002] Module verification failed (") + phase + "): " + msg +
            "\n\nThe generated LLVM IR parsed successfully but violates LLVM's structural\n"
            "rules — compiling it would be undefined behavior. This is always a TML\n"
            "compiler codegen bug. Inspect with `--emit-ir` and report it. To bisect,\n"
            "TML_NO_VERIFY_IR=1 temporarily demotes this error to a warning.";
        return false;
    }
    if (error) {
        LLVMDisposeMessage(error);
    }
    return true;
}

} // namespace

auto LLVMBackend::compile_ir_to_object(const std::string& ir_content, const fs::path& output_path,
                                       const LLVMCompileOptions& options) -> LLVMCompileResult {
    TML_ZONE("llvm::compile_ir_to_object");
    LLVMCompileResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "[K009] LLVM backend not initialized";
        return result;
    }

    auto ctx = static_cast<LLVMContextRef>(context_);

    // Create a memory buffer from the IR content
    LLVMMemoryBufferRef buffer =
        LLVMCreateMemoryBufferWithMemoryRangeCopy(ir_content.c_str(), ir_content.size(), "ir");

    if (!buffer) {
        result.error_message = "[K006] Failed to create memory buffer for IR";
        return result;
    }

    // Parse the LLVM IR
    LLVMModuleRef module = nullptr;
    char* error = nullptr;

    if (LLVMParseIRInContext(ctx, buffer, &module, &error) != 0) {
        std::string llvm_error = consume_error_message(error);
        result.error_message =
            "[K001] Failed to parse LLVM IR: " + llvm_error +
            "\n\nDEBUG: This usually indicates a codegen bug in the TML compiler.\n" +
            "The generated LLVM IR is invalid. Common causes:\n" +
            "  • Type mismatches in function calls (e.g., passing struct by value vs ref)\n" +
            "  • Incorrect struct/enum field layouts or sizes\n" +
            "  • Wrong calling conventions or parameter counts\n" +
            "  • Invalid aggregate type operations\n";
        return result;
    }

    // Verify BEFORE optimization: invalid IR fed to LLVMRunPasses is undefined
    // behavior (and a suspected source of X003-class crashes).
    if (!verify_module_or_fail(module, "pre-optimization", result)) {
        LLVMDisposeModule(module);
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
        result.error_message = "[K005] Failed to get target: " + consume_error_message(error);
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

    // Determine optimization level for codegen
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
        result.error_message = "[K003] Failed to create target machine";
        LLVMDisposeModule(module);
        return result;
    }

    // Set data layout
    LLVMTargetDataRef data_layout = LLVMCreateTargetDataLayout(target_machine);
    char* data_layout_str = LLVMCopyStringRepOfTargetData(data_layout);
    LLVMSetDataLayout(module, data_layout_str);
    LLVMDisposeMessage(data_layout_str);
    LLVMDisposeTargetData(data_layout);

    // Embed compiler identification as !llvm.ident metadata via the LLVM C API.
    // The backend emits this into .comment (ELF) or the debug directory (COFF).
    {
        std::string ident_str = "tml version " + std::string(tml::VERSION);
        LLVMMetadataRef ident_val =
            LLVMMDStringInContext2(ctx, ident_str.c_str(), ident_str.size());
        LLVMMetadataRef ident_node_md = LLVMMDNodeInContext2(ctx, &ident_val, 1);
        LLVMValueRef ident_node = LLVMMetadataAsValue(ctx, ident_node_md);
        LLVMAddNamedMetadataOperand(module, "llvm.ident", ident_node);
    }

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

    // Verify AFTER optimization: passes running on valid input must yield
    // valid output; a failure here is an optimizer-interaction bug. Hard
    // error (phase25b) — was a demoted K002 warning before.
    if (options.optimization_level > 0 &&
        !verify_module_or_fail(module, "post-optimization", result)) {
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(module);
        return result;
    }

    // Emit object file with retry on Windows file locking errors.
    // During concurrent compilation (e.g., test suites), the output .obj file
    // may be temporarily locked by another process (linker, antivirus, etc.).
    // "The requested operation cannot be performed on a file with a user-mapped
    // section open" is a transient Windows error that resolves on retry.
    //
    // Large IR files (35k+ lines from programs importing the full parser chain)
    // can overflow LLVM's register allocator / dominator-tree DFS stack (16 MB
    // default on Windows).  Run the emit on a thread with 128 MB reserved stack
    // so LLVM has enough headroom.
    std::string output_str = output_path.string();
    error = nullptr;
    constexpr int max_retries = 3;
    constexpr int retry_delay_ms = 100;
    bool emit_ok = false;
    std::string emit_error_msg;
    run_on_large_stack([&] {
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            char* emit_err = nullptr;
            if (LLVMTargetMachineEmitToFile(target_machine, module, output_str.c_str(),
                                            LLVMObjectFile, &emit_err) == 0) {
                emit_ok = true;
                return;
            }
            std::string msg = emit_err ? emit_err : "";
            if (emit_err) {
                LLVMDisposeMessage(emit_err);
                emit_err = nullptr;
            }
            // Only retry on Windows file locking errors
            if (msg.find("user-mapped section") != std::string::npos ||
                msg.find("being used by another process") != std::string::npos) {
                TML_LOG_DEBUG("backend", "Object file locked (attempt "
                                             << (attempt + 1) << "/" << max_retries
                                             << "), retrying in " << retry_delay_ms
                                             << "ms: " << output_path.string());
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(retry_delay_ms * (attempt + 1)));
                continue;
            }
            // Non-retryable error — capture and stop
            emit_error_msg = msg;
            return;
        }
    });
    if (!emit_error_msg.empty()) {
        result.error_message = "[K004] Failed to emit object file: " + emit_error_msg;
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(module);
        return result;
    }
    if (!emit_ok) {
        result.error_message = "[K004] Failed to emit object file after " +
                               std::to_string(max_retries) +
                               " retries (file locked): " + output_str;
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(module);
        return result;
    }

    if (options.verbose) {
        TML_LOG_DEBUG("backend", "Compiled to: " << output_path);
    }

    // Cleanup
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeModule(module);

    result.success = true;
    result.object_file = output_path;
    return result;
}

auto LLVMBackend::compile_ir_to_buffer(const std::string& ir_content,
                                       const LLVMCompileOptions& options) -> LLVMCompileResult {
    LLVMCompileResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "[K009] LLVM backend not initialized";
        return result;
    }

    auto ctx = static_cast<LLVMContextRef>(context_);

    // Create a memory buffer from the IR content
    LLVMMemoryBufferRef buffer =
        LLVMCreateMemoryBufferWithMemoryRangeCopy(ir_content.c_str(), ir_content.size(), "ir");

    if (!buffer) {
        result.error_message = "[K006] Failed to create memory buffer for IR";
        return result;
    }

    // Parse the LLVM IR
    LLVMModuleRef module = nullptr;
    char* error = nullptr;

    if (LLVMParseIRInContext(ctx, buffer, &module, &error) != 0) {
        std::string llvm_error = consume_error_message(error);
        result.error_message =
            "[K001] Failed to parse LLVM IR: " + llvm_error +
            "\n\nDEBUG: This usually indicates a codegen bug in the TML compiler.\n" +
            "The generated LLVM IR is invalid. Common causes:\n" +
            "  • Type mismatches in function calls (e.g., passing struct by value vs ref)\n" +
            "  • Incorrect struct/enum field layouts or sizes\n" +
            "  • Wrong calling conventions or parameter counts\n" +
            "  • Invalid aggregate type operations\n";
        return result;
    }

    // Verify BEFORE optimization (phase25b). This path (in-process linking)
    // previously never ran the verifier at all.
    if (!verify_module_or_fail(module, "pre-optimization", result)) {
        LLVMDisposeModule(module);
        return result;
    }

    // Determine target triple
    std::string target_triple = options.target_triple;
    if (target_triple.empty()) {
        target_triple = get_default_target_triple();
    }

    LLVMSetTarget(module, target_triple.c_str());

    // Look up the target
    LLVMTargetRef target = nullptr;
    error = nullptr;
    if (LLVMGetTargetFromTriple(target_triple.c_str(), &target, &error) != 0) {
        result.error_message = "[K005] Failed to get target: " + consume_error_message(error);
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

    LLVMRelocMode reloc_mode = options.position_independent ? LLVMRelocPIC : LLVMRelocDefault;

    // Create target machine
    LLVMTargetMachineRef target_machine =
        LLVMCreateTargetMachine(target, target_triple.c_str(), cpu.c_str(), features.c_str(),
                                opt_level, reloc_mode, LLVMCodeModelDefault);

    if (!target_machine) {
        result.error_message = "[K003] Failed to create target machine";
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
        LLVMPassBuilderOptionsRef pass_opts = LLVMCreatePassBuilderOptions();
        LLVMPassBuilderOptionsSetDebugLogging(pass_opts, options.verbose ? 1 : 0);
        const char* passes = get_opt_level_string(options.optimization_level);
        LLVMRunPasses(module, passes, target_machine, pass_opts);
        LLVMDisposePassBuilderOptions(pass_opts);

        // Verify AFTER optimization (phase25b) — see compile_ir_to_object.
        if (!verify_module_or_fail(module, "post-optimization", result)) {
            LLVMDisposeTargetMachine(target_machine);
            LLVMDisposeModule(module);
            return result;
        }
    }

    // Emit to memory buffer instead of file
    LLVMMemoryBufferRef obj_buffer = nullptr;
    error = nullptr;
    if (LLVMTargetMachineEmitToMemoryBuffer(target_machine, module, LLVMObjectFile, &error,
                                            &obj_buffer) != 0) {
        result.error_message =
            "[K011] Failed to emit object to memory: " + consume_error_message(error);
        LLVMDisposeTargetMachine(target_machine);
        LLVMDisposeModule(module);
        return result;
    }

    // Copy memory buffer to result
    const char* buf_start = LLVMGetBufferStart(obj_buffer);
    size_t buf_size = LLVMGetBufferSize(obj_buffer);
    result.object_data.assign(reinterpret_cast<const uint8_t*>(buf_start),
                              reinterpret_cast<const uint8_t*>(buf_start) + buf_size);

    LLVMDisposeMemoryBuffer(obj_buffer);
    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeModule(module);

    result.success = true;
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
        result.error_message = "[K008] IR file not found: " + ir_file.string();
        return result;
    }

    std::ifstream file(ir_file);
    if (!file) {
        result.error_message = "[K007] Failed to open IR file: " + ir_file.string();
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

// JIT execution support is in jit_engine.cpp (Phase 0b).
