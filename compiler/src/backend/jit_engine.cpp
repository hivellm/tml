TML_MODULE("codegen_x86")

//! # JIT Engine Implementation
//!
//! ORC LLJIT-based in-process execution using the LLVM C++ API.
//! Guarded by TML_HAS_JIT — only compiled when TML_USE_JIT=ON.

#if TML_HAS_JIT

#include "backend/jit_engine.hpp"

// LLVM C++ API headers for ORC JIT
#include <cstdio>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <cstdlib>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tml::backend {

// ============================================================================
// Internal helpers
// ============================================================================

/// Convert an llvm::Error to a std::string and consume it.
static auto error_to_string(llvm::Error err) -> std::string {
    std::string msg;
    llvm::handleAllErrors(std::move(err), [&](const llvm::ErrorInfoBase& info) {
        if (!msg.empty()) {
            msg += "; ";
        }
        msg += info.message();
    });
    return msg;
}

/// Initialize LLVM native targets exactly once across all threads.
///
/// Safe to call from multiple threads — uses std::call_once internally.
static void ensure_native_targets_initialized() {
    static std::once_flag s_init_flag;
    std::call_once(s_init_flag, []() {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
    });
}

// ============================================================================
// JitEngineImpl (pimpl)
// ============================================================================

struct JitEngineImpl {
    std::unique_ptr<llvm::orc::LLJIT> jit;
};

// ============================================================================
// JitEngine
// ============================================================================

JitEngine::JitEngine(std::unique_ptr<JitEngineImpl> impl) : impl_(std::move(impl)) {}

JitEngine::~JitEngine() = default;

JitEngine::JitEngine(JitEngine&&) noexcept = default;
JitEngine& JitEngine::operator=(JitEngine&&) noexcept = default;

/// Create a new JIT engine targeting the host machine.
auto JitEngine::create() -> std::pair<JitResult, std::unique_ptr<JitEngine>> {
    ensure_native_targets_initialized();

    auto jit_or_err = llvm::orc::LLJITBuilder().create();
    if (!jit_or_err) {
        JitResult result;
        result.success = false;
        result.error = "[J001] Failed to create LLJIT: " + error_to_string(jit_or_err.takeError());
        return {result, nullptr};
    }

    auto impl = std::make_unique<JitEngineImpl>();
    impl->jit = std::move(*jit_or_err);

    // Register a search generator that resolves symbols from the current process.
    // This makes all TML runtime functions (print, tml_alloc, tml_free, etc.)
    // and C library functions (memcpy, malloc, etc.) available to JIT'd code.
    auto& main_dylib = impl->jit->getMainJITDylib();
    char global_prefix = impl->jit->getDataLayout().getGlobalPrefix();

    auto proc_gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(global_prefix);
    if (!proc_gen) {
        JitResult result;
        result.success = false;
        result.error = "[J006] Failed to create process symbol generator: " +
                       error_to_string(proc_gen.takeError());
        return {result, nullptr};
    }
    main_dylib.addGenerator(std::move(*proc_gen));

    // Manually register TML runtime functions that aren't dllexport'd.
    // These are bare-name C functions (print, println, panic, etc.) linked
    // statically into the compiler binary but not exported from the DLL.
    {
        auto& es = impl->jit->getExecutionSession();
        llvm::orc::SymbolMap runtime_syms;

        auto add_sym = [&](const char* name, void* addr) {
            auto flags = llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable;
            runtime_syms[es.intern(name)] = {llvm::orc::ExecutorAddr::fromPtr(addr), flags};
        };

        // C library functions commonly used by generated IR.
        // TML runtime functions (print, mem_alloc, etc.) are now TML_EXPORT'd
        // and discovered automatically by GetForCurrentProcess().
        add_sym("snprintf", reinterpret_cast<void*>(&::snprintf));
        add_sym("memcpy", reinterpret_cast<void*>(&::memcpy));
        add_sym("memset", reinterpret_cast<void*>(&::memset));
        add_sym("memmove", reinterpret_cast<void*>(&::memmove));
        add_sym("strlen", reinterpret_cast<void*>(&::strlen));

        if (auto err = main_dylib.define(llvm::orc::absoluteSymbols(std::move(runtime_syms)))) {
            JitResult result;
            result.success = false;
            result.error =
                "[J007] Failed to register runtime symbols: " + error_to_string(std::move(err));
            return {result, nullptr};
        }
    }

    JitResult ok;
    ok.success = true;
    return {ok, std::unique_ptr<JitEngine>(new JitEngine(std::move(impl)))};
}

