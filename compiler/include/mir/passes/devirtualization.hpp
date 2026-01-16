//! # Devirtualization Optimization Pass
//!
//! Converts virtual method calls to direct (non-virtual) calls when the
//! receiver type is known precisely. This eliminates vtable lookup overhead
//! and enables further optimizations like inlining.
//!
//! ## Optimization Strategies
//!
//! ### 1. Sealed Class Devirtualization
//!
//! If the receiver type is a sealed class, the method cannot be overridden
//! in subclasses, so the call can always be devirtualized:
//!
//! ```tml
//! sealed class FinalWidget { func render(this) { ... } }
//! let w: FinalWidget = FinalWidget::new()
//! w.render()  // Can be direct call
//! ```
//!
//! ### 2. Exact Type Devirtualization
//!
//! If we can prove the receiver's exact runtime type (e.g., right after
//! construction), the call can be devirtualized:
//!
//! ```tml
//! let dog: Dog = Dog::new()
//! dog.speak()  // We know it's exactly Dog, not a subclass
//! ```
//!
//! ### 3. Single Implementation
//!
//! If only one class in the hierarchy implements a virtual method,
//! the call can be devirtualized (even if the base type is used):
//!
//! ```tml
//! abstract class Shape { abstract func area(this) -> I32 }
//! class Circle extends Shape { override func area(this) -> I32 { ... } }
//! // If Circle is the only Shape subclass, Shape.area() → Circle.area()
//! ```
//!
//! ## Statistics
//!
//! The pass tracks how many calls were devirtualized by each strategy.

#pragma once

#include "mir/mir_pass.hpp"
#include "types/env.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Reason why a call was devirtualized.
enum class DevirtReason {
    SealedClass,         ///< Receiver is a sealed class.
    ExactType,           ///< Receiver type is known exactly (e.g., after new).
    SingleImpl,          ///< Only one implementation exists.
    FinalMethod,         ///< Method is marked as final (cannot be overridden).
    NoOverride,          ///< Method is not virtual/overridable.
    NotDevirtualized,    ///< Could not devirtualize.
};

/// Statistics collected during devirtualization.
struct DevirtualizationStats {
    size_t method_calls_analyzed = 0;   ///< Total method calls examined.
    size_t devirtualized_sealed = 0;    ///< Devirtualized due to sealed class.
    size_t devirtualized_exact = 0;     ///< Devirtualized due to exact type.
    size_t devirtualized_single = 0;    ///< Devirtualized due to single impl.
    size_t devirtualized_final = 0;     ///< Devirtualized due to final method.
    size_t devirtualized_nonvirtual = 0;///< Already non-virtual (no vtable).
    size_t not_devirtualized = 0;       ///< Could not devirtualize.

    /// Total devirtualized calls.
    [[nodiscard]] auto total_devirtualized() const -> size_t {
        return devirtualized_sealed + devirtualized_exact +
               devirtualized_single + devirtualized_final + devirtualized_nonvirtual;
    }

    /// Devirtualization rate (0.0 to 1.0).
    [[nodiscard]] auto devirt_rate() const -> double {
        if (method_calls_analyzed == 0) return 0.0;
        return static_cast<double>(total_devirtualized()) /
               static_cast<double>(method_calls_analyzed);
    }
};

/// Class hierarchy analysis result for a class.
struct ClassHierarchyInfo {
    std::string name;                              ///< Class name.
    std::optional<std::string> base_class;         ///< Direct parent class.
    std::vector<std::string> interfaces;           ///< Implemented interfaces.
    std::unordered_set<std::string> subclasses;    ///< Direct subclasses.
    std::unordered_set<std::string> all_subclasses;///< Transitive subclasses.
    std::unordered_set<std::string> final_methods; ///< Methods marked as final.
    bool is_sealed;                                ///< True if sealed.
    bool is_abstract;                              ///< True if abstract.

    /// Returns true if this class has no subclasses (leaf in hierarchy).
    [[nodiscard]] auto is_leaf() const -> bool {
        return subclasses.empty();
    }

    /// Returns true if calls to this type can be devirtualized.
    [[nodiscard]] auto can_devirtualize() const -> bool {
        return is_sealed || is_leaf();
    }

    /// Returns true if a method is final in this class.
    [[nodiscard]] auto is_method_final(const std::string& method_name) const -> bool {
        return final_methods.count(method_name) > 0;
    }
};

/// Devirtualization optimization pass.
///
/// Analyzes method calls and converts virtual dispatch to direct calls
/// when the receiver type is known precisely.
class DevirtualizationPass : public MirPass {
public:
    /// Creates a devirtualization pass.
    /// @param env Type environment for class hierarchy queries.
    explicit DevirtualizationPass(types::TypeEnv& env) : env_(env) {}

    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "Devirtualization";
    }

    /// Runs devirtualization on the entire module.
    auto run(Module& module) -> bool override;

    /// Returns devirtualization statistics.
    [[nodiscard]] auto get_stats() const -> DevirtualizationStats {
        return stats_;
    }

    /// Queries the class hierarchy info for a type.
    [[nodiscard]] auto get_class_info(const std::string& class_name) const
        -> const ClassHierarchyInfo*;

    /// Checks if a method call can be devirtualized.
    [[nodiscard]] auto can_devirtualize(const std::string& receiver_type,
                                        const std::string& method_name) const
        -> DevirtReason;

private:
    types::TypeEnv& env_;
    DevirtualizationStats stats_;

    // Class hierarchy analysis cache (mutable for lazy initialization in const methods)
    mutable std::unordered_map<std::string, ClassHierarchyInfo> class_hierarchy_;
    mutable bool hierarchy_built_ = false;

    /// Builds the class hierarchy from the type environment.
    /// Const-qualified because it only mutates mutable cache members.
    void build_class_hierarchy() const;

    /// Computes transitive subclasses for all classes.
    void compute_transitive_subclasses() const;

    /// Processes a function for devirtualization opportunities.
    auto process_function(Function& func) -> bool;

    /// Processes a basic block for devirtualization.
    auto process_block(BasicBlock& block) -> bool;

    /// Tries to devirtualize a method call.
    /// Returns the direct function name if successful, empty string otherwise.
    [[nodiscard]] auto try_devirtualize(const MethodCallInst& call) const
        -> std::pair<std::string, DevirtReason>;

    /// Checks if a class is sealed (cannot be subclassed).
    [[nodiscard]] auto is_sealed_class(const std::string& class_name) const -> bool;

    /// Checks if a method is virtual in the given class.
    [[nodiscard]] auto is_virtual_method(const std::string& class_name,
                                         const std::string& method_name) const -> bool;

    /// Checks if a method is final in the given class or any ancestor.
    /// Final methods cannot be overridden in subclasses.
    [[nodiscard]] auto is_final_method(const std::string& class_name,
                                       const std::string& method_name) const -> bool;

    /// Gets the implementing class for a method (for single-impl detection).
    [[nodiscard]] auto get_single_implementation(const std::string& class_name,
                                                 const std::string& method_name) const
        -> std::optional<std::string>;
};

} // namespace tml::mir
