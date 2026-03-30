//! # JIT Engine
//!
//! ORC LLJIT-based in-process execution engine for TML.
//!
//! Parses LLVM IR text, JIT-compiles it, and executes the result
//! without writing any files to disk.
//!
//! ## Usage
//!
//! ```cpp
//! auto result = JitEngine::create();
//! if (!result.success) { /* handle */ }
//! auto& engine = *result.engine;
//!
//! auto add_result = engine.addModule(ir_text);
//! if (!add_result.success) { /* handle */ }
//!
//! auto exec_result = engine.executeMain({});
//! // exec_result.exit_code == program return value
//! ```
//!
//! ## Requirements
//!
//! Only available when the compiler is built with TML_USE_JIT=ON.
//! All classes and functions are guarded by `#if TML_HAS_JIT`.

#pragma once

#include <memory>
#include <string>
#include <vector>

#if TML_HAS_JIT

namespace tml::backend {

/// Result type for JIT operations.
///
/// Returned by `create()`, `addModule()`, `lookup()`, and `executeMain()`.
struct JitResult {
    /// Whether the operation succeeded.
    bool success = false;

    /// Exit code from `executeMain()`. Only meaningful when success is true
    /// and the operation was `executeMain()`.
    int exit_code = 0;

    /// Human-readable error message when success is false.
    std::string error;
};

/// Opaque implementation details hidden from callers.
struct JitEngineImpl;

/// In-process ORC LLJIT execution engine.
///
/// Create with `JitEngine::create()`. Once created, call `addModule()` to
/// load LLVM IR, then `executeMain()` to run it.
///
/// Non-copyable; move-only.
class JitEngine {
public:
    ~JitEngine();

    // Non-copyable, movable
    JitEngine(const JitEngine&) = delete;
    JitEngine& operator=(const JitEngine&) = delete;
    JitEngine(JitEngine&&) noexcept;
    JitEngine& operator=(JitEngine&&) noexcept;

    /// Create a new JIT engine targeting the host machine.
    ///
    /// Initializes LLVM native targets (once, thread-safe) and creates an
    /// LLJIT instance configured for the host triple.
    ///
    /// @return JitResult with success=true and engine populated, or
    ///         success=false with error describing the failure.
    [[nodiscard]] static auto create() -> std::pair<JitResult, std::unique_ptr<JitEngine>>;

    /// Add an LLVM IR module to the JIT.
    ///
    /// Parses `ir_text` as LLVM IR, wraps it in a ThreadSafeModule, and
    /// adds it to the main JITDylib. Can be called multiple times to load
    /// multiple modules.
    ///
    /// @param ir_text LLVM IR text (bitcode is not supported)
    /// @return JitResult::success=true on success, false on parse/add failure
    [[nodiscard]] auto addModule(const std::string& ir_text) -> JitResult;

    /// Look up a symbol by IR name and return its address.
    ///
    /// Symbol must have been added via `addModule()` and compiled successfully.
    ///
    /// @param symbol_name IR-level symbol name (e.g., "main", "my_func")
    /// @return JitResult::success=true with exit_code containing the raw
    ///         function pointer cast to uintptr_t, or success=false on lookup failure
    [[nodiscard]] auto lookup(const std::string& symbol_name) -> JitResult;

    /// Execute the `main` function loaded into the JIT.
    ///
    /// Looks up the symbol `main`, casts it to `int(*)(int, char**)`,
    /// calls it with argc=0 and argv=nullptr, and captures the return value.
    ///
    /// @param args Command-line arguments to pass (argv[0] is set to "jit")
    /// @return JitResult with exit_code set to the return value of main()
    [[nodiscard]] auto executeMain(const std::vector<std::string>& args) -> JitResult;

    /// Self-test: JIT a minimal IR module that returns 42 and verify the result.
    ///
    /// Creates a new engine, adds a module with `define i32 @main(...)` that
    /// returns 42, executes it, and checks the exit code.
    ///
    /// @return true if the self-test passed (exit_code == 42)
    [[nodiscard]] static auto selfTest() -> bool;

private:
    explicit JitEngine(std::unique_ptr<JitEngineImpl> impl);

    std::unique_ptr<JitEngineImpl> impl_;
};

} // namespace tml::backend

#endif // TML_HAS_JIT