/// Parse LLVM IR text and add the resulting module to the main JITDylib.
auto JitEngine::addModule(const std::string& ir_text) -> JitResult {
    JitResult result;

    // Each module needs its own context to avoid threading issues.
    auto ctx = std::make_unique<llvm::LLVMContext>();

    llvm::SMDiagnostic diag;
    auto mem_buf =
        llvm::MemoryBufferRef(llvm::StringRef(ir_text.data(), ir_text.size()), "jit_input");
    auto mod = llvm::parseIR(mem_buf, diag, *ctx);

    if (!mod) {
        result.success = false;
        result.error = "[J002] IR parse error: " + std::string(diag.getMessage());
        return result;
    }

    // Verify before handing the module to the JIT (phase25b): executing
    // verifier-invalid IR is undefined behavior. Hard error unless the
    // TML_NO_VERIFY_IR=1 bisection escape hatch is set (same policy as the
    // object-emission paths in llvm_backend.cpp).
    {
        std::string verifier_msg;
        llvm::raw_string_ostream verifier_os(verifier_msg);
        if (llvm::verifyModule(*mod, &verifier_os)) {
            const char* demote = std::getenv("TML_NO_VERIFY_IR");
            if (demote == nullptr || demote[0] != '1' || demote[1] != '\0') {
                result.success = false;
                result.error = "[K002] Module verification failed (jit): " + verifier_os.str() +
                               "\nThe generated LLVM IR parsed but violates LLVM's structural "
                               "rules — a TML codegen bug. TML_NO_VERIFY_IR=1 demotes this to "
                               "a warning for bisection.";
                return result;
            }
            fprintf(stderr,
                    "[K002] Module verification failed (jit, demoted by TML_NO_VERIFY_IR=1): %s\n",
                    verifier_os.str().c_str());
        }
    }

    // Override the module's data layout to match the JIT's host target.
    // TML generates a simplified data layout that may differ from LLVM's canonical
    // host layout (e.g., missing p270/p271/p272 address spaces, f80 alignment).
    // This is safe because both layouts are ABI-compatible for the types TML uses.
    mod->setDataLayout(impl_->jit->getDataLayout());

    // ThreadSafeModule takes ownership of both the module and the context.
    llvm::orc::ThreadSafeModule tsm(std::move(mod), std::move(ctx));

    if (auto err = impl_->jit->addIRModule(std::move(tsm))) {
        result.success = false;
        result.error = "[J003] Failed to add IR module: " + error_to_string(std::move(err));
        return result;
    }

    result.success = true;
    return result;
}

/// Read a precompiled object file and load it into the main JITDylib.
auto JitEngine::addObjectFile(const std::string& path) -> JitResult {
    JitResult result;

    auto buf_or_err = llvm::MemoryBuffer::getFile(path);
    if (!buf_or_err) {
        result.success = false;
        result.error =
            "[J008] Failed to read object file '" + path + "': " + buf_or_err.getError().message();
        return result;
    }

    if (auto err =
            impl_->jit->addObjectFile(impl_->jit->getMainJITDylib(), std::move(*buf_or_err))) {
        result.success = false;
        result.error =
            "[J009] Failed to add object file '" + path + "': " + error_to_string(std::move(err));
        return result;
    }

    result.success = true;
    return result;
}

/// Look up a symbol in the main JITDylib by IR name.
///
/// The returned `exit_code` field carries the raw function pointer address
/// cast to int (only the lower bits are meaningful — callers should cast
/// via `toPtr<T>()` or use `executeMain()` for `main`).
auto JitEngine::lookup(const std::string& symbol_name) -> JitResult {
    JitResult result;

    auto sym_or_err = impl_->jit->lookup(symbol_name);
    if (!sym_or_err) {
        result.success = false;
        result.error = "[J004] Symbol lookup failed for '" + symbol_name +
                       "': " + error_to_string(sym_or_err.takeError());
        return result;
    }

    // Expose address as exit_code for callers that just want to verify presence.
    result.success = true;
    result.exit_code = static_cast<int>(sym_or_err->getValue() & 0x7FFFFFFF);
    return result;
}

