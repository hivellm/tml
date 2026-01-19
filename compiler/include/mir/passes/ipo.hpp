//! # Interprocedural Optimization (IPO) Pass
//!
//! This pass performs interprocedural optimizations that analyze and transform
//! code across function boundaries.
//!
//! ## Optimizations
//!
//! 1. **Interprocedural Constant Propagation (IPCP)**:
//!    Propagate constant arguments to function parameters.
//!
//!    ```tml
//!    func foo(x: I32) -> I32 { return x + 1 }
//!    func main() {
//!        let a = foo(5)  // IPCP: specializes foo(5) = 6
//!    }
//!    ```
//!
//! 2. **Argument Promotion**:
//!    Convert reference parameters to value parameters for small types.
//!
//!    ```tml
//!    // Before: ref parameter requires indirection
//!    func double(x: ref I32) -> I32 { return *x * 2 }
//!
//!    // After: value parameter is more efficient
//!    func double(x: I32) -> I32 { return x * 2 }
//!    ```
//!
//! 3. **Function Attribute Inference**:
//!    Infer function attributes like `@pure` and `@nothrow`.
//!
//!    ```tml
//!    // Inferred @pure: no side effects, only depends on args
//!    func add(a: I32, b: I32) -> I32 { return a + b }
//!
//!    // Inferred @nothrow: never panics or throws
//!    func clamp(x: I32, min: I32, max: I32) -> I32 {
//!        if x < min { return min }
//!        if x > max { return max }
//!        return x
//!    }
//!    ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Information about a function's constant arguments
struct ConstantArgInfo {
    std::string func_name;
    size_t arg_index;
    Constant value;
    int64_t call_count = 0; ///< How many times called with this constant
};

/// Inferred function attributes
struct FunctionAttributes {
    bool is_pure = false;         ///< No side effects, result depends only on args
    bool is_nothrow = false;      ///< Never throws or panics
    bool is_readonly = false;     ///< Only reads memory, no writes
    bool is_norecurse = false;    ///< Doesn't call itself directly or indirectly
    bool is_willreturn = false;   ///< Always returns (no infinite loops)
    bool is_speculatable = false; ///< Safe to execute speculatively
};

/// Statistics for IPO
struct IpoStats {
    size_t constants_propagated = 0;
    size_t args_promoted = 0;
    size_t pure_functions_found = 0;
    size_t nothrow_functions_found = 0;
    size_t readonly_functions_found = 0;

    void reset() {
        *this = IpoStats{};
    }
};

/// Interprocedural Optimization Pass
class IpoPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "IPO";
    }

    auto run(Module& module) -> bool override;

    /// Get statistics
    [[nodiscard]] auto stats() const -> const IpoStats& {
        return stats_;
    }

    /// Get inferred attributes for a function
    [[nodiscard]] auto get_attributes(const std::string& func_name) const
        -> const FunctionAttributes*;

private:
    IpoStats stats_;

    /// Map from function name to inferred attributes
    std::unordered_map<std::string, FunctionAttributes> function_attrs_;

    /// Map from function name to constant argument patterns
    std::unordered_map<std::string, std::vector<ConstantArgInfo>> constant_args_;

    /// Set of functions that have been analyzed
    std::unordered_set<std::string> analyzed_functions_;

    // ============ Analysis Methods ============

    /// Build call graph and gather constant argument info
    void analyze_calls(const Module& module);

    /// Analyze a single function for attribute inference
    auto analyze_function_attributes(const Function& func) -> FunctionAttributes;

    /// Check if a function is pure (no side effects)
    auto is_function_pure(const Function& func) const -> bool;

    /// Check if a function never throws/panics
    auto is_function_nothrow(const Function& func) const -> bool;

    /// Check if a function only reads memory
    auto is_function_readonly(const Function& func) const -> bool;

    /// Check if a function always returns
    auto is_function_willreturn(const Function& func) const -> bool;

    // ============ Transformation Methods ============

    /// Apply interprocedural constant propagation
    auto apply_ipcp(Module& module) -> bool;

    /// Apply argument promotion (ref -> value for small types)
    auto apply_argument_promotion(Module& module) -> bool;

    /// Promote a specific argument from ref to value
    auto promote_argument(Function& func, size_t arg_index) -> bool;

    /// Check if an argument can be promoted (small type, not escaped)
    auto can_promote_argument(const Function& func, size_t arg_index) const -> bool;
};

/// Interprocedural Constant Propagation Pass
///
/// Propagates constant values across function boundaries.
class IpcpPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "IPCP";
    }

    auto run(Module& module) -> bool override;

private:
    /// Map from (func_name, arg_index) to known constant value
    std::unordered_map<std::string, std::unordered_map<size_t, Constant>> constant_args_;

    /// Gather constant arguments from all call sites
    void gather_constants(const Module& module);

    /// Specialize a function with known constant arguments
    auto specialize_function(Function& func, size_t arg_index, const Constant& value) -> bool;
};

/// Argument Promotion Pass
///
/// Promotes reference arguments to value arguments for small types.
class ArgPromotionPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ArgPromotion";
    }

    auto run(Module& module) -> bool override;

    /// Size threshold for promotion (bytes)
    void set_size_threshold(size_t bytes) {
        size_threshold_ = bytes;
    }

private:
    size_t size_threshold_ = 16; ///< Promote refs to values for types <= this size

    /// Check if a parameter should be promoted
    auto should_promote(const FunctionParam& param) const -> bool;

    /// Promote a parameter from ref to value
    auto promote_param(Function& func, size_t param_index) -> bool;

    /// Update all call sites to pass by value instead of ref
    void update_call_sites(Module& module, const std::string& func_name, size_t param_index);
};

/// Function Attribute Inference Pass
///
/// Infers attributes like @pure, @nothrow for functions.
class AttrInferencePass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "AttrInference";
    }

    auto run(Module& module) -> bool override;

    /// Get inferred attributes for a function
    [[nodiscard]] auto get_attributes(const std::string& func_name) const
        -> const FunctionAttributes*;

private:
    std::unordered_map<std::string, FunctionAttributes> attrs_;

    /// Analyze a function's attributes
    auto analyze_function(const Function& func) -> FunctionAttributes;

    /// Check if instruction has side effects
    auto has_side_effects(const Instruction& inst) const -> bool;

    /// Check if instruction can throw
    auto can_throw(const Instruction& inst) const -> bool;

    /// Check if instruction reads memory
    auto reads_memory(const Instruction& inst) const -> bool;

    /// Check if instruction writes memory
    auto writes_memory(const Instruction& inst) const -> bool;
};

} // namespace tml::mir
