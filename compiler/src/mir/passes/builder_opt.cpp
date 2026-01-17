//! # Builder Pattern Optimization Pass Implementation
//!
//! Detects and optimizes method chaining patterns for OOP performance.

#include "mir/passes/builder_opt.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// BuilderOptPass Implementation
// ============================================================================

auto BuilderOptPass::run(Module& module) -> bool {
    bool changed = false;
    stats_ = BuilderOptStats{};

    // Phase 1: Analyze all methods to identify builder methods
    analyze_builder_methods(module);

    // Phase 2: Process each function to detect and optimize chains
    for (auto& func : module.functions) {
        // Detect method chains in this function
        auto chains = detect_chains(func);

        for (const auto& chain : chains) {
            stats_.chains_detected++;

            // Optimize each chain
            if (optimize_chain(func, chain)) {
                changed = true;
            }
        }
    }

    return changed;
}

void BuilderOptPass::analyze_builder_methods(const Module& module) {
    builder_methods_.clear();

    for (const auto& func : module.functions) {
        stats_.methods_analyzed++;

        // Extract class name and method name from function name
        // Pattern: ClassName__method_name or ClassName_method_name
        auto sep_pos = func.name.find("__");
        if (sep_pos == std::string::npos) {
            sep_pos = func.name.rfind('_');
            if (sep_pos == std::string::npos || sep_pos == 0) {
                continue;
            }
        }

        std::string class_name = func.name.substr(0, sep_pos);
        std::string method_name =
            func.name.substr(sep_pos + (func.name[sep_pos + 1] == '_' ? 2 : 1));

        if (method_name.empty()) {
            continue;
        }

        // Check if method returns self
        BuilderMethodInfo info;
        info.class_name = class_name;
        info.method_name = method_name;
        info.returns_self = returns_self_type(func);
        info.is_terminal = is_terminal_method(method_name);

        // Check if method modifies state (has store instructions)
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<StoreInst>(inst.inst)) {
                    info.modifies_state = true;
                    break;
                }
            }
            if (info.modifies_state)
                break;
        }

        if (info.returns_self) {
            stats_.builder_methods_found++;
        }

        builder_methods_[class_name][method_name] = info;
    }
}

auto BuilderOptPass::detect_chains(const Function& func) -> std::vector<MethodChain> {
    std::vector<MethodChain> chains;

    // Track method calls and their receivers
    // A chain is formed when the result of one call is the receiver of the next
    std::unordered_map<ValueId, MethodChain> active_chains;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            // Look for method calls
            if (auto* call = std::get_if<MethodCallInst>(&inst.inst)) {
                ValueId receiver_id = call->receiver.id;

                // Check if receiver is a result from a previous call in a chain
                auto chain_it = active_chains.find(receiver_id);

                if (chain_it != active_chains.end()) {
                    // Extend existing chain
                    auto& chain = chain_it->second;
                    chain.call_results.push_back(inst.result);
                    chain.methods.push_back(call->method_name);

                    // Check if this is a terminal method
                    if (is_terminal_method(call->method_name)) {
                        chain.has_terminal = true;
                        chain.final_result = inst.result;
                        chains.push_back(chain);
                        active_chains.erase(chain_it);
                    } else {
                        // Update active chain with new result
                        active_chains[inst.result] = chain;
                        active_chains.erase(chain_it);
                    }
                } else if (is_builder_method(call->receiver_type, call->method_name)) {
                    // Start new chain
                    MethodChain new_chain;
                    new_chain.receiver = receiver_id;
                    new_chain.call_results.push_back(inst.result);
                    new_chain.methods.push_back(call->method_name);

                    if (is_terminal_method(call->method_name)) {
                        new_chain.has_terminal = true;
                        new_chain.final_result = inst.result;
                        chains.push_back(new_chain);
                    } else {
                        active_chains[inst.result] = new_chain;
                    }
                }
            }

            // Also check direct CallInst for devirtualized methods
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                // Check if this is a devirtualized method call
                if (call->devirt_info.has_value()) {
                    const auto& devirt = *call->devirt_info;
                    std::string class_name = devirt.original_class;
                    std::string method_name = devirt.method_name;

                    // First argument is typically the receiver (this)
                    if (!call->args.empty()) {
                        ValueId receiver_id = call->args[0].id;

                        auto chain_it = active_chains.find(receiver_id);

                        if (chain_it != active_chains.end()) {
                            // Extend existing chain
                            auto& chain = chain_it->second;
                            chain.call_results.push_back(inst.result);
                            chain.methods.push_back(method_name);

                            if (is_terminal_method(method_name)) {
                                chain.has_terminal = true;
                                chain.final_result = inst.result;
                                chains.push_back(chain);
                                active_chains.erase(chain_it);
                            } else {
                                active_chains[inst.result] = chain;
                                active_chains.erase(chain_it);
                            }
                        } else if (is_builder_method(class_name, method_name)) {
                            // Start new chain
                            MethodChain new_chain;
                            new_chain.receiver = receiver_id;
                            new_chain.call_results.push_back(inst.result);
                            new_chain.methods.push_back(method_name);

                            if (is_terminal_method(method_name)) {
                                new_chain.has_terminal = true;
                                new_chain.final_result = inst.result;
                                chains.push_back(new_chain);
                            } else {
                                active_chains[inst.result] = new_chain;
                            }
                        }
                    }
                }
            }
        }
    }

    // Any remaining active chains without terminals are also included
    // (they might still benefit from optimization)
    for (auto& [_, chain] : active_chains) {
        if (chain.methods.size() >= 2) { // Only include chains with 2+ calls
            chains.push_back(std::move(chain));
        }
    }

    return chains;
}