/// Execute the `main` function loaded into the JIT.
auto JitEngine::executeMain(const std::vector<std::string>& args) -> JitResult {
    JitResult result;

    auto sym_or_err = impl_->jit->lookup("main");
    if (!sym_or_err) {
        result.success = false;
        result.error =
            "[J005] Cannot find 'main' symbol: " + error_to_string(sym_or_err.takeError());
        return result;
    }

    // Build argv: always include a program name as argv[0].
    std::vector<std::string> argv_storage;
    argv_storage.push_back("jit");
    for (const auto& arg : args) {
        argv_storage.push_back(arg);
    }

    std::vector<const char*> argv_ptrs;
    argv_ptrs.reserve(argv_storage.size() + 1);
    for (const auto& s : argv_storage) {
        argv_ptrs.push_back(s.c_str());
    }
    argv_ptrs.push_back(nullptr);

    int argc = static_cast<int>(argv_storage.size());

    // Cast the symbol address to the standard main signature.
    using MainFn = int (*)(int, char**);
    auto* main_fn = sym_or_err->toPtr<MainFn>();

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    int exit_code = main_fn(argc, const_cast<char**>(argv_ptrs.data()));

    result.success = true;
    result.exit_code = exit_code;
    return result;
}

/// Self-test: JIT a minimal module returning 42 and verify the exit code.
auto JitEngine::selfTest() -> bool {
    // --- Test 1: pure IR (no external calls) returns 42 ---
    {
        constexpr const char* ir = R"ir(
define i32 @main(i32 %argc, i8** %argv) {
entry:
  ret i32 42
}
)ir";
        auto [cr, engine] = JitEngine::create();
        if (!cr.success)
            return false;
        auto ar = engine->addModule(ir);
        if (!ar.success)
            return false;
        auto er = engine->executeMain({});
        if (!er.success || er.exit_code != 42)
            return false;
    }

    // --- Test 2: invalid IR returns error, no crash ---
    {
        auto [cr, engine] = JitEngine::create();
        if (!cr.success)
            return false;
        auto ar = engine->addModule("this is not valid IR $$$$");
        if (ar.success || ar.error.empty())
            return false;
    }

    // --- Test 3: call print() from C runtime ---
    {
        constexpr const char* ir = R"ir(
@.str = private constant [6 x i8] c"hello\00"
declare void @print(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @print(ptr @.str)
  ret i32 0
}
)ir";
        auto [cr, engine] = JitEngine::create();
        if (!cr.success)
            return false;
        auto ar = engine->addModule(ir);
        if (!ar.success)
            return false;
        auto er = engine->executeMain({});
        if (!er.success || er.exit_code != 0)
            return false;
    }

    // --- Test 4: allocate and free memory via tml_alloc/tml_free ---
    {
        constexpr const char* ir = R"ir(
declare ptr @tml_alloc(i64)
declare void @tml_free(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  %ptr = call ptr @tml_alloc(i64 64)
  call void @tml_free(ptr %ptr)
  ret i32 0
}
)ir";
        auto [cr, engine] = JitEngine::create();
        if (!cr.success)
            return false;
        auto ar = engine->addModule(ir);
        if (!ar.success)
            return false;
        auto er = engine->executeMain({});
        if (!er.success || er.exit_code != 0)
            return false;
    }

    // --- Test 5: pointer read/write (lowlevel equivalent) ---
    {
        constexpr const char* ir = R"ir(
declare ptr @tml_alloc(i64)
declare void @tml_free(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  %ptr = call ptr @tml_alloc(i64 8)
  store i32 42, ptr %ptr
  %val = load i32, ptr %ptr
  call void @tml_free(ptr %ptr)
  ret i32 %val
}
)ir";
        auto [cr, engine] = JitEngine::create();
        if (!cr.success)
            return false;
        auto ar = engine->addModule(ir);
        if (!ar.success)
            return false;
        auto er = engine->executeMain({});
        if (!er.success || er.exit_code != 42)
            return false;
    }

    return true;
}

} // namespace tml::backend

#endif // TML_HAS_JIT