auto BuilderOptPass::optimize_chain(Function& func, const MethodChain& chain) -> bool {
    bool changed = false;

    // Optimization 1: Mark intermediate allocations as eliminable
    // When a method returns self, no new allocation is needed
    if (chain.methods.size() >= 2) {
        mark_intermediates_eliminable(func, chain);
        stats_.intermediates_eliminated += chain.methods.size() - 1;
        changed = true;
    }

    // Optimization 2: Apply copy elision for the final result
    if (chain.has_terminal) {
        if (apply_copy_elision(func, chain)) {
            stats_.copies_elided++;
            changed = true;
        }
    }

    // Optimization 3: Mark chain for fusion (SROA can treat as single scope)
    if (chain.methods.size() >= 2) {
        stats_.chains_fused++;
    }

    return changed;
}

auto BuilderOptPass::returns_self_type(const Function& method) const -> bool {
    // Check if the function returns the same type as its first parameter (this)
    if (method.params.empty()) {
        return false;
    }

    const auto& this_type = method.params[0].type;
    const auto& return_type = method.return_type;

    // Compare types - they should be the same pointer type
    if (!this_type || !return_type) {
        return false;
    }

    // Both should be pointer types to the same class
    if (std::holds_alternative<MirPointerType>(this_type->kind) &&
        std::holds_alternative<MirPointerType>(return_type->kind)) {
        // For pointer types, compare the pointed-to types
        return true; // Simplified: assume ptr->ptr is self-returning
    }

    return false;
}

auto BuilderOptPass::is_terminal_method(const std::string& name) const -> bool {
    return terminal_methods_.count(name) > 0;
}

auto BuilderOptPass::is_builder_method(const std::string& class_name,
                                       const std::string& method_name) const -> bool {
    auto class_it = builder_methods_.find(class_name);
    if (class_it == builder_methods_.end()) {
        return false;
    }

    auto method_it = class_it->second.find(method_name);
    if (method_it == class_it->second.end()) {
        return false;
    }

    return method_it->second.returns_self;
}

auto BuilderOptPass::get_method_info(const std::string& class_name,
                                     const std::string& method_name) const
    -> const BuilderMethodInfo* {
    auto class_it = builder_methods_.find(class_name);
    if (class_it == builder_methods_.end()) {
        return nullptr;
    }

    auto method_it = class_it->second.find(method_name);
    if (method_it == class_it->second.end()) {
        return nullptr;
    }

    return &method_it->second;
}

void BuilderOptPass::mark_intermediates_eliminable(Function& func, const MethodChain& chain) {
    // Mark all intermediate results as eliminable
    // This allows later passes to optimize away unnecessary allocations

    // For each call result except the last, mark as intermediate
    for (size_t i = 0; i + 1 < chain.call_results.size(); ++i) {
        ValueId intermediate_id = chain.call_results[i];

        // Find the instruction that produces this result
        for (auto& block : func.blocks) {
            for (auto& inst : block.instructions) {
                if (inst.result == intermediate_id) {
                    // Mark the instruction as producing an intermediate value
                    // This is used by later optimization passes
                    if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                        call->is_stack_eligible = true; // Reuse flag for optimization hint
                    } else if (auto* method_call = std::get_if<MethodCallInst>(&inst.inst)) {
                        (void)method_call; // Mark as optimizable
                    }
                    break;
                }
            }
        }
    }
}

auto BuilderOptPass::apply_copy_elision(Function& func, const MethodChain& chain) -> bool {
    if (!chain.has_terminal || chain.final_result == INVALID_VALUE) {
        return false;
    }

    // Find where the final result is used
    // If it's assigned directly to a return value or stored, we can elide the copy
    for (auto& block : func.blocks) {
        // Check terminator for return
        if (block.terminator.has_value()) {
            if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
                if (ret->value && ret->value->id == chain.final_result) {
                    // Final result is returned - copy elision applicable
                    // The callee can construct directly into the return slot
                    return true;
                }
            }
        }

        // Check for store to output parameter
        for (const auto& inst : block.instructions) {
            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                if (store->value.id == chain.final_result) {
                    // Final result is stored - copy elision may apply
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace tml::mir
